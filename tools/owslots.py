#!/usr/bin/env python3
"""After an open-world engine links: check its overlays' calls.

    tools/owslots.py MAP OBJ...

The engine (src/ow/engine.h) is a core and three overlays, the world's
(world-*.o), the town's (town-*.o) and the dungeon's (dungeon-*.o, and
ow_dungeon.o, the generator's copy), that take turns in one slot. Code in
an overlay is only there while that overlay is loaded, so the core may reach
it only through the jump table at the start of the slot (ovl.s, the only
module allowed to name both), and an overlay may not reach the other's. This
reads each object's imports with od65 and fails if anything else uses a
symbol an overlay defines in its code (its data, in BSS, stays put and may be
shared). Then it says how full the slot is.
"""

import os
import re
import subprocess
import sys

SEGS = {"world": ("OVWHEAD", "OVWCODE"), "town": ("OVTHEAD", "OVTCODE"),
        "dungeon": ("OVDHEAD", "OVDCODE", "OVDART")}


def od65(obj, what):
    text = subprocess.run(["od65", "--dump-" + what, obj], capture_output=True,
                          text=True, check=True).stdout
    return set(re.findall(r'Name:\s+"([^"]+)"', text))


def part_of(obj):
    base = os.path.basename(obj)
    if base in ("ow_dungeon.o", "foe_art.o", "foes.o"):
        return "dungeon"
    for p in SEGS:
        if base.startswith(p + "-"):
            return p
    return "core"


def main():
    mapfile, objs = sys.argv[1], sys.argv[2:]
    text = open(mapfile).read()
    segs = {}
    for m in re.finditer(r"^(\w+)\s+([0-9A-F]{6})\s+([0-9A-F]{6})\s+([0-9A-F]{6})",
                         text.split("Segment list:")[1], re.M):
        segs[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
    exports = {}
    for m in re.finditer(r"(\w+)\s+([0-9A-F]{6}) R\w*", text.split("Exports list by name:")[1]):
        exports[m.group(1)] = int(m.group(2), 16)

    def in_code(sym, part):
        a = exports.get(sym)
        return a is not None and any(
            s in segs and segs[s][0] <= a <= segs[s][1] for s in SEGS[part])

    code = {}           # symbol -> the overlay whose code defines it
    for obj in objs:
        p = part_of(obj)
        if p != "core":
            for sym in od65(obj, "exports"):
                if in_code(sym, p):
                    code[sym] = p

    bad = []
    for obj in objs:
        base = os.path.basename(obj)
        if base == "ovl.o":
            continue
        p = part_of(obj)
        for sym in od65(obj, "imports"):
            owner = code.get(sym)
            if owner and owner != p:
                bad.append("%s (%s) calls %s, in the %s overlay" % (
                    base, p, sym.lstrip("_"), owner))
    if bad:
        print("owslots: reaches into an overlay that may not be loaded:")
        for b in sorted(bad):
            print("   ", b)
        return 1

    sizes = []
    for p, (head, *body) in SEGS.items():
        lo = segs[head][0]
        hi = max(segs[s][1] for s in body if s in segs)
        sizes.append("%s %d" % (p, hi - lo + 1))
    print("owslots: slot at $%04X: %s" % (segs["OVWHEAD"][0], ", ".join(sizes)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
