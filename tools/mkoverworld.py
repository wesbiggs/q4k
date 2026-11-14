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
PIX = {".": 0, "a": 1, "b": 2, "c": 3}
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
    opens, fringes, drawn, also = [], {}, {}, {}
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
            seams(v, lineno, "shore")
            shore = (colour(f[1], lineno, ram=True), v)
        elif f[0] == "edges":
            # edges <terrain>: its edges drawn, not cut: beside, above, both,
            # diagonal, then beside and above for its second variant; eight
            # rows for the top left quarter and eight for the bottom left,
            # since what is drawn has a top and must not be turned over.
            rows = [r.split() for r in lines[i:i + 16]]
            i += 16
            for n, r in enumerate(rows):
                if len(r) != 6 or any(len(q) != 4 or set(q) - set(".abc")
                                      for q in r):
                    fail(lineno + 1 + n, "want six of 4 of '.abc', got %r"
                         % " ".join(r))
            # drawn[name][variant][top or bottom][edge] = 8 rows of 4
            half = [[[r[k] for r in rows[h * 8:h * 8 + 8]] for k in range(6)]
                    for h in range(2)]
            drawn[f[1]] = [[h[:4] for h in half],
                           [h[4:6] + h[2:4] for h in half]]
        elif f[0] == "open":
            opens = f[1:]
        elif f[0] == "also":
            also[f[1]] = f[2:]
        elif f[0] == "fringe":
            # fringe <terrain>...: those terrains fray where they meet open
            # ground, in the masks that follow: beside, above, both and
            # diagonal for a terrain's first variant, and beside and above
            # again for its second, so that an edge is not one notch
            # repeated. The second pair must start and end as the first.
            rows = [r.split() for r in lines[i:i + 8]]
            i += 8
            for n, r in enumerate(rows):
                if len(r) != 6 or any(len(q) != 4 or set(q) - set(".x")
                                      for q in r):
                    fail(lineno + 1 + n, "want six of 4 of '.x', got %r"
                         % " ".join(r))
            v = [[r[k] for r in rows] for k in range(6)]
            col = lambda art, c: "".join(r[c] for r in art)
            if (v[4][0], v[4][7]) != (v[0][0], v[0][7]) or \
                    (col(v[5], 0), col(v[5], 3)) != (col(v[1], 0), col(v[1], 3)):
                fail(lineno, "fringe: the second beside must start and end "
                     "as the first, and the second above too")
            seams(v[:4], lineno, "fringe")
            for n in f[1:]:
                fringes[n] = [v[:4], v[4:6] + v[2:4]]
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
    return (palette, sprcols, tiles, sprites, shore,
            (fringes, opens, drawn, also))


def seams(v, lineno, what):
    """Edges that meet another square's edge of a different kind have to
    match, or the join shows: the diagonal's top row is the beside's, and
    its left column the above's; both's bottom row is the beside's, and its
    right column the above's. (Each against itself is mirrored, so meets.)"""
    col = lambda art, c: "".join(r[c] for r in art)
    for say, ok in (
            ("the diagonal's top row is the beside's", v[3][0] == v[0][0]),
            ("the diagonal's left column is the above's",
             col(v[3], 0) == col(v[1], 0)),
            ("both's bottom row is the beside's", v[2][7] == v[0][7]),
            ("both's right column is the above's",
             col(v[2], 3) == col(v[1], 3))):
        if not ok:
            sys.exit("%s:%d: %s: %s, so a join would show a seam"
                     % (SRC, lineno, what, say))


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
    palette, sprcols, tiles, sprites, shore, fringe = parse()
    codes = tile_codes()

    chars = []           # (glyph bytes, colour)
    char_of = {}
    metatiles = []       # four character codes each
    water = []           # (left, right) character pairs to rotate
    by_name = {}
    mt_name, mt_art, mt_cols = [], [], []
    for name, cols, rows, lineno in tiles:
        if name not in codes:
            fail(lineno, "no tile %r in assets/tiles.txt" % name)
        quad = []
        for q in range(4):
            key = (glyph(rows, q & 1, q >> 1, PIX), cols[q])
            if key not in char_of:
                char_of[key] = len(chars)
                chars.append(key)
            quad.append(char_of[key])
        if name == "water":
            water += [(quad[0], quad[1]), (quad[2], quad[3])]
        by_name.setdefault(name, []).append(len(metatiles))
        metatiles.append(quad)
        mt_name.append(name)
        mt_art.append(rows)
        mt_cols.append(cols)
    def char(g, colour):
        key = (g, colour)
        if key not in char_of:
            char_of[key] = len(chars)
            chars.append(key)
        return char_of[key]

    def art_glyph(rows):
        return tuple(sum(PIX[ch] << (6 - 2 * k) for k, ch in enumerate(r))
                     for r in rows)

    # The edges: for each metatile, quarter and kind of edge (beside, above,
    # both, diagonal), the character to draw instead of its own. Water gets
    # the beaches; grass only a headland, where sea is on both sides; the
    # fringe's terrains their own art with the mask's squares cut back to the
    # background; anything else keeps its own character.
    beach = [[char(art_glyph(quarter(shore[1][v], q)), shore[0])
              for v in range(5)] for q in range(4)]
    fringes, open_names, drawn, also = fringe
    for n in (list(fringes) + open_names + list(drawn) + list(also) +
              [m for v in also.values() for m in v]):
        if n not in by_name:
            sys.exit("mkoverworld: fringe: no tile %r here" % n)
    blend = []
    for m, quad in enumerate(metatiles):
        name = mt_name[m]
        art = mt_art[m]
        for q in range(4):
            for v in range(4):
                if name == "water":
                    c = beach[q][v]
                elif name == "grass":
                    c = beach[q][4] if v == 2 else quad[q]
                elif name in drawn:
                    nth = min(by_name[name].index(m), 1)
                    # Mirrored for a right-hand quarter only where the open
                    # side is a side: above or below is the same from both,
                    # and a mirrored tree has its shadow on the wrong side.
                    art_q = drawn[name][nth][q >> 1][v]
                    if q & 1 and v != 1:
                        art_q = [r[::-1] for r in art_q]
                    c = char(art_glyph(art_q), mt_cols[m][q])
                elif name in fringes:
                    nth = by_name[name].index(m)
                    mask = quarter(fringes[name][min(nth, 1)][v], q)
                    own = [r[(q & 1) * 4:(q & 1) * 4 + 4]
                           for r in art[(q >> 1) * 8:(q >> 1) * 8 + 8]]
                    cut = ["".join("." if mask[y][x] == "x" else own[y][x]
                                   for x in range(4)) for y in range(8)]
                    c = char(art_glyph(cut), mt_cols[m][q])
                else:
                    c = quad[q]
                blend.append(c)

    # What each tile code is to its neighbours, for sm_shores: bit 0 land
    # (not water, a pier or a bridge), bit 1 open ground, and a bit more for
    # each terrain that something frays against besides (`also`). And for
    # each tile code, which of those bits make an edge on it (ow_bsel), and
    # whether they count when set or when clear (ow_binv): water's edges are
    # where land is, grass's where land is not, a fringe's where open ground
    # or its `also` terrains are; 0 for a tile that takes no edges.
    extra = sorted({m for v in also.values() for m in v})
    bit_of = {m: 1 << (2 + k) for k, m in enumerate(extra)}
    if len(extra) > 6:
        sys.exit("mkoverworld: also: %d terrains, 6 at most" % len(extra))
    nflag, bsel, binv = [], [], []
    for slot in range(TILE_SLOTS):
        name = next((n for n, c in codes.items() if c - TILE_BASE == slot),
                    None)
        nflag.append((name not in ("water", "port", "bridge")) |
                     (name in open_names) << 1 | bit_of.get(name, 0))
        if name == "water":
            sel, inv = 1, 0
        elif name == "grass":
            sel, inv = 1, 0xFF
        elif name in fringes or name in drawn:
            sel = 2
            for m in also.get(name, []):
                sel |= bit_of[m]
            inv = 0
        else:
            sel, inv = 0, 0
        bsel.append(sel)
        binv.append(inv)

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
    w('        .export ow_blend, ow_qvar, ow_nflag, ow_bsel, ow_binv')
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
    w("; The edges: metatile * 16 + quarter * 4 + which - 1 (1 beside, 2 above")
    w("; or below, 3 both, 4 the diagonal only), the character to draw.")
    w("ow_blend:")
    for m in range(len(metatiles)):
        w("        .byte " + ",".join(str(c) for c in blend[m * 16:m * 16 + 16])
          + "   ; " + mt_name[m])
    w("")
    w("; Per tile code ($60 + index): bit 0 land, bit 1 open ground, then")
    w("; " + ", ".join("bit %d %s" % (2 + k, m) for k, m in enumerate(extra)) + ".")
    w("ow_nflag:  .byte " + ",".join(str(b) for b in nflag))
    w("; Per tile code: which of a neighbour's bits make an edge (0 none) ...")
    w("ow_bsel:   .byte " + ",".join(str(b) for b in bsel))
    w("; ... and whether when set (0) or when clear ($FF).")
    w("ow_binv:   .byte " + ",".join(str(b) for b in binv))
    w("")
    w("; Which edge (0 none) for each quarter and each byte of what is round")
    w("; the square: 256 a quarter.")
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
