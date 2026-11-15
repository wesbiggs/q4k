#!/usr/bin/env python3
"""After the offline build links: check the engines, and make their files.

    tools/mkengines.py BUILD_DIR PRG

ld65 writes each engine beside the PRG (cfg/c64-q4k.cfg) as PRG.map,
PRG.fight and so on. This

  1. checks that nothing calls into an engine that may not be in the slot: an
     engine may use the core and itself, and the core and other engines may
     use only an engine's *_entry (through engine_call()). A call that broke
     that would jump into whatever the slot held at the time. Two exceptions,
     listed in ALLOWED, are boot.c running the title screen, which the PRG
     brings in the slot;
  2. copies the engines to the names the game loads them by (e.map, ...);
  3. says how big each engine is, and what is left.

(The music is not the engines' business: tools/mksid.py makes its files.)

Exits non-zero if the check fails. See include/q4k/engine.h and ENGINES.md.
"""

import os
import re
import subprocess
import sys

ENGINES = {"TITLE": None, "MAP": "map", "FIGHT": "fight", "TALK": "talk",
           "SHOP": "shop"}
ALLOWED = {("boot", "_title_screen"), ("boot", "_tiles_install")}
SLOT = 0x4000


def engine_of_segment(name):
    for e in ENGINES:
        if name.startswith(e):
            return e
    return None


def parse_map(path):
    """Each module's fragments as (segment, start, size), and segments' ends."""
    text = open(path).read()
    mods_text = text.split("Modules list:")[1].split("Segment list:")[0]
    segs_text = text.split("Segment list:")[1].split("Exports list")[0]
    seg_start, seg_end = {}, {}
    for m in re.finditer(r"^(\w+)\s+([0-9A-F]{6})\s+([0-9A-F]{6})\s+([0-9A-F]{6})",
                         segs_text, re.M):
        seg_start[m.group(1)] = int(m.group(2), 16)
        seg_end[m.group(1)] = int(m.group(3), 16)
    frags = {}
    current = None
    for line in mods_text.splitlines():
        if line and not line.startswith(" "):
            name = line.strip().rstrip(":")
            m = re.match(r".*\((.*)\.o\)$", name)
            lib = name.split("(")[0] if m else ""
            if m and lib.endswith("q4k.lib"):
                current = m.group(1)
            elif m:
                current = None          # cc65's runtime: the core's
            else:
                current = os.path.basename(name)[:-2]
            continue
        m = re.match(r"\s+(\w+)\s+Offs=([0-9A-F]+)\s+Size=([0-9A-F]+)", line)
        if m and current:
            seg = m.group(1)
            frags.setdefault(current, []).append(
                (seg, seg_start[seg] + int(m.group(2), 16), int(m.group(3), 16)))
    return frags, seg_start, seg_end


def labels(path):
    out = {}
    for line in open(path):
        p = line.split()
        if len(p) == 3 and p[0] == "al":
            out[p[2].lstrip(".")] = int(p[1], 16)
    return out


def od65(obj, what):
    text = subprocess.run(["od65", "--dump-" + what, obj], capture_output=True,
                          text=True, check=True).stdout
    return set(re.findall(r'Name:\s+"([^"]+)"', text))


def main():
    build, prg = sys.argv[1], sys.argv[2]
    frags, seg_start, seg_end = parse_map(os.path.join(build, "q4k.map"))
    addr = labels(os.path.join(build, "q4k.lbl"))

    objs = {m: os.path.join(build, "lib", m + ".o") for m in frags}
    objs["q4k"] = os.path.join(build, "q4k.o")

    # Which engine each exported symbol lives in: the engine of the fragment
    # of its own module that holds its address.
    owner_engine, exporter = {}, {}
    for mod, obj in objs.items():
        for sym in od65(obj, "exports"):
            exporter[sym] = mod
            a = addr.get(sym)
            eng = None
            if a is not None:
                for seg, start, size in frags.get(mod, []):
                    if size and start <= a < start + size:
                        eng = engine_of_segment(seg)
                        break
            owner_engine[sym] = eng

    bad = []
    for mod, obj in objs.items():
        mine = {engine_of_segment(seg) for seg, _, size in frags.get(mod, [])
                if size} - {None}
        for sym in od65(obj, "imports"):
            eng = owner_engine.get(sym)
            if eng is None or eng in mine or sym.endswith("_entry") \
                    or (mod, sym) in ALLOWED:
                continue
            bad.append("%s uses %s, in the %s engine" % (
                mod + (" (" + "/".join(sorted(mine)) + ")" if mine else " (core)"),
                sym.lstrip("_"), eng.lower()))

    if bad:
        print("mkengines: reaches into an engine that may not be loaded:")
        for b in sorted(bad):
            print("   ", b)
        return 1

    # The engines, under the names engine.c loads them by.
    dist = os.path.dirname(prg)
    sizes = {}
    for eng, suffix in ENGINES.items():
        if not suffix:
            continue
        src = prg + "." + suffix
        data = open(src, "rb").read()
        sizes[eng] = len(data) - 2
        with open(os.path.join(dist, "e." + suffix), "wb") as f:
            f.write(data)
        os.remove(src)

    # How full things are.
    slot = seg_end.get("TITLEDATA", SLOT) - SLOT + 1
    sizes["TITLE"] = slot
    for e, bss in (("FIGHT", "FIGHTBSS"), ("TALK", "TALKBSS")):
        if bss in seg_end:
            sizes[e] = seg_end[bss] - SLOT + 1
    top = max(sizes.values())
    print("mkengines: slot $%04X, %d bytes at most:" % (SLOT, top),
          ", ".join("%s %d" % (e.lower(), n) for e, n in sizes.items()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
