#!/usr/bin/env python3
"""Build the smooth-scrolling prototype's tile sets.

Each set is an asset file -- assets/overworld.txt (which has the format, and
the hero's sprite), and the places' sets, assets/town_tiles.txt,
castle_tiles.txt and dungeon_tiles.txt -- and comes out in src/lib/ow_data.s
as one blob, a file of its own on the disk (`--sets DIR` writes them:
t.overworld, t.town, t.castle, t.dungeon), which the engine loads into SHORE
($A000, the file's address) and unpacks (src/ow/smooth.s, sm_tileset) into
its tables when you go somewhere that uses it: the multicolour character set, a
colour for each character, the metatiles, their edges, and the water
characters it animates. Identical characters (same pixels, same colour) are
stored once in a set.

A set's file can `include` another, for art two sets share.
"""

import os
import random
import sys

import figures

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "overworld.txt")
TILES = os.path.join(ROOT, "assets", "tiles.txt")
# The sets, in the order the engine numbers them (openworld.c's TS_*).
SETS = ["overworld", "town", "castle", "dungeon"]
FONT = 0xA0             # smooth.s's: the text window's characters
SET_FILE = {"overworld": "overworld.txt", "town": "town_tiles.txt",
            "castle": "castle_tiles.txt", "dungeon": "dungeon_tiles.txt"}
OUT = os.path.join(ROOT, "src", "lib", "ow_data.s")
OUT_FIGS = os.path.join(ROOT, "src", "lib", "ow_figs.s")

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
    global SRC, GROUND
    GROUND = None
    tagged = read_lines(path)
    lines = [t for t, _ in tagged]
    palette = sprcols = shore = None
    opens, fringes, drawn, also, joins = [], {}, {}, {}, {}
    own_codes, own_pass = {}, set()
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
        elif f[0] == "code":
            # code <name> <hex> [pass]: a tile of this set's own, which the
            # game does not have (a roof): a code from $92-$9F, which none
            # uses; with pass, one the hero walks on (a road).
            n = int(f[2], 16)
            if not 0x92 <= n <= 0x9F:
                fail(lineno, "code: $92-$9F only")
            own_codes[f[1]] = n
            if f[3:] == ["pass"]:
                own_pass.add(n)
            elif f[3:]:
                fail(lineno, "code <name> <hex> [pass]")
        elif f[0] == "ground":
            # ground, then 16 rows of 8 in . a b: what every background
            # pixel of the set shows instead, by where it is in its square
            # (so it lines up across tiles, edges and beaches). A shared
            # colour, since the pixel may be in anyone's character.
            rows = lines[i:i + 16]
            i += 16
            for n, r in enumerate(rows):
                if len(r) != 8 or set(r) - set(".ab"):
                    fail(lineno + 1 + n, "want 8 of '.ab', got %r" % r)
            GROUND = rows
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
        elif f[0] == "join":
            # join <terrain> <tile>...: the terrain's edges are wherever a
            # neighbour is neither itself nor one of those (a road runs into
            # a town, and ends anywhere else).
            joins[f[1]] = f[2:]
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
                # sprite <name> [bob]: with bob, a second frame it idles
                # between, sunk a row (tools/figures.py).
                sprites.append((f[1], rows))
                if "bob" in f[2:]:
                    sprites.append((f[1] + " bobbing", figures.bob(rows)))
        else:
            fail(lineno, "what is %r?" % f[0])
    # What stands on something is drawn over the first of that something.
    art = {}
    for name, cols, rows, lineno, base in tiles:
        art.setdefault(name, rows)
    done = []
    OVERLAYS.clear()
    for name, cols, rows, lineno, base in tiles:
        if base:
            OVERLAYS.setdefault(name, (cols, rows))
        if base:
            if base not in art:
                fail(lineno, "on %r: no such tile above" % base)
            rows = ["".join(b if ch == "-" else ch for ch, b in zip(r, br))
                    for r, br in zip(rows, art[base])]
        done.append((name, cols, rows, lineno))
    return (palette, sprcols, done, sprites, shore,
            (fringes, opens, drawn, also, joins), offmap, fallback,
            own_codes, own_pass)


# The art of the last set parsed that stands `on` another tile, as drawn
# (with its - pixels): tools/mkfields.py puts it on other ground.
OVERLAYS = {}


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


# The set's `ground`, while one is being built (None: plain background).
GROUND = None


def grounded(rows, q, ground=None):
    """Quarter q's 8 rows of 4, its background pixels showing the ground."""
    ground = ground or GROUND
    if not ground:
        return rows
    gx, gy = (q & 1) * 4, (q >> 1) * 8
    return ["".join(ground[gy + y][gx + x] if ch == "." else ch
                    for x, ch in enumerate(r)) for y, r in enumerate(rows)]


def glyph(rows, qx, qy, values):
    """One character of a metatile, as 8 bytes."""
    out = []
    part = [r[qx * 4:qx * 4 + 4] for r in rows[qy * 8:qy * 8 + 8]]
    for r in grounded(part, qy * 2 + qx):
        b = 0
        for ch in r:
            b = (b << 2) | values[ch]
        out.append(b)
    return tuple(out)


def build(setname):
    """One set: its blob's bytes, its name for each metatile, and what is
    worth printing about it."""
    path = os.path.join(ROOT, "assets", SET_FILE[setname])
    (palette, sprcols, tiles, sprites, shore, fringe, offmap,
     fallback, own_codes, own_pass) = parse(path)
    codes = tile_codes()
    for name, n in own_codes.items():
        if n in codes.values() and codes.get(name) != n:
            sys.exit("mkoverworld: %s: code %s $%02X is assets/tiles.txt's"
                     % (setname, name, n))
    codes.update(own_codes)

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

    def art_glyph(rows, q):
        return tuple(sum(PIX[ch] << (6 - 2 * k) for k, ch in enumerate(r))
                     for r in grounded(rows, q))

    # The edges: for each metatile, quarter and kind of edge (beside, above,
    # both, diagonal), the character to draw instead of its own. With a
    # shore, water gets the beaches and grass a headland where sea is on two
    # sides; the fringe's terrains get their own art with the mask's squares
    # cut back to the background, and the drawn ones what is drawn; anything
    # else keeps its own character.
    beach = shore and [[char(art_glyph(quarter(shore[1][v], q), q),
                             shore[0] if v < 4 else shore[2])
                        for v in range(5)] for q in range(4)]
    fringes, open_names, drawn, also, joins = fringe
    for n in (list(fringes) + open_names + list(drawn) + list(also) +
              [m for v in also.values() for m in v] + list(joins)):
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
                    c = char(art_glyph(art_q, q), mt_cols[m][q])
                elif name in fringes:
                    nth = by_name[name].index(m)
                    mask = quarter(fringes[name][min(nth, 1)][v], q)
                    own = [r[(q & 1) * 4:(q & 1) * 4 + 4]
                           for r in art[(q >> 1) * 8:(q >> 1) * 8 + 8]]
                    cut = ["".join("." if mask[y][x] == "x" else own[y][x]
                                   for x in range(4)) for y in range(8)]
                    c = char(art_glyph(cut, q), mt_cols[m][q])
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
    # A joining terrain's bit is set on it and on what it joins.
    join_bit = {t: 1 << (2 + len(extra) + k) for k, t in enumerate(sorted(joins))}
    if len(extra) + len(joins) > 6:
        sys.exit("mkoverworld: also and join: %d terrains, 6 at most"
                 % (len(extra) + len(joins)))
    slot_name = [next((n for n, c in codes.items() if c - TILE_BASE == slot),
                      None) for slot in range(TILE_SLOTS)]
    # A tile this set has no art for is drawn as its fallback, so it is the
    # fallback to its neighbours and takes the fallback's edges.
    fb = fallback or mt_name[0]
    slot_name = [n if n is None or n in by_name else fb
                 for n in slot_name]
    nflag, bsel, binv = [], [], []
    for name in slot_name:
        nflag.append((name not in ("water", "port", "bridge")) |
                     (name in open_names) << 1 | bit_of.get(name, 0) |
                     sum(b for t, b in join_bit.items()
                         if name == t or name in joins[t]))
        if name in joins:
            sel, inv = join_bit[name], 0xFF     # where it is not joined
        elif shore and name == "water":
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
            "extra": extra,
            # ... and the set itself, for tools/mkfields.py, which draws the
            # multicolour arena's fields with it.
            "palette": palette, "charlist": chars, "quads": metatiles,
            "blend": blend, "nflag": nflag, "bsel": bsel, "binv": binv,
            "mt0": mt0, "mt1": mt1, "codes": codes, "art": mt_art,
            "own_pass": own_pass,
            "cols": mt_cols, "by_name": by_name, "water": water,
            "overlays": dict(OVERLAYS), "ground": GROUND}


def write_figs(hero):
    """The classes' frames again, for the party screen (src/owtown.c): in a
    segment of their own, which its cfg puts on a sprite block's boundary
    in VIC bank 0."""
    out = ["; GENERATED from assets/monsters.txt by tools/mkoverworld.py -- "
           "do not edit.",
           "",
           "; The party's figures for the party screen (src/owtown.c): each "
           "class's",
           "; two frames, 64 bytes a block, as the engines and the fight have "
           "them,",
           "; then each class's skin, the same way (a hi-res sprite over the "
           "figure).",
           "",
           "        .export _ow_figs",
           '        .segment "OWFIGS"',
           "_ow_figs:"]
    for name, b in hero:
        out.append("        .byte " + ",".join("$%02X" % x for x in b)
                   + "   ; " + name)
    out += ["",
            "; The VIC sees the character ROM at $1000-$1FFF of bank 0.",
            "        .assert _ow_figs >= $2000 && _ow_figs + %d <= $4000, "
            "lderror, \"ow_figs: not where the VIC can see it\"" % (64 * len(hero)),
            "        .assert (_ow_figs & 63) = 0, lderror, "
            "\"ow_figs: not on a sprite block\"",
            ""]
    open(OUT_FIGS, "w").write("\n".join(out))


# Where a set's file loads: SHORE (src/ow/smooth.s), which every way into
# an area builds afresh after sm_tileset() has read the set.
SET_ADDR = 0xA000
SET_ROOM = 0x1000


def write_sets(sets, where):
    """Each set as sm_tileset reads it, a PRG file at SET_ADDR: the three
    shared colours, the numbers of characters (0 is 256), water pairs and
    metatiles, what is off the map, the water pairs, each character's colour
    RAM value, the metatiles of each tile code and of its variant, each tile
    code's neighbour flags, edge selection and inversion, the metatiles'
    characters a quarter at a time, their edges, and the characters."""
    os.makedirs(where, exist_ok=True)
    for name in SETS:
        b = bytes(sets[name]["blob"])
        if len(b) > SET_ROOM:
            sys.exit("mkoverworld: %s: %d bytes, SHORE has %d"
                     % (name, len(b), SET_ROOM))
        with open(os.path.join(where, "t." + name), "wb") as f:
            f.write(bytes([SET_ADDR & 255, SET_ADDR >> 8]) + b)
        print("mkoverworld: t.%-9s %5d bytes (%s)"
              % (name, len(b), ", ".join(sets[name]["names"])))


def main():
    sets = {name: build(name) for name in SETS}
    # The engine's text window has the character set from FONT up for its
    # letters (src/ow/smooth.s), so a set must stay below it. (Before the
    # sets are written too: a set's file can change when ow_data.s does not.)
    for name in SETS:
        if sets[name]["chars"] > FONT:
            sys.exit("mkoverworld: %s: %d characters, %d at most (the text's "
                     "font is above)" % (name, sets[name]["chars"], FONT))
    if len(sys.argv) == 3 and sys.argv[1] == "--sets":
        write_sets(sets, sys.argv[2])
        return
    ow = sets["overworld"]
    sprites, sprcols = ow["sprites"], ow["sprcols"]
    hero = figures.party()
    skins = figures.skins()
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
    # ... and the sets' own that say so: the same code in another set must
    # not be a different thing, since this table is every set's.
    for name in SETS:
        for code in sets[name]["own_pass"]:
            for other in SETS:
                if code in sets[other]["codes"].values() and \
                        code not in sets[other]["own_pass"]:
                    sys.exit("mkoverworld: $%02X is walked on in %s but "
                             "not in %s" % (code, name, other))
            passable[code - TILE_BASE] = 1

    rnd = random.Random(0x0C64)
    hashes = [rnd.randrange(256) for _ in range(256)]

    out = []
    w = out.append
    w("; GENERATED from assets/*tiles.txt and overworld.txt by "
      "tools/mkoverworld.py -- do not edit.")
    w("")
    w('        .export _ow_spr1, _ow_spr2, _ow_pass')
    w('        .export ow_rnd, ow_qvar, ow_sprites, ow_nsprites, _ow_skins')
    w("")
    w("; All of it is only read, and the same in both engines, so it goes in")
    w("; the file they share, OW-DATA (cfg/c64-openworld.cfg), whose load")
    w("; address this is.")
    w('        .import __COMMON_START__')
    w('        .segment "OWADDR"')
    w('        .word __COMMON_START__')
    w('        .segment "OWDATA"')
    w("")
    w("_ow_spr1:    .byte %d" % sprcols[0])
    w("_ow_spr2:    .byte %d" % sprcols[1])
    w("ow_nsprites: .byte %d" % (len(hero) + len(sprites)))
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
    w("; (The tile sets are files of their own: mkoverworld.py --sets.)")
    w("")
    w("; The sprites, 64 bytes a frame from block 64 (smooth.s's SPRITES): the")
    w("; hero as each class, two frames each (assets/monsters.txt, as the fight")
    w("; has them), then the set's own (assets/overworld.txt).")
    w("ow_sprites:")
    for name, b in hero + [(n, figures.block(r)) for n, r in sprites]:
        w("        .byte " + ",".join("$%02X" % x for x in b) + "   ; " + name)
    w("")
    w("; The hero's skin, hi-res, a sprite of its own in the leader's skin")
    w("; colour (tools/figures.py): each class's two frames, of which the")
    w("; engine copies the leader's into the VIC's bank (engine.c).")
    w("_ow_skins:")
    for name, b in skins:
        w("        .byte " + ",".join("$%02X" % x for x in b) + "   ; " + name)
    w("")

    open(OUT, "w").write("\n".join(out))
    write_figs(hero + skins)
    for name in SETS:
        st = sets[name]
        print("mkoverworld: %-9s %3d characters, %2d metatiles, %5d bytes"
              % (name, st["chars"], st["metatiles"], len(st["blob"])))
    print("mkoverworld: -> %s" % os.path.relpath(OUT, ROOT))


if __name__ == "__main__":
    main()
