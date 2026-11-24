#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build what the multicolour arena's fight loads (src/fight/).

    tools/mkfields.py                 the sprites that are part of the fight:
                                      src/fight/fight_data.s
    tools/mkfields.py DIR             ... and every file of the disk, in DIR:
                                      f.<field> and s.<monster>
    tools/mkfields.py --preview PNG   a picture of all of it, to look at

A field (assets/fields.txt) is drawn here, not on the C64: its squares are
the smooth-scrolling prototype's metatiles (tools/mkoverworld.py reads the
set, and its rules for the edges between terrains are applied here as
src/ow/smooth.s applies them), and what comes out is the finished screen,
the characters it uses, their colours and what can be walked on, laid out
so that one LOAD at $4000 puts all of it where the VIC and the fight read
it (include/q4k/fight.h has the addresses):

    $4000  the screen: 18 rows of field, a row of solid black, 6 of spaces
    $4400  the field's facts: FI_* in fight.h
    $4800  its characters, 128 at most (character 0 is the solid one); the
           letters go after them, at $4C00, from the ROM

A monster's sprites (assets/monsters.txt) are a file too, 64 bytes a frame,
loaded at $5000: two frames, or eight for a body of four sprites (top left,
top right, bottom left, bottom right, twice).
"""

import os
import sys

import figures
import mkoverworld as ow

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIELDS = os.path.join(ROOT, "assets", "fields.txt")
SPRITES = os.path.join(ROOT, "assets", "monsters.txt")
TILES = os.path.join(ROOT, "assets", "tiles.txt")
OUT_S = os.path.join(ROOT, "src", "fight", "fight_data.s")
OUT_FOES = os.path.join(ROOT, "src", "ow", "foe_art.s")

# The monsters the open world's dungeons show on the map (src/ow/dungeon.c's
# A_*, in this order): two frames each, in the dungeon's overlay.
FOE_ART = ["rat", "goblin", "wolf", "wasp", "bandit", "skeleton", "ogre",
           "wraith", "drake", "keeper"]
FOE_FRAMES = 1          # src/ow/dungeon.c's: the slot has room for one

FW, FH = 20, 9
SCREEN, INFO, CHARS, MONSPR = 0x4000, 0x4400, 0x4800, 0x5000
MAXCHARS = 128
FONT_SPACE = 0xA0           # the ROM's space, where the fight puts the letters
# The facts, from INFO: fight.h's FI_*.
FI_PAL, FI_HX, FI_HY, FI_MX, FI_MY, FI_ROCK, FI_NCHARS = 0, 3, 4, 5, 6, 7, 11
FI_NSCAT, FI_SCAT, SCAT_MAX = 12, 13, 3
FI_COLOUR, FI_FLAGS = 32, 160
DEEP_CH = '"'            # the deep sea, which assets/tiles.txt has no glyph for
FLAG = {"pass": 0, "enter": 0, "water": 1, "block": 2}
# What a step onto a square costs, past the 10 tenths of grass: bits 2-3 of
# its flag, a level of 5 tenths each (fight.h's FF_COST).
SLOW = {"shore": 1, "swamp": 2}     # (not the forest: it is no slower)
SLOW_LEVEL = {15: 1, 20: 2}

# The sprites linked into the fight, in the order of fight.h's SPR_*; each
# class has two frames. The arrow is one block, which the fight fills when
# one flies from fight_arrow[], eight pictures by DIR_* (down, up, left,
# right, down-left, down-right, up-left, up-right) made from three drawn
# ones: up (arrow), right (arrow_r) and up-right (arrow_ur), mirrored.
STATIC = figures.CLASSES
SHOTS = ["fire", "wave", "bolt", "cursor", "hit"]
ARROWS = ["arrow", "arrow_r", "arrow_ur"]


def die(msg):
    sys.exit("mkfields: " + msg)


def tile_table():
    """Map character -> (name, code, flag), from assets/tiles.txt."""
    out = {}
    for line in open(TILES):
        f = line.split()
        if len(f) >= 5 and f[0] == "tile":
            out[f[2]] = (f[1], int(f[3], 16), f[4])
    return out


def read_fields():
    fields = []
    lines = open(FIELDS).read().split("\n")
    i = 0
    while i < len(lines):
        f = lines[i].split("#")[0].split()
        i += 1
        if not f:
            continue
        if f[0] != "field" or len(f) < 4:
            die("%s:%d: what is %r?" % (FIELDS, i, " ".join(f)))
        rows = lines[i:i + FH]
        if len(rows) != FH or any(len(r) != FW for r in rows):
            die("%s:%d: %s: want %d rows of %d" % (FIELDS, i, f[1], FH, FW))
        i += FH
        # scatter=<overlay>[:<cost>],...: what the fight may put down
        # besides boulders: a block, or ground that costs 15 or 20 a step.
        scat = []
        for w in f:
            if w.startswith("scatter="):
                for item in w[8:].split(","):
                    n, _, c = item.partition(":")
                    if c and int(c) not in SLOW_LEVEL:
                        die("%s: %s: a cost of 15 or 20" % (f[1], n))
                    scat.append((n, int(c) if c else 0))
        if len(scat) > SCAT_MAX:
            die("%s: %d kinds of scatter at most" % (f[1], SCAT_MAX))
        fields.append({"name": f[1], "set": f[2], "ground": f[3],
                       "size": 2 if "size" in f else 1,
                       "shut": "shut" in f, "sea": "sea" in f,
                       "rows": rows, "scatter": scat})
    return fields


_sets = {}


def tile_set(name):
    if name not in _sets:
        ow.SET_FILE[name] = name + ".txt"
        _sets[name] = ow.build(name)
    return _sets[name]


def art_glyph(rows):
    return tuple(sum(ow.PIX[ch] << (6 - 2 * k) for k, ch in enumerate(r))
                 for r in rows)


def compose(fd, tiles):
    """One field: its file's bytes (without the load address), and the
    pieces a preview wants."""
    st = tile_set(fd["set"])
    codes = st["codes"]
    name = fd["name"]
    if fd["ground"] not in st["by_name"]:
        die("%s: no %r in %s" % (name, fd["ground"], fd["set"]))

    # The squares: a tile name each, and what stands where.
    grid, rocks = [], []
    hx = hy = mx = my = None
    for y, r in enumerate(fd["rows"]):
        row = []
        for x, ch in enumerate(r):
            if ch == "@":
                hx, hy = x, y
                row.append(fd["ground"])
            elif ch == "m":
                mx, my = x, y
                row.append("deep" if fd["sea"] else fd["ground"])
            elif ch == DEEP_CH:
                row.append("deep")
            elif ch == "o":
                rocks.append((x, y))
                row.append(fd["ground"])
            elif ch in tiles and tiles[ch][0] in st["by_name"]:
                row.append(tiles[ch][0])
            else:
                die("%s: row %d: no tile for %r in %s"
                    % (name, y + 1, ch, fd["set"]))
        grid.append(row)
    if hx is None or mx is None:
        die("%s: wants an @ and an m" % name)

    slot = lambda n: (codes[n] - ow.TILE_BASE) & 63
    # Beyond the field is more of whatever is at its edge: no edge there.
    at = lambda x, y: grid[min(max(y, 0), FH - 1)][min(max(x, 0), FW - 1)]

    local = {}                  # (glyph, colour) -> character
    glyphs = [(tuple([0xFF] * 8), 0)]   # 0: solid, and black

    def char(key):
        if key not in local:
            local[key] = len(glyphs)
            glyphs.append(key)
        return local[key]

    screen = [[0] * (FW * 2) for _ in range(FH * 2)]
    for y in range(FH):
        for x in range(FW):
            n = grid[y][x]
            s = slot(n)
            sel, inv = st["bsel"][s], st["binv"][s]
            mask = 0
            if sel:
                # North, then clockwise: mkoverworld's N..NW.
                for bit, (dx, dy) in enumerate(((0, -1), (1, -1), (1, 0),
                                                (1, 1), (0, 1), (-1, 1),
                                                (-1, 0), (-1, -1))):
                    if st["nflag"][slot(at(x + dx, y + dy))] & sel:
                        mask |= 1 << bit
                mask ^= inv
            # Which of a tile's two drawings: a hash of the square.
            v = (x * 7 + y * 13 + (x * y) % 5 + (x ^ y)) & 1
            m = (st["mt1"] if v else st["mt0"])[s]
            for q in range(4):
                e = ow.qvar(q, mask)
                c = st["blend"][m * 16 + q * 4 + e - 1] if e else st["quads"][m][q]
                g, col = st["charlist"][c]
                screen[y * 2 + (q >> 1)][x * 2 + (q & 1)] = char((g, 8 | col))

    # The boulder, on this field's ground: its - pixels are the ground's.
    # A character has one colour of its own, so either the boulder has its
    # colour and the ground's own-colour pixels under it go -- or, where the
    # ground is mostly that colour (sand), which would leave a square hole
    # in it, the boulder's lit side takes the ground's colour instead. The
    # rest of what the fight scatters is drawn the same way.
    ground = st["art"][st["by_name"][fd["ground"]][0]]
    gcols = st["cols"][st["by_name"][fd["ground"]][0]]
    mostly = sum(r.count("c") for r in ground) * 4 > 16 * 8

    def overlay(oname):
        if oname not in st["overlays"]:
            die("%s: %s has no %s drawn `on` anything"
                % (name, fd["set"], oname))
        ocols, orows_ = st["overlays"][oname]
        out = []
        for q in range(4):
            col = gcols[q] if mostly else ocols[q]
            rows = ["".join((g if g != "c" or col == gcols[q] else ".")
                            if o == "-" else o
                            for o, g in zip(orow[(q & 1) * 4:(q & 1) * 4 + 4],
                                            grow[(q & 1) * 4:(q & 1) * 4 + 4]))
                    for orow, grow in zip(orows_[(q >> 1) * 8:(q >> 1) * 8 + 8],
                                          ground[(q >> 1) * 8:(q >> 1) * 8 + 8])]
            rows = ow.grounded(rows, q, st["ground"])
            out.append(char((art_glyph(rows), 8 | col)))
        return out

    rock = overlay("boulder")
    scat = [(overlay(n), FLAG["block"] if not c else SLOW_LEVEL[c] << 2)
            for n, c in fd["scatter"]]
    for x, y in rocks:
        for q in range(4):
            screen[y * 2 + (q >> 1)][x * 2 + (q & 1)] = rock[q]

    if len(glyphs) > MAXCHARS:
        die("%s: %d characters, %d at most" % (name, len(glyphs), MAXCHARS))

    kind = {t[0]: t[2] for t in tiles.values()}
    kind["deep"] = "water"              # (the overworld set's own code)
    flags = [[FLAG.get(kind.get(n), 2) | SLOW.get(n, 0) << 2
              for n in row] for row in grid]
    for x, y in rocks:
        flags[y][x] = 2

    check(fd, flags, hx, hy, mx, my)

    blob = bytearray(0x0C00)
    for y in range(FH * 2):
        blob[y * 40:y * 40 + 40] = bytes(screen[y])
    for i in range(19 * 40, 1000):
        blob[i] = FONT_SPACE
    info = INFO - SCREEN
    blob[info + FI_PAL:info + FI_PAL + 3] = bytes(st["palette"])
    blob[info + FI_HX:info + FI_HX + 4] = bytes([hx, hy, mx, my])
    blob[info + FI_ROCK:info + FI_ROCK + 4] = bytes(rock)
    blob[info + FI_NSCAT] = len(scat)
    for k, (chars, flag) in enumerate(scat):
        at_ = info + FI_SCAT + k * 5
        blob[at_:at_ + 5] = bytes(chars + [flag])
    blob[info + FI_NCHARS] = len(glyphs)
    for i, (g, col) in enumerate(glyphs):
        blob[info + FI_COLOUR + i] = col
        blob[CHARS - SCREEN + i * 8:CHARS - SCREEN + i * 8 + 8] = bytes(g)
    for y in range(FH):
        for x in range(FW):
            blob[info + FI_FLAGS + y * FW + x] = flags[y][x]
    return blob, {"screen": screen, "glyphs": glyphs,
                  "palette": st["palette"], "hx": hx, "hy": hy,
                  "mx": mx, "my": my}


def check(fd, flags, hx, hy, mx, my):
    """The way out is where the fight looks for it, and the monster can
    reach the party: its whole body, if it is two squares by two."""
    name, size = fd["name"], fd["size"]
    flags = [[f & 3 for f in row] for row in flags]    # the kind, not the cost
    mid = FH // 2
    for y in range(FH):
        for x in (0, FW - 1):
            if fd["shut"]:
                if y == mid and flags[y][x] == 0:
                    die("%s: shut, but row %d is open at the side"
                        % (name, mid + 1))
            elif (flags[y][x] == 0) != (y == mid):
                die("%s: the sides must be shut but for row %d, the way out"
                    % (name, mid + 1))

    if fd["sea"]:
        # At sea the monsters keep to the water and the party to the deck
        # (src/fight/fight.c's SEA_FIGHT): no way round to look for.
        if flags[my][mx] != FLAG["water"] or flags[hy][hx]:
            die("%s: the monster starts in the water, the party on deck"
                % name)
        return

    def fits(x, y):
        return all(0 <= x + i < FW and 0 <= y + j < FH
                   and flags[y + j][x + i] == 0
                   for i in range(size) for j in range(size))

    if not fits(mx, my) or flags[hy][hx]:
        die("%s: something stands on blocked ground" % name)
    seen, todo = {(mx, my)}, [(mx, my)]
    while todo:
        x, y = todo.pop()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            p = (x + dx, y + dy)
            if p not in seen and fits(*p):
                seen.add(p)
                todo.append(p)
    if not any(abs(x + i - hx) + abs(y + j - hy) == 1 for x, y in seen
               for i in range(size) for j in range(size)):
        die("%s: the monster cannot reach the party" % name)


# --- sprites -----------------------------------------------------------------

def read_sprites():
    """name -> (size, [frames]), a frame being 16 * size rows (with its
    bob, tools/figures.py, if drawn once)."""
    return figures.read(SPRITES)


block = figures.block


def blocks(size, frame):
    """A frame's sprites: one, or the four quarters of a big body."""
    if size == 1:
        return [block(frame)]
    return [block([r[qx * 8:qx * 8 + 8] for r in frame[qy * 16:qy * 16 + 16]])
            for qy in range(2) for qx in range(2)]


def static_blocks(sprites):
    out = []
    for n in STATIC:
        for fr in sprites[n][1]:
            out.append((n, block(figures.bare(fr))))
    out.append(("the arrow, filled from fight_arrow", bytes(64)))
    for n in SHOTS:
        rows = sprites[n][1][0]
        if n == "cursor":
            # A row down, for the fight puts it a line higher than whom it
            # marks: mux.s sorts it first, onto a lower-numbered sprite,
            # which the VIC draws in front.
            rows = ["." * 8] + rows
        out.append((n, block(rows)))
    return out


def skin_blocks(sprites):
    """SPR_SKIN: the party's skins, a frame each, which the fight copies to
    $5200, past the biggest monster's eight blocks ($5400-$57FF is full)."""
    return [(n + " skin", figures.skin(sprites[n][1][0], multicolour=True))
            for n in STATIC]


def arrows(sprites):
    """The eight arrows by DIR_*, as rows of strings."""
    up = sprites["arrow"][1][0]
    right = sprites["arrow_r"][1][0]
    ur = sprites["arrow_ur"][1][0]
    flip = lambda rows: [r[::-1] for r in rows]   # left for right
    return [("down", up[::-1]), ("up", up),
            ("left", flip(right)), ("right", right),
            ("down-left", flip(ur[::-1])), ("down-right", ur[::-1]),
            ("up-left", flip(ur)), ("up-right", ur)]


def monsters(sprites, order):
    skip = set(STATIC + SHOTS + ARROWS)
    return [n for n in order if n not in skip]


def write_static(sprites):
    out = ["; GENERATED from assets/monsters.txt by tools/mkfields.py -- do "
           "not edit.",
           "",
           "; The sprites that are part of the fight: the party's three "
           "classes, two",
           "; frames each, the shots, the cursor and a blow landing, 64 "
           "bytes a",
           "; block from $5400 (cfg/c64-fight.cfg; SPR_* in "
           "include/q4k/fight.h).",
           "",
           '        .segment "FIGHTSPR"',
           ""]
    for name, b in static_blocks(sprites):
        out.append("        .byte " + ",".join("$%02X" % x for x in b)
                   + "   ; " + name)
    out += ["",
            "; The party's skins (SPR_SKIN), which party_start() copies to "
            "$5200.",
            "",
            "        .export _fight_skins",
            '        .segment "RODATA"',
            "_fight_skins:"]
    for name, b in skin_blocks(sprites):
        out.append("        .byte " + ",".join("$%02X" % x for x in b)
                   + "   ; " + name)
    out += ["",
            "; The arrow's eight pictures, by DIR_*: two bytes for each of "
            "sixteen",
            "; rows (the third byte of a sprite's row is always 0).",
            "",
            "        .export _fight_arrow",
            '        .segment "RODATA"',
            "_fight_arrow:"]
    for name, rows in arrows(sprites):
        b = block(rows)
        two = [b[i] for i in range(48) if i % 3 != 2]
        out.append("        .byte " + ",".join("$%02X" % x for x in two)
                   + "   ; " + name)
    out.append("")
    open(OUT_S, "w").write("\n".join(out))


def write_foes(sprites):
    out = ["; GENERATED from assets/monsters.txt by tools/mkfields.py -- do "
           "not edit.",
           "",
           "; The monsters on a dungeon's map (src/ow/dungeon.c's A_*): "
           "FOE_FRAMES each,",
           "; 64 bytes a block, in the dungeon's overlay, which is in the "
           "engine's VIC",
           "; bank (cfg/c64-openworld.cfg's OVDART, aligned to a block).",
           "",
           "        .export _foe_art, _foe_blank",
           '        .segment "OVDART"',
           "_foe_art:"]
    for name in FOE_ART:
        size, frames = sprites[name]
        if size != 1:
            die("%s: a dungeon's monster is one square" % name)
        for fr in frames[:FOE_FRAMES]:
            out.append("        .byte " + ",".join("$%02X" % x
                                                   for x in block(fr))
                       + "   ; " + name)
    out.append("; A monster out of the party's sight (dungeon.c's fog): nothing.")
    out.append("_foe_blank:")
    out.append("        .res 64, 0")
    out.append("")
    out.append("; Sprites: the VIC sees bank 1, $4000-$7FFF, and the hero's")
    out.append("; skin's blocks are at $7E80, the corners' at $7F00 (smooth.s).")
    out.append("        .assert * <= $7E80, lderror, "
               "\"foe_art: over the skin's and the corners' sprites\"")
    out.append("")
    open(OUT_FOES, "w").write("\n".join(out))


# --- a picture ---------------------------------------------------------------

RGB = [(0, 0, 0), (255, 255, 255), (136, 57, 50), (103, 182, 189),
       (139, 63, 150), (85, 160, 73), (64, 49, 141), (191, 206, 114),
       (139, 84, 41), (87, 66, 0), (184, 105, 98), (80, 80, 80),
       (120, 120, 120), (148, 224, 137), (120, 105, 196), (159, 159, 159)]
MONCOL = {"rat": 8, "goblin": 3, "wolf": 15, "wasp": 7, "bandit": 10,
          "skeleton": 1, "ogre": 14, "wraith": 4, "drake": 2, "keeper": 10,
          "dragon": 2, "warrior": 6, "archer": 5, "wizard": 4, "fire": 8,
          "wave": 14, "bolt": 1, "cursor": 7, "hit": 1, "arrow": 7,
          "arrow_r": 7, "arrow_ur": 7}


def preview(path, fields, tiles, sprites, order):
    from PIL import Image
    img = Image.new("RGB", (320, len(fields) * 152 + 80), (0, 0, 0))
    px = img.load()

    def sprite(rows, x0, y0, own):
        for y, r in enumerate(rows):
            for x, ch in enumerate(r):
                if ch != ".":
                    c = RGB[{"k": 0, "s": 10, "t": own}[ch]]
                    px[x0 + x * 2, y0 + y] = px[x0 + x * 2 + 1, y0 + y] = c

    for n, fd in enumerate(fields):
        blob, p = compose(fd, tiles)
        pal = p["palette"]
        for cy in range(FH * 2):
            for cx in range(FW * 2):
                g, col = p["glyphs"][p["screen"][cy][cx]]
                for r in range(8):
                    for k in range(4):
                        v = (g[r] >> (6 - 2 * k)) & 3
                        c = RGB[[pal[0], pal[1], pal[2], col & 7][v]]
                        x, y = cx * 8 + k * 2, n * 152 + cy * 8 + r
                        px[x, y] = px[x + 1, y] = c
        for k, name in enumerate(monsters(sprites, order)):
            size, frames = sprites[name]
            if (size == 2) == (fd["size"] == 2):
                sprite(frames[0], 8 + (k % 10) * 32, n * 152 + 16
                       + (k // 10) * 32, MONCOL.get(name, 1))
        for k, name in enumerate(STATIC):
            sprite(sprites[name][1][0], (p["hx"] - 1 + k) * 16,
                   n * 152 + p["hy"] * 16, MONCOL[name])
    y0 = len(fields) * 152 + 8
    for k, name in enumerate(order):
        size, frames = sprites[name]
        for f in range(2):
            if size == 1:
                sprite(frames[f], 4 + (k % 9) * 34 + f * 16,
                       y0 + (k // 9) * 20, MONCOL.get(name, 1))
    img = img.resize((img.width * 2, img.height * 2), Image.NEAREST)
    img.save(path)


# A field is 3K at SCREEN, mostly runs of the same few bytes, and the open
# world's disk is full: the files are packed, PackBits-style (a byte n < 128
# and n + 1 bytes as they are; n >= 128 and the next byte n - 125 times),
# loaded to PACKED and unpacked by the module (src/fight/fight.c's
# unpack()), about a third of the size.
PACKED = 0xC000
FIELD_SIZE = 0x0C00


def pack(d):
    out = bytearray()
    lit = bytearray()

    def flush():
        nonlocal lit
        while lit:
            out.append(len(lit[:128]) - 1)
            out.extend(lit[:128])
            lit = lit[128:]

    i = 0
    while i < len(d):
        j = i
        while j < len(d) and d[j] == d[i] and j - i < 130:
            j += 1
        if j - i >= 3:
            flush()
            out += bytes([0x80 + j - i - 3, d[i]])
            i = j
        else:
            lit.append(d[i])
            i += 1
    flush()
    return bytes(out)


def unpack(p, size):
    out = bytearray()
    i = 0
    while len(out) < size:
        n = p[i]
        if n < 128:
            out += p[i + 1:i + 2 + n]
            i += 2 + n
        else:
            out += bytes([p[i + 1]]) * (n - 125)
            i += 2
    return bytes(out)


def main():
    tiles = tile_table()
    fields = read_fields()
    sprites, order = read_sprites()
    for n in STATIC + SHOTS + ARROWS:
        if n not in sprites or sprites[n][0] != 1:
            die("%s wants a sprite %r, one square" % (SPRITES, n))

    args = sys.argv[1:]
    if args and args[0] == "--preview":
        preview(args[1], fields, tiles, sprites, order)
        return
    write_static(sprites)
    write_foes(sprites)
    if not args:
        return
    out = args[0]
    os.makedirs(out, exist_ok=True)
    for old in os.listdir(out):
        if old[:2] in ("f.", "s."):
            os.remove(os.path.join(out, old))
    for fd in fields:
        blob, p = compose(fd, tiles)
        blob = bytes(blob).ljust(FIELD_SIZE, b"\0")
        if len(blob) != FIELD_SIZE:
            die("field %s is %d bytes, not %d" % (fd["name"], len(blob),
                                                  FIELD_SIZE))
        packed = pack(blob)
        assert unpack(packed, FIELD_SIZE) == blob
        open(os.path.join(out, "f." + fd["name"]), "wb").write(
            bytes([PACKED & 255, PACKED >> 8]) + packed)
        print("mkfields: f.%-9s %3d characters" % (fd["name"], len(p["glyphs"])))
    for name in monsters(sprites, order):
        size, frames = sprites[name]
        data = b"".join(b for fr in frames for b in blocks(size, fr))
        open(os.path.join(out, "s." + name), "wb").write(
            bytes([MONSPR & 255, MONSPR >> 8]) + data)
    print("mkfields: %d monsters' sprites, s.*" % len(monsters(sprites, order)))


if __name__ == "__main__":
    main()
