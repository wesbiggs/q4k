#!/usr/bin/env python3
"""Build the smooth-scrolling prototype's tile data from assets/overworld.txt.

Writes src/lib/ow_data.s: the multicolour character set, a colour for each
character, the metatile tables the engine (src/lib/smooth.s) draws through,
the water characters it animates, and the hero's sprite frames. See the
asset file for the format.

Identical characters (same pixels, same colour) are stored once.
"""

import os
import random
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "overworld.txt")
TILES = os.path.join(ROOT, "assets", "tiles.txt")
OUT = os.path.join(ROOT, "src", "lib", "ow_data.s")

COLOURS = {
    "black": 0, "white": 1, "red": 2, "cyan": 3, "purple": 4, "green": 5,
    "blue": 6, "yellow": 7, "orange": 8, "brown": 9, "lightred": 10,
    "gray1": 11, "gray2": 12, "lightgreen": 13, "lightblue": 14, "gray3": 15,
}
TILE_BASE = 0x60
TILE_SLOTS = 32          # tile codes $60-$7F, indexed (code - $60) & 31
FALLBACK = "grass"       # whatever has no metatile of its own


def fail(lineno, msg):
    sys.exit("%s:%d: %s" % (SRC, lineno, msg))


def tile_codes():
    codes = {}
    for line in open(TILES):
        f = line.split()
        if len(f) >= 4 and f[0] == "tile":
            codes[f[1]] = int(f[3], 16)
    return codes


def colour(name, lineno, ram=False):
    if name not in COLOURS:
        fail(lineno, "unknown colour %r" % name)
    c = COLOURS[name]
    if ram and c > 7:
        fail(lineno, "%s cannot be a character's own colour in "
                     "multicolour: 0-7 only" % name)
    return c


def parse():
    lines = open(SRC).read().split("\n")
    palette = sprcols = shore = None
    tiles, sprites = [], []
    i = 0
    while i < len(lines):
        f = lines[i].split("#")[0].split()
        lineno = i + 1
        i += 1
        if not f:
            continue
        if f[0] == "palette":
            palette = [colour(n, lineno) for n in f[1:4]]
        elif f[0] == "spritecolors":
            sprcols = [colour(n, lineno) for n in f[1:3]]
        elif f[0] == "shore":
            rows = [r.split() for r in lines[i:i + 8]]
            i += 8
            for n, r in enumerate(rows):
                if len(r) != 5 or any(len(q) != 4 or set(q) - set(".abc")
                                      for q in r):
                    fail(lineno + 1 + n, "want five of 4 of '.abc', got %r"
                         % " ".join(r))
            # Five variants, each 8 rows of 4: beside, above, both, diagonal,
            # and the grass headland.
            v = [[r[k] for r in rows] for k in range(5)]
            col = lambda art, c: "".join(r[c] for r in art)
            for what, ok in (
                    ("the diagonal's top row is the beside's", v[3][0] == v[0][0]),
                    ("the diagonal's left column is the above's",
                     col(v[3], 0) == col(v[1], 0)),
                    ("both's bottom row is the beside's", v[2][7] == v[0][7]),
                    ("both's right column is the above's",
                     col(v[2], 3) == col(v[1], 3))):
                if not ok:
                    fail(lineno, "shore: %s, so a coast would show a seam"
                         % what)
            shore = (colour(f[1], lineno, ram=True), v)
        elif f[0] in ("tile", "sprite"):
            rows = lines[i:i + 16]
            i += 16
            legal = ".abc" if f[0] == "tile" else ".kst"
            for n, r in enumerate(rows):
                if len(r) != 8 or any(ch not in legal for ch in r):
                    fail(lineno + 1 + n, "want 8 of %r, got %r" % (legal, r))
            if f[0] == "tile":
                cols = [colour(n, lineno, ram=True) for n in f[2:]]
                if len(cols) == 1:
                    cols *= 4
                if len(cols) != 4:
                    fail(lineno, "one colour or four")
                tiles.append((f[1], cols, rows, lineno))
            else:
                sprites.append((f[1], rows))
        else:
            fail(lineno, "what is %r?" % f[0])
    return palette, sprcols, tiles, sprites, shore


# The land round a water square, a bit a neighbour: north, then clockwise.
N, NE, E, SE, S, SW, W, NW = range(8)
# Each quarter's own three: beside it, above or below it, and the diagonal.
CORNERS = [(W, N, NW), (E, N, NE), (W, S, SW), (E, S, SE)]


def qvar(q, mask):
    """Which beach a quarter gets: 0 none, 1 beside, 2 above or below,
    3 both, 4 only the diagonal."""
    h, v, d = (bool(mask & (1 << b)) for b in CORNERS[q])
    if h and v:
        return 3
    if h:
        return 1
    if v:
        return 2
    return 4 if d else 0


def quarter(rows, q):
    """The top left quarter's art turned to face quarter q."""
    if q & 1:
        rows = [r[::-1] for r in rows]
    if q & 2:
        rows = rows[::-1]
    return rows


def glyph(rows, qx, qy, values):
    """One character of a metatile, as 8 bytes."""
    out = []
    for r in rows[qy * 8:qy * 8 + 8]:
        b = 0
        for ch in r[qx * 4:qx * 4 + 4]:
            b = (b << 2) | values[ch]
        out.append(b)
    return tuple(out)


def main():
    palette, sprcols, tiles, sprites, shore = parse()
    codes = tile_codes()

    chars = []           # (glyph bytes, colour)
    char_of = {}
    metatiles = []       # four character codes each
    water = []           # (left, right) character pairs to rotate
    by_name = {}
    for name, cols, rows, lineno in tiles:
        if name not in codes:
            fail(lineno, "no tile %r in assets/tiles.txt" % name)
        quad = []
        for q in range(4):
            key = (glyph(rows, q & 1, q >> 1, {".": 0, "a": 1, "b": 2, "c": 3}),
                   cols[q])
            if key not in char_of:
                char_of[key] = len(chars)
                chars.append(key)
            quad.append(char_of[key])
        if name == "water":
            water += [(quad[0], quad[1]), (quad[2], quad[3])]
        by_name.setdefault(name, []).append(len(metatiles))
        metatiles.append(quad)
    shore_chars = [0] * 32
    for q in range(4):
        for v in range(5):
            rows = quarter(shore[1][v], q)
            g = tuple(sum({".": 0, "a": 1, "b": 2, "c": 3}[ch] << (6 - 2 * k)
                          for k, ch in enumerate(r)) for r in rows)
            key = (g, shore[0])
            if key not in char_of:
                char_of[key] = len(chars)
                chars.append(key)
            # 1-4 the water's, by qvar(); 7 the grass headland.
            shore_chars[q * 8 + (v + 1 if v < 4 else 7)] = char_of[key]
    if len(chars) > 256:
        sys.exit("mkoverworld: %d characters, 256 at most" % len(chars))
    if len(metatiles) > 32:
        sys.exit("mkoverworld: %d metatiles, 32 at most" % len(metatiles))
    # A water character shared with anything else would animate there too.
    flat = [c for pair in water for c in pair]
    if len(set(flat)) != len(flat):
        sys.exit("mkoverworld: water characters must be distinct")

    mt0, mt1 = [], []
    for slot in range(TILE_SLOTS):
        name = next((n for n, c in codes.items() if c - TILE_BASE == slot),
                    None)
        v = by_name.get(name, by_name[FALLBACK])
        mt0.append(v[0])
        mt1.append(v[-1])

    rnd = random.Random(0x0C64)
    hashes = [rnd.randrange(256) for _ in range(256)]

    out = []
    w = out.append
    w("; GENERATED from assets/overworld.txt by tools/mkoverworld.py -- do not edit.")
    w("")
    w('        .export _ow_bg, _ow_mc1, _ow_mc2, _ow_spr1, _ow_spr2')
    w('        .export ow_chars, ow_nchars, ow_chcol, ow_mt0, ow_mt1, ow_chq')
    w('        .export ow_rnd, ow_water, ow_nwater, ow_sprites, ow_nsprites')
    w('        .export ow_shore, ow_qvar')
    w("")
    w("; All of it is only read, so it goes in HIGH (cfg/c64-openworld.cfg).")
    w('        .segment "HIDATA"')
    w("")
    w("_ow_bg:      .byte %d" % palette[0])
    w("_ow_mc1:     .byte %d" % palette[1])
    w("_ow_mc2:     .byte %d" % palette[2])
    w("_ow_spr1:    .byte %d" % sprcols[0])
    w("_ow_spr2:    .byte %d" % sprcols[1])
    w("ow_nchars:   .byte %d" % len(chars))
    w("ow_nwater:   .byte %d" % len(water))
    w("ow_nsprites: .byte %d" % len(sprites))
    w("")
    w("; Each character's colour RAM value: its own colour, multicolour on.")
    w("ow_chcol:")
    cc = [8 | c for _, c in chars] + [8] * (256 - len(chars))
    for n in range(0, 256, 16):
        w("        .byte " + ",".join("$%02X" % b for b in cc[n:n + 16]))
    w("")
    w("; The metatile of each tile code ($60 + index), and of its variant.")
    w("ow_mt0: .byte " + ",".join(str(m) for m in mt0))
    w("ow_mt1: .byte " + ",".join(str(m) for m in mt1))
    w("")
    w("; The character of each quadrant of each metatile: 32 to a quadrant,")
    w("; top left, top right, bottom left, bottom right.")
    w("ow_chq:")
    for q in range(4):
        row = [metatiles[m][q] if m < len(metatiles) else 0 for m in range(32)]
        w("        .byte " + ",".join(str(c) for c in row))
    w("")
    w("; The beaches: quarter * 8 + which (1 beside, 2 above or below, 3 both,")
    w("; 4 the diagonal only; 7 a quarter of grass with sea on both sides).")
    w("ow_shore: .byte " + ",".join(str(c) for c in shore_chars))
    w("")
    w("; Which (0 none) for each quarter and each byte of land round the")
    w("; square: 256 a quarter.")
    w("ow_qvar:")
    for q in range(4):
        for n in range(0, 256, 32):
            w("        .byte " + ",".join(str(qvar(q, m)) for m in range(n, n + 32)))
    w("")
    w("; Which variant a square gets: ow_rnd[(ow_rnd[y] + x) & 255] & 1.")
    w("ow_rnd:")
    for n in range(0, 256, 16):
        w("        .byte " + ",".join("$%02X" % b for b in hashes[n:n + 16]))
    w("")
    w("; Water's characters, left and right, a row of the metatile each.")
    w("ow_water: .byte " + ",".join("%d,%d" % p for p in water))
    w("")
    w("ow_chars:")
    for n, (g, c) in enumerate(chars):
        w("        .byte " + ",".join("$%02X" % b for b in g) + "   ; %d" % n)
    w("")
    w("; The hero: down, up, left and right, two strides each; 64 bytes a frame.")
    w("ow_sprites:")
    for name, rows in sprites:
        data = []
        for r in range(21):
            b0 = 0
            if r < 16:
                for ch in rows[r][:4]:
                    b0 = (b0 << 2) | {".": 0, "k": 1, "t": 2, "s": 3}[ch]
                b1 = 0
                for ch in rows[r][4:]:
                    b1 = (b1 << 2) | {".": 0, "k": 1, "t": 2, "s": 3}[ch]
            else:
                b1 = 0
            data += [b0, b1, 0]
        data.append(0)
        w("        .byte " + ",".join("$%02X" % b for b in data) + "   ; " + name)
    w("")

    open(OUT, "w").write("\n".join(out))
    print("mkoverworld: %d characters, %d metatiles, %d sprite frames -> %s"
          % (len(chars), len(metatiles), len(sprites),
             os.path.relpath(OUT, ROOT)))


if __name__ == "__main__":
    main()
