# SPDX-License-Identifier: GPL-3.0-or-later
"""The figures: the multicolour sprites that stand on a square, shared by
the tools that make them for the open world's programs.

assets/monsters.txt has the party's three classes and the monsters, drawn
for the arena (src/fight/); the open world (tools/mkoverworld.py) walks the
same three, and its villager, so a member looks the same on the map, on the
party screen (src/owtown.c) and in a fight. Each figure idles between two
frames, the bob: drawn once, the second frame is the first sunk a row onto
its feet, which stay where they are (bob()).

A sprite's pixels: . nothing, k the first shared colour ($D025, black), s
the second ($D026, light red: skin), t the sprite's own colour.

The party's skin is their own colour too, which a multicolour sprite has
only one of: so a member is two sprites, the figure with its skin left out
(party()) and a hi-res one of only the skin (skins()), in the member's skin
colour, at the same place. A skin's two frames are its figure's.
"""

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MONSTERS = os.path.join(ROOT, "assets", "monsters.txt")

# The party's classes, in CL_* order (include/q4k/actor.h): two frames
# each, the class's frames at 2 * cls.
CLASSES = ["warrior", "archer", "wizard"]

PIX = {".": 0, "k": 1, "t": 2, "s": 3}


def bob(rows):
    """A frame's second: sunk a row, onto feet that stay where they are."""
    return ["." * len(rows[0])] + rows[:-2] + [rows[-1]]


def read(path=MONSTERS):
    """name -> (size, [frames]), a frame being 16 * size rows of 8 * size;
    and the names in order. A name drawn once gets its bob as a second
    frame."""
    out, order = {}, []
    lines = open(path).read().split("\n")
    i = 0
    while i < len(lines):
        f = lines[i].split("#")[0].split()
        i += 1
        if not f:
            continue
        if f[0] != "sprite":
            sys.exit("%s:%d: what is %r?" % (path, i, f[0]))
        size = int(f[2]) if len(f) > 2 else 1
        rows = lines[i:i + 16 * size]
        for n, r in enumerate(rows):
            if len(r) != 8 * size or set(r) - set(PIX):
                sys.exit("%s:%d: want %d of '.kst', got %r"
                         % (path, i + n + 1, 8 * size, r))
        if len(rows) != 16 * size:
            sys.exit("%s:%d: %s: want %d rows" % (path, i, f[1], 16 * size))
        i += 16 * size
        if f[1] not in out:
            out[f[1]] = (size, [])
            order.append(f[1])
        out[f[1]][1].append(rows)
    for name in order:
        size, frames = out[name]
        if len(frames) == 1:
            frames.append(bob(frames[0]))
        elif len(frames) != 2:
            sys.exit("%s: %s: one frame or two" % (path, name))
    return out, order


def block(rows):
    """Sixteen rows of 8 as a sprite's 64 bytes."""
    data = []
    for r in rows:
        b = [0, 0]
        for k, ch in enumerate(r):
            b[k >> 2] = (b[k >> 2] << 2) | PIX[ch]
        data += [b[0], b[1], 0]
    return bytes(data + [0] * (64 - len(data)))


def bare(rows):
    """A party figure with its skin left out, for skin() to draw."""
    return [r.replace("s", ".") for r in rows]


def skin(rows, multicolour=False):
    """Only the skin, hi-res: each double-width s is two pixels set; or,
    multicolour, in the sprite's own colour (where every sprite is
    multicolour, as the fight's are)."""
    on = "t" if multicolour else "s"
    return block(["".join(on if c == "s" else "." for c in r)
                  for r in rows])


def party():
    """The classes' frames, in order, as (name, 64 bytes): six blocks,
    without their skin."""
    sprites, _ = read()
    return [(name, block(bare(fr))) for name in CLASSES
            for fr in sprites[name][1]]


def skins():
    """Each class's skin, the two frames as party()'s: six blocks."""
    sprites, _ = read()
    return [(name + " skin", skin(fr)) for name in CLASSES
            for fr in sprites[name][1]]
