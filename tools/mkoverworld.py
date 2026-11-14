#!/usr/bin/env python3
"""Build the smooth-scrolling prototype's tile sets.

Each set is an asset file -- assets/overworld.txt (which has the format, and
the hero's sprite), and the places' sets, assets/town_tiles.txt,
castle_tiles.txt and dungeon_tiles.txt -- and comes out in src/lib/ow_data.s
as one blob the engine (src/lib/smooth.s, sm_tileset) unpacks into its
tables when you go somewhere that uses it: the multicolour character set, a
colour for each character, the metatiles, their edges, and the water
characters it animates. Identical characters (same pixels, same colour) are
stored once in a set.

A set's file can `include` another, for art two sets share.
"""

import os
import random
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "overworld.txt")
TILES = os.path.join(ROOT, "assets", "tiles.txt")
# The sets, in the order the engine numbers them (openworld.c's TS_*).
SETS = ["overworld", "town", "castle", "dungeon"]
SET_FILE = {"overworld": "overworld.txt", "town": "town_tiles.txt",
            "castle": "castle_tiles.txt", "dungeon": "dungeon_tiles.txt"}
OUT = os.path.join(ROOT, "src", "lib", "ow_data.s")

COLOURS = {
    "black": 0, "white": 1, "red": 2, "cyan": 3, "purple": 4, "green": 5,
    "blue": 6, "yellow": 7, "orange": 8, "brown": 9, "lightred": 10,
    "gray1": 11, "gray2": 12, "lightgreen": 13, "lightblue": 14, "gray3": 15,
}
TILE_BASE = 0x60
TILE_SLOTS = 64          # tile codes $60-$9F, indexed (code - $60) & 63
PIX = {".": 0, "a": 1, "b": 2, "c": 3}
WATER_MAX = 8            # water character pairs a set can animate


def fail(lineno, msg):
    sys.exit("%s:%d: %s" % (SRC, lineno, msg))


def read_lines(path, seen=()):
    """The file's lines, with each `include <file>` replaced by that file's,
    as (text, where) pairs so errors can say where."""
    out = []
    for n, text in enumerate(open(path).read().split("\n")):
        f = text.split("#")[0].split()
        if f and f[0] == "include":
            inc = os.path.join(os.path.dirname(path), f[1])
            if inc in seen:
                sys.exit("%s:%d: %s includes itself" % (path, n + 1, f[1]))
            out += read_lines(inc, seen + (path,))
        else:
            out.append((text, "%s:%d" % (os.path.relpath(path, ROOT), n + 1)))
    return out


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


def parse(path):
    global SRC
    tagged = read_lines(path)
    lines = [t for t, _ in tagged]
    palette = sprcols = shore = None
    opens, fringes, drawn, also = [], {}, {}, {}
    tiles, sprites = [], []
    offmap = fallback = None
    i = 0
    while i < len(lines):
        f = lines[i].split("#")[0].split()
        SRC, lineno = tagged[i][1].rsplit(":", 1)
        lineno = int(lineno)
        i += 1
        if not f:
            continue
        if f[0] == "palette":
            palette = [colour(n, lineno) for n in f[1:4]]
        elif f[0] == "offmap":
            offmap = f[1]               # what lies beyond the map's edges
        elif f[0] == "fallback":
            fallback = f[1]             # what a tile with no art is drawn as
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
            # shore <colour> [<headland colour>] [against <tile>...]: the
            # beaches' own colour, the headland's if it differs, and what
            # land makes a beach (anything not water, if not said).
            against = None
            if "against" in f:
                against = f[f.index("against") + 1:]
                f = f[:f.index("against")]
            water_col = colour(f[1], lineno, ram=True)
            head_col = colour(f[2], lineno, ram=True) if len(f) > 2 else water_col
            shore = (water_col, v, head_col, against)
        elif f[0] == "edges":
            # edges <terrain>...: their edges drawn, not cut: beside, above, both,
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
            for n in f[1:]:
                drawn[n] = [[h[:4] for h in half],
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
            legal = ".abc-" if f[0] == "tile" else ".kst"
            for n, r in enumerate(rows):
                if len(r) != 8 or any(ch not in legal for ch in r):
                    fail(lineno + 1 + n, "want 8 of %r, got %r" % (legal, r))
            if f[0] == "tile":
                # tile <name> <colour>... [on <tile>]: a - is the other
                # tile's pixel, for things that stand on a floor.
                base = None
                if "on" in f:
                    base = f[f.index("on") + 1]
                    f = f[:f.index("on")]
                elif any("-" in r for r in rows):
                    fail(lineno, "a - wants `on <tile>`")
                cols = [colour(n, lineno, ram=True) for n in f[2:]]
                if len(cols) == 1:
                    cols *= 4
                if len(cols) != 4:
                    fail(lineno, "one colour or four")
                tiles.append((f[1], cols, rows, lineno, base))
            else:
                sprites.append((f[1], rows))
        else:
            fail(lineno, "what is %r?" % f[0])
    # What stands on something is drawn over the first of that something.
    art = {}
    for name, cols, rows, lineno, base in tiles:
        art.setdefault(name, rows)
    done = []
    for name, cols, rows, lineno, base in tiles:
        if base:
            if base not in art:
                fail(lineno, "on %r: no such tile above" % base)
            rows = ["".join(b if ch == "-" else ch for ch, b in zip(r, br))
                    for r, br in zip(rows, art[base])]
        done.append((name, cols, rows, lineno))
    return (palette, sprcols, done, sprites, shore,
            (fringes, opens, drawn, also), offmap, fallback)


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


def build(setname):
    """One set: its blob's bytes, its name for each metatile, and what is
    worth printing about it."""
    path = os.path.join(ROOT, "assets", SET_FILE[setname])
    (palette, sprcols, tiles, sprites, shore, fringe, offmap,
     fallback) = parse(path)
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
    # both, diagonal), the character to draw instead of its own. With a
    # shore, water gets the beaches and grass a headland where sea is on two
    # sides; the fringe's terrains get their own art with the mask's squares
    # cut back to the background, and the drawn ones what is drawn; anything
    # else keeps its own character.
    beach = shore and [[char(art_glyph(quarter(shore[1][v], q)),
                             shore[0] if v < 4 else shore[2])
                        for v in range(5)] for q in range(4)]
    fringes, open_names, drawn, also = fringe
    for n in (list(fringes) + open_names + list(drawn) + list(also) +
              [m for v in also.values() for m in v]):
        if n not in by_name:
            sys.exit("mkoverworld: %s: no tile %r in the set" % (setname, n))
    blend = []
    for m, quad in enumerate(metatiles):
        name = mt_name[m]
        art = mt_art[m]
        for q in range(4):
            for v in range(4):
                if shore and name == "water":
                    c = beach[q][v]
                elif shore and name == "grass":
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
    # each tile code, which of those bits make an edge on it (bsel), and
    # whether they count when set or when clear (binv): water's edges are
    # where land is, grass's where land is not (with a shore), a fringe's
    # where open ground or its `also` terrains are; 0 for none.
    against = shore[3] if shore else None
    extra = sorted({m for v in also.values() for m in v} | set(against or []))
    bit_of = {m: 1 << (2 + k) for k, m in enumerate(extra)}
    if len(extra) > 6:
        sys.exit("mkoverworld: also: %d terrains, 6 at most" % len(extra))
    slot_name = [next((n for n, c in codes.items() if c - TILE_BASE == slot),
                      None) for slot in range(TILE_SLOTS)]
    nflag, bsel, binv = [], [], []
    for name in slot_name:
        nflag.append((name not in ("water", "port", "bridge")) |
                     (name in open_names) << 1 | bit_of.get(name, 0))
        if shore and name == "water":
            sel, inv = 1, 0             # where land is ...
            if against:                 # ... or only that land
                sel = 0
                for m in against:
                    sel |= bit_of[m]
        elif shore and name == "grass":
            sel, inv = 1, 0xFF
        elif name in fringes or name in drawn:
            # Against open ground -- unless it is open ground itself (a
            # town's floor fraying into grass), when only its `also` count.
            sel = 0 if name in open_names else 2
            for m in also.get(name, []):
                sel |= bit_of[m]
            inv = 0
        else:
            sel, inv = 0, 0
        bsel.append(sel)
        binv.append(inv)

    if len(chars) > 256:
        sys.exit("mkoverworld: %s: %d characters, 256 at most"
                 % (setname, len(chars)))
    if len(metatiles) > 32:
        sys.exit("mkoverworld: %s: %d metatiles, 32 at most"
                 % (setname, len(metatiles)))
    if len(water) > WATER_MAX:
        sys.exit("mkoverworld: %s: %d water pairs, %d at most"
                 % (setname, len(water), WATER_MAX))
    # A water character shared with anything else would animate there too.
    flat = [c for pair in water for c in pair]
    if len(set(flat)) != len(flat):
        sys.exit("mkoverworld: water characters must be distinct")

    fallback = fallback or mt_name[0]
    if fallback not in by_name:
        sys.exit("mkoverworld: %s: fallback %r has no art" % (setname, fallback))
    mt0, mt1 = [], []
    for name in slot_name:
        v = by_name.get(name, by_name[fallback])
        mt0.append(v[0])
        mt1.append(v[-1])
    offmap = offmap or fallback
    if offmap not in codes:
        sys.exit("mkoverworld: %s: offmap %r is no tile" % (setname, offmap))

    # The blob, in the order sm_tileset reads it.
    nmt = len(metatiles)
    blob = [palette[0], palette[1], palette[2], len(chars) & 255,
            len(water), codes[offmap], nmt]
    blob += [c for pair in water for c in pair]
    blob += [8 | c for _, c in chars]
    blob += mt0 + mt1 + nflag + bsel + binv
    for q in range(4):
        blob += [metatiles[m][q] for m in range(nmt)]
    blob += blend
    for g, _ in chars:
        blob += list(g)
    return {"blob": blob, "names": mt_name, "chars": len(chars),
            "metatiles": nmt, "sprites": sprites, "sprcols": sprcols,
            "extra": extra}


def main():
    sets = {name: build(name) for name in SETS}
    ow = sets["overworld"]
    sprites, sprcols = ow["sprites"], ow["sprcols"]
    codes = tile_codes()

    # Which tiles can be walked onto, from assets/tiles.txt: the same in
    # every set.
    flags = {}
    for line in open(TILES):
        f = line.split()
        if len(f) >= 5 and f[0] == "tile":
            flags[int(f[3], 16)] = f[4]
    passable = [1 if flags.get(TILE_BASE + slot) in ("pass", "enter") else 0
                for slot in range(TILE_SLOTS)]

    rnd = random.Random(0x0C64)
    hashes = [rnd.randrange(256) for _ in range(256)]

    out = []
    w = out.append
    w("; GENERATED from assets/*tiles.txt and overworld.txt by "
      "tools/mkoverworld.py -- do not edit.")
    w("")
    w('        .export _ow_spr1, _ow_spr2, _ow_pass')
    w('        .export ow_rnd, ow_qvar, ow_sprites, ow_nsprites')
    w('        .export ' + ", ".join("_ts_" + n for n in SETS))
    w("")
    w("; All of it is only read, so it goes in HIGH (cfg/c64-openworld.cfg).")
    w('        .segment "HIDATA"')
    w("")
    w("_ow_spr1:    .byte %d" % sprcols[0])
    w("_ow_spr2:    .byte %d" % sprcols[1])
    w("ow_nsprites: .byte %d" % len(sprites))
    w("")
    w("; Whether each tile code ($60 + index) can be walked onto.")
    w("_ow_pass: .byte " + ",".join(str(b) for b in passable))
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
    w("; The tile sets, each as sm_tileset reads it: the three shared colours,")
    w("; the numbers of characters (0 is 256), water pairs and metatiles, what")
    w("; is off the map, the water pairs, each character's colour RAM value,")
    w("; the metatiles of each tile code and of its variant, each tile code's")
    w("; neighbour flags, edge selection and inversion, the metatiles'")
    w("; characters a quarter at a time, their edges, and the characters.")
    for name in SETS:
        st = sets[name]
        w("")
        w("; %s: %d characters, %d metatiles (%s); %d bytes"
          % (name, st["chars"], st["metatiles"], ", ".join(st["names"]),
             len(st["blob"])))
        w("_ts_%s:" % name)
        b = st["blob"]
        for n in range(0, len(b), 16):
            w("        .byte " + ",".join("$%02X" % x for x in b[n:n + 16]))
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
    for name in SETS:
        st = sets[name]
        print("mkoverworld: %-9s %3d characters, %2d metatiles, %5d bytes"
              % (name, st["chars"], st["metatiles"], len(st["blob"])))
    print("mkoverworld: -> %s" % os.path.relpath(OUT, ROOT))


if __name__ == "__main__":
    main()
