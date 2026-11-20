#!/usr/bin/env python3
"""The open world's memory map, as a page: docs/memmap.html.

    tools/mkmemmap.py [OUT]

Where every byte of the C64 goes in each mode of the open world (the
splash, the bootstrap building the lands, owparty, the engine in each of
its three areas, owtown's screens of text and a fight), read from what the
build left: the linker maps (build/*.map), the files on the disk that are
data rather than code (the splash pictures, OW-DATA, the theme, the fields),
the game block's size (include/q4k/owgame.h) and the engine's sprite count
(src/lib/ow_data.s). The page itself is tools/memmap.html, which draws the
table from the blocks this writes into it.

What the page says lives where is this file's knowledge (the blocks below);
the addresses are the build's. The make rule runs it after any of those
inputs changes. The page is stamped with the date and the commit it was
built from (and whether the tree had changes beyond it), and is rewritten
only when something but the stamp changed, so that a relink that moves
nothing leaves the file, and its stamp, as they were.
"""

import datetime
import glob
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "build")
DIST = os.path.join(ROOT, "dist")
TEMPLATE = os.path.join(ROOT, "tools", "memmap.html")
OUT = os.path.join(ROOT, "docs", "memmap.html")
MARK = "/*DATA*/null"


def die(msg):
    sys.exit("mkmemmap: " + msg)


def read_map(name):
    """A map's segment list: name -> (start, end)."""
    path = os.path.join(BUILD, name + ".map")
    try:
        text = open(path).read()
    except OSError:
        die("no %s: build first" % os.path.relpath(path, ROOT))
    segs = {}
    part = text.split("Segment list:", 1)[1]
    for line in part.splitlines():
        f = line.split()
        if len(f) == 5 and re.fullmatch(r"[0-9A-F]{6}", f[1]):
            start, end = int(f[1], 16), int(f[2], 16)
            if int(f[3], 16):
                segs[f[0]] = (start, end)
        elif segs and line.startswith("Exports list"):
            break
    return segs


def span(segs, lo, hi, skip=()):
    """The first and last byte of the segments that start in [lo, hi]."""
    got = [v for k, v in segs.items()
           if lo <= v[0] <= hi and k not in skip]
    if not got:
        die("nothing between $%04X and $%04X" % (lo, hi))
    return min(s for s, _ in got), max(e for _, e in got)


def seg(segs, name):
    if name not in segs:
        die("no segment %s" % name)
    return segs[name]


def prg_end(path):
    """The last address a PRG file loads to."""
    data = open(path, "rb").read()
    return data[0] | data[1] << 8, (data[0] | data[1] << 8) + len(data) - 3


def rle_len(d, o):
    """Bytes a .v2i stream at o decodes to ($00, n: n zeros; $00, $00 ends)."""
    n = 0
    while True:
        b = d[o]
        o += 1
        if b:
            n += 1
        elif d[o]:
            n += d[o]
            o += 1
        else:
            return n


def sid_range(path):
    d = open(path, "rb").read()
    off = int.from_bytes(d[6:8], "big")
    load = int.from_bytes(d[8:10], "big")
    body = d[off:]
    if load == 0:
        load, body = body[0] | body[1] << 8, body[2:]
    return load, load + len(body) - 1


def define(path, name):
    m = re.search(r"#define\s+%s\s+(0x[0-9A-Fa-f]+|\d+)" % name,
                  open(path).read())
    if not m:
        die("no %s in %s" % (name, path))
    return int(m.group(1), 0)


def git(*args):
    try:
        return subprocess.run(["git", "-C", ROOT] + list(args),
                              capture_output=True, text=True,
                              check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def stamp(out):
    """The commit the build came from, and whether the tree had more."""
    full = git("rev-parse", "HEAD")
    rel = os.path.relpath(out, ROOT)
    dirty = [l for l in git("status", "--porcelain").splitlines()
             if l[3:] != rel]
    return {"date": datetime.date.today().isoformat(),
            "commit": full, "short": full[:7],
            "branch": git("rev-parse", "--abbrev-ref", "HEAD"),
            "subject": git("log", "-1", "--format=%s"),
            "dirty": bool(dirty)}


def blocks():
    boot = read_map("openworld")
    party = read_map("owparty")
    cpu = read_map("ow-cpu")
    reu = read_map("ow-reu")
    town = read_map("owtown")
    talk = read_map("owtalk")
    fight = read_map("fight")

    B = lambda r, key, cat, label: [r[0], r[1], key, cat, label]

    # owtalk's, the same with a portrait or without; and the largest
    # portrait (tools/mkportrait.py's files): its file, and its sprites.
    talk_lo = [
        B(span(talk, 0x0801, 0x37FF), "talklow", "town",
          "owtalk: main, talk, gift, the portrait's talk, the C library"),
        [0x37C0, 0x37FF, "cstack", "town", "C stack (top of MAIN, grows down)"],
        [0x3800, 0x3FFF, "charset", "video",
         "Text charset, with read-only data in its spare glyphs"]]
    talk_high = span(talk, 0x4000, 0x7FFF)
    talk_top = [
        B(seg(talk, "BSS"), "talkbss", "tbss", "owtalk BSS"),
        B(seg(talk, "TALKBSS"), "talkbuf2", "tbss",
          "Talk buffer (a person's lines)")]
    pic_end, pic_spr = 0x8000, 0x4400
    pdir = os.path.join(ROOT, "build", "portraits")
    for f in sorted(os.listdir(pdir)) if os.path.isdir(pdir) else ():
        data = open(os.path.join(pdir, f), "rb").read()
        pic_end = max(pic_end, prg_end(os.path.join(pdir, f))[1])
        d = data[2:]
        o = 26 + sum(d[8 + 2 * i] | d[9 + 2 * i] << 8 for i in range(3))
        n = rle_len(d, o)
        pic_spr = max(pic_spr, 0x4400 + n - 1)

    g_addr = define(os.path.join(ROOT, "include/q4k/owgame.h"), "OWG_ADDR")
    g_size = define(os.path.join(ROOT, "include/q4k/owgame.h"), "OWG_SIZE")
    m = re.search(r"ow_nsprites:\s*\.byte\s+(\d+)",
                  open(os.path.join(ROOT, "src/lib/ow_data.s")).read())
    nspr = int(m.group(1))
    tune = sid_range(os.path.join(DIST, "theme.sid"))
    owdata = prg_end(os.path.join(DIST, "ow-data.prg"))
    splash = max(prg_end(p)[1]
                 for p in glob.glob(os.path.join(DIST, "splash-?.prg")))
    fields = glob.glob(os.path.join(BUILD, "fight", "disk", "f.*"))
    field = max(fields, key=os.path.getsize)
    packed = prg_end(field)
    disk = seg(boot, "OWDISK")

    low = [
        [0x0000, 0x0001, "cpuport", "sys", "CPU port ($01: ROM banking)"],
        [0x0002, 0x001B, "zp", "sys", "cc65 zero page (each program's own)"],
        [0x001C, 0x00FF, "kzp", "sys", "KERNAL and BASIC zero page"],
        [0x0100, 0x01FF, "stack", "sys",
         "6502 stack (its bottom half: the REU cache's part of the loader, "
         "during a load; a continent's build thread, in an engine)"],
        [0x0200, 0x02A6, "kwork", "sys", "KERNAL work area, keyboard buffer"],
        [0x02A7, 0x02EC, "owblk", "keep",
         "OW hand-off block: seed, lands, ships, REU"],
        [0x02ED, 0x02EF, "owb", "keep", "OWB: which screen to open"],
        [0x0300, 0x0333, "vectors", "sys", "KERNAL and BASIC vectors"],
        [0x0800, 0x0800, "basic0", "sys", "BASIC's zero byte"],
    ]
    loader = [
        [0x0334, 0x037F, "loader", "keep",
         "ow_run loader, copied in for each load"],
        [0x0380, 0x03BF, "disksprite", "keep", "Disk icon sprite"],
        [0x03C0, 0x03FF, "diskirq", "keep", "Disk icon interrupt hook"],
    ]
    textscr = [0x0400, 0x07FF, "textscr", "video", "Text screen"]
    top = [
        [g_addr, g_addr + g_size - 1, "game", "keep",
         "Game block: party, purse, quest, where, each member's colour "
         "(%d bytes)" % g_size],
        [0xCF00, 0xCF13, "probe", "sys", "Probe page (tests)"],
        B(disk, "diskcode", "keep", "Disk icon code"),
        [0xD000, 0xDFFF, "vault", "keep",
         "Vault: home, 64 by 64, under the I/O area"],
        [0xE7C0, 0xE7FF, "tunehdr", "keep", "Tune header"],
        B(tune, "tune", "keep", "Theme (SID file)"),
        [tune[1] + 1, 0xFFF9, "tunearea", "keep",
         "Reserved for a bigger tune"],
        [0xFFFA, 0xFFFF, "cpuvec", "sys", "CPU vectors"],
    ]
    od = B(owdata, "owdata", "data", "OW-DATA: town maps, shared tables, constants")

    def engine(segs):
        core = span(segs, 0x0801, 0x3FFF, skip=("MAPCODE",))
        return [
            [0x0334, 0x03FF, "estack", "ebss",
             "C stack (sm_stack), over the loader and the icon, idle while "
             "the engine runs"],
            B(seg(segs, "OWLO"), "owlo", "engine",
              "OWLO: stlow.s and the text window's odd jobs, in the KERNAL's text screen"),
            B(core, "ecore", "engine",
              "Engine core: main loop, scrolling, music"),
            B(seg(segs, "MAPCODE"), "egen", "engine",
              "isle_rnd: the generators' random bytes (they are in the overlays)"),
            [0x4000, 0x43FF, "scra", "video", "Screen A"],
            [0x4400, 0x47FF, "scrb", "video", "Screen B"],
            [0x4800, 0x4FFF, "mcchars", "video", "Multicolour characters"],
            [0x5000, 0x5000 + 64 * nspr - 1, "msprites", "video",
             "Sprite blocks (%d): the three classes, the villager, the ship, "
             "two frames each" % nspr],
            B(seg(segs, "OWTOP"), "owtop", "engine", "OWTOP code (from .hi)"),
            B(seg(segs, "HICODE"), "hicode", "engine",
              "HIGH code (from .hi)"),
            B(seg(segs, "HIDATA"), "local", "ebss", "local[]: the place's map"),
            od,
            [0x7F00, 0x7FFF, "corners", "video",
             "The viewport's rounded corners: four sprite blocks (sm_init)"],
            B(seg(segs, "OWCBSS"), "owcbss", "ebss",
              "REU load cache's scratch, the people's multiplexer, past OW-DATA"),
            [0xA000, 0xAFFF, "shore", "ebss",
             "SHORE: edges of each square (a tile set's file, t.*, on the way in)"],
            [0xB000, 0xBFFF, "world", "ebss", "WORLD: the 64 by 64 window"],
            B(seg(segs, "ALIGNBSS"), "chcol", "ebss",
              "ow_chcol (page aligned)"),
            B(seg(segs, "BSS"), "ebss", "ebss", "Engine BSS"),
            [0xE000, 0xE7BF, "kept", "keep",
             "Continent kept, under the KERNAL"],
        ]

    def overlay(segs, head, code):
        return seg(segs, head)[0], seg(segs, code)[1]

    eng = engine(cpu)
    town_low = span(town, 0x0801, 0x37FF, skip=("OWFIGS",))
    town_high = span(town, 0x4000, 0xAFFF)
    town_lo = [
        B(town_low, "townlow", "town",
          "owtown low: main, inn rumours, fight caller, C library"),
        B(seg(town, "OWFIGS"), "owfigs", "video",
          "Party screen's figures: sprite blocks (OWFIGS)"),
        [0x37C0, 0x37FF, "cstack", "town", "C stack (top of MAIN, grows down)"],
    ]
    town_bss = [
        B(seg(town, "BSS"), "townbss", "tbss", "owtown BSS"),
        B(seg(town, "TALKBSS"), "talkbuf", "tbss",
          "Talk buffer (a person's lines)"),
    ]
    fmod = span(fight, 0x5800, 0xAFFF, skip=("BSS",))

    modes = [
        {"id": "splash", "name": "Splash",
         "sub": "openworld showing the picture; KERNAL out", "blocks": low + [
             [0x0400, 0x07FF, "textscr", "video", "Text screen (black)"],
             B(span(boot, 0x0801, 0x3FFF), "boot", "boot",
               "Bootstrap: code, land generator"),
             [0x4000, 0x40FF, "v2t", "splash", "Splash text (.v2t)"],
             [0x4100, splash, "v2i", "splash",
              "Splash picture file (.v2i, the larger of PAL's and NTSC's)"],
             B(seg(boot, "BSS"), "bootbss", "boot", "Bootstrap BSS"),
             B(seg(boot, "HIDATA"), "bootlocal", "boot", "local[] (bootstrap)"),
             [0xC000, 0xFFFF, "pic", "splash",
              "Picture unpacked: bitmap, screen, colours (over the block, "
              "the vault and the music)"]]},
        {"id": "boot", "name": "Building",
         "sub": "openworld after F1 or F3: lands, music, ships",
         "blocks": low + loader + [
             textscr,
             B(span(boot, 0x0801, 0x3FFF), "boot", "boot",
               "Bootstrap: code, land generator"),
             [0x4000, owdata[0] - 1, "v2stale", "stale",
              "Splash file, no longer read"],
             od,
             B(seg(boot, "BSS"), "bootbss", "boot", "Bootstrap BSS"),
             B(seg(boot, "HIDATA"), "bootlocal", "boot",
               "local[]: each continent built here once"),
         ] + top},
        {"id": "party", "name": "Party",
         "sub": "owparty, after F1: the story and the party",
         "blocks": low + loader + [
             textscr,
             B(span(party, 0x0801, 0x37FF), "owparty", "party",
               "owparty: questions, party, title glyphs"),
             [0x37C0, 0x37FF, "cstack", "party",
              "C stack (top of MAIN, grows down)"],
             [0x3800, 0x3FFF, "charset", "video",
              "Text charset (charset_install)"],
             B(seg(party, "BSS"), "partybss", "party", "owparty BSS"),
             [od[0], od[1], "owdata", "data",
              "OW-DATA, untouched (still loaded again on the way out)"],
         ] + top},
        {"id": "world", "name": "Overworld", "sub": "ow-cpu + world overlay",
         "blocks": low + eng + [
             B(overlay(cpu, "OVWHEAD", "OVWCODE"), "ovw", "ovw",
               "World overlay: window, streaming, ships")] + top},
        {"id": "town", "name": "Town",
         "sub": "ow-cpu + town overlay (towns, the castle)",
         "blocks": low + eng + [
             B(overlay(cpu, "OVTHEAD", "OVTCODE"), "ovt", "ovt",
               "Town overlay: roofs, people and their bob, gates, the maps"),
             B(seg(cpu, "OVTBSS"), "ovtbss", "ovt",
               "Town overlay's scratch: the buildings' regions")] + top},
        {"id": "dungeon", "name": "Dungeon", "sub": "ow-cpu + dungeon overlay",
         "blocks": low + eng + [
             B(overlay(cpu, "OVDHEAD", "OVDCODE"), "ovd", "ovd",
               "Dungeon overlay and its generator")] + top},
        {"id": "owtown", "name": "Text screens",
         "sub": "owtown: shop, inn, party (I), help (?)",
         "blocks": low + loader + [textscr] + town_lo + [
             [0x3800, 0x3FFF, "charset", "video",
              "Text charset, with read-only data in its spare glyphs"],
             B(town_high, "townhigh", "town",
               "owtown high: shops, inn, arms, bestiary, party screen, "
               "charset_install"),
             [town_high[1] + 1, od[1], "owdatastale", "stale",
              "OW-DATA, partly overwritten (reloaded on the way back)"],
         ] + town_bss + top},
        {"id": "owtalk", "name": "Talk",
         "sub": "owtalk: a conversation with someone with no portrait",
         "blocks": low + loader + [textscr] + talk_lo + [
             B(talk_high, "talkhigh", "town",
               "owtalk high: the font image and charset_install (used once), "
               "actor's and town_data's tables, the portrait's code "
               "(copied up)"),
             [od[0], od[1], "owdatastale", "stale",
              "OW-DATA, as the engine left it (reloaded on the way back)"],
         ] + talk_top + top},
        {"id": "owtalkpic", "name": "Portrait",
         "sub": "owtalk: a conversation with a portrait (src/ow/talkpic.c)",
         "blocks": low + loader + [textscr] + talk_lo + [
             [0x4000, 0x43E7, "picscreen", "video",
              "Portrait: screen (cell colours), the words' too"],
             [0x43F8, 0x43FF, "picptrs", "video", "Portrait: sprite pointers"],
             [0x4400, pic_spr, "picspr", "video",
              "Portrait: sprite blocks (the largest portrait's)"],
             [0x6000, 0x7F3F, "picbitmap", "video",
              "Bitmap: the portrait, and the talk's words drawn by v2t"],
             [0x8000, pic_end, "picfile", "town",
              "The portrait's file, its kernel run in place (the largest)"],
         ] + talk_top + [
             B(seg(talk, "PICCODE"), "piccode", "town",
               "The portrait's code: v2i.inc, v2t.inc, keys"),
         ] + top},
        {"id": "fight", "name": "Fight",
         "sub": "owtown + the fight module over its high half",
         "blocks": low + loader + [
             [0x0400, 0x07FF, "textscr", "video",
              "Text screen (the result is said here)"]] + town_lo + [
             [0x3800, 0x3FFF, "charset", "video",
              "Text charset, kept for the result"],
             [0x4000, 0x43FF, "fscreen", "video", "Field: screen"],
             [0x4400, 0x47FF, "ffacts", "video", "Field: facts, colours, flags"],
             [0x4800, 0x4BFF, "fchars", "video", "Field: characters"],
             [0x4C00, 0x4FFF, "fletters", "video", "Letters (from ROM)"],
             [0x5000, 0x53FF, "fmon", "video", "Monster's sprites (s.*)"],
             B(seg(fight, "FIGHTSPR"), "fparty", "fight",
               "Party's sprites (two frames a class), shots, cursor, arrow"),
             B(seg(fight, "FIGHTCNR"), "fcorners", "fight",
               "The rounded corners: four sprite blocks (corners.s)"),
             B(fmod, "fmodule", "fight",
               "Fight module: jump, block, code, data"),
             B(seg(fight, "BSS"), "fbss", "fbss", "Fight module BSS"),
             [town_bss[0][0], town_bss[0][1], "townbss", "tbss", "owtown BSS"],
             [town_bss[1][0], town_bss[1][1], "talkbuf", "tbss",
              "Talk buffer (idle)"],
             [0xC000, packed[1], "packed", "fbss",
              "Packed field file, as loaded (the largest, %s)"
              % os.path.basename(field)],
         ] + top},
    ]
    for m in modes:
        check(m)

    reu_note = {"cpu_main": span(cpu, 0x0801, 0x3FFF)[1],
                "reu_main": span(reu, 0x0801, 0x3FFF)[1],
                "cpu_hi": seg(cpu, "HICODE")[1],
                "reu_hi": seg(reu, "HICODE")[1],
                "cpu_bss": seg(cpu, "BSS")[1], "reu_bss": seg(reu, "BSS")[1],
                "tune": list(tune)}
    return modes, reu_note


def check(mode):
    """No two blocks of a mode may overlap: if they do, the build moved
    something this file places by hand."""
    b = sorted(mode["blocks"], key=lambda x: x[0])
    for x, y in zip(b, b[1:]):
        if y[0] <= x[1]:
            die("%s: %s ($%04X-$%04X) overlaps %s ($%04X-$%04X)"
                % (mode["name"], x[4], x[0], x[1], y[4], y[0], y[1]))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else OUT
    modes, notes = blocks()
    data = {"modes": modes, "notes": notes}
    page = open(TEMPLATE).read()
    if MARK not in page:
        die("%s has no %s" % (TEMPLATE, MARK))

    def render(st):
        d = dict(data, stamp=st)
        return page.replace(MARK, json.dumps(d, separators=(",", ":")))

    st = stamp(out)
    new = render(st)
    try:
        old = open(out).read()
    except OSError:
        old = None
    if old is not None and '"stamp":' in old:
        at = old.index('"stamp":') + len('"stamp":')
        was = json.JSONDecoder().raw_decode(old, at)[0]
        if render(was) == old:
            os.utime(out)           # nothing moved: keep the old stamp
            print("mkmemmap: %s unchanged" % os.path.relpath(out, ROOT))
            return
    os.makedirs(os.path.dirname(out), exist_ok=True)
    open(out, "w").write(new)
    print("mkmemmap: -> %s (%s%s)" % (os.path.relpath(out, ROOT), st["short"],
                                      ", with uncommitted changes"
                                      if st["dirty"] else ""))


if __name__ == "__main__":
    main()
