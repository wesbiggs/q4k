#!/usr/bin/env python3
"""The open world's saved game as the disk comes with it: dist/ow-save.prg.

    tools/mkowsave.py OUT.prg [SEED] [--town ID [--at X,Y]]

A save is the open world's game block (OwGame, include/q4k/owgame.h) as it
is in memory, loaded back to $CE3F. The game writes its own over this one
(F1); until then the disk has the world of SEED (7) with a lone warrior,
Alaric, at the castle's gate, the keys to every dungeon and the three
stones in hand (as the tests' quick start has it). The castle is found with
the model of the land generator, so this stays right when the generator
changes.

--town puts him inside a town instead, for play-testing its map (`make
playtown`): an id from assets/towns.txt, or `gen` for the town the game makes
on home. He stands where the map's `$` is, or at --at's square. Leaving
puts him on home outside that town (Millford's square, for a town that is
on a continent across the sea: this is a test, not a game).
"""
import argparse
import os
import struct
import sys

import isleref
import mktiles
import mktowns

ADDR = 0xCE3F           # OWG_ADDR
MAGIC = 0x47            # OWG_MAGIC
HOME_X, HOME_Y = 128, 64
CASTLE = 1              # home's places: Millford, the castle, ...
ACTOR = 21              # sizeof(Actor), include/q4k/actor.h
LOOTED = 5              # LOOTED_BYTES
OWG_SIZE = 193          # include/q4k/owgame.h
Q_HEART = 7
KEYS = 0x3F             # IT_SIGIL ... IT_HEART: as the quick start
AREA_HOME, AREA_TOWN, AREA_CASTLE, ISLE_HOME, GATE = 0, 1, 2, 4, 0xFF
MILLFORD, HOME_TOWN = 0, 4  # home's places (isleref.HOME_PLACES' order)
CALM = 4                # OWG_CALM: nothing wanders out (a test fixture)
FOES = 4 * 6            # MAX_DEEP * OWG_FOES: the dungeons' monsters


def petscii(s):
    """As cc65 encodes a string literal: a-z to $41-$5A, A-Z to $C1-$DA."""
    out = bytearray()
    for c in s:
        if "a" <= c <= "z":
            out.append(ord(c) - 0x20)
        elif "A" <= c <= "Z":
            out.append(ord(c) + 0x80)
        else:
            out.append(ord(c))
    return bytes(out)


def actor(name, hp):
    a = struct.pack("<BBBB", 0, 0, hp, hp)
    a += petscii(name).ljust(9, b"\0")
    a += bytes([0, 0, 0, 0, 0, 0, 0, 0])   # warrior, no arms, no magic
    assert len(a) == ACTOR
    return a


def tunic(name):
    """A member's colours from their name: src/lib/ow_party.c's tunic(),
    the skin in the high four bits (light red or brown) and the clothes in
    the low, never the same."""
    h = 0
    for c in petscii(name):
        h = ((h << 1) | (h >> 7)) & 255 ^ c
    dress = 2 + h % 14
    skin = 9 if h & 0x10 else 10
    if dress == skin:
        dress += 1
    return skin << 4 | dress


def where(seed, town, at):
    """The game block's "where": area, land, place, depth, x, y, then the
    world square (wx, wy) you are on, or went into the place from."""
    places = isleref.build(seed, isleref.HOME).places
    if town is None:
        x, y = places[CASTLE]
        return [AREA_HOME, ISLE_HOME, 0, 0, GATE, 0], HOME_X + x, HOME_Y + y
    tiles = mktiles.parse("assets/tiles.txt")
    towns = mktowns.parse({t.mapchar: t for t in tiles})
    ids = [t.id for t in towns]
    if town == "gen":
        index, home_place, w, h = len(towns), HOME_TOWN, \
            mktowns.GEN_W, mktowns.GEN_H
    elif town in ids:
        index = ids.index(town)
        w, h = towns[index].w, towns[index].h
        home_place = {"millford": MILLFORD, "castle": CASTLE}.get(town,
                                                                  MILLFORD)
    else:
        sys.exit("mkowsave: no town %r; there are %s and gen"
                 % (town, ", ".join(ids)))
    x, y = GATE, 0
    if at:
        x, y = (int(v) for v in at.split(","))
        if not (0 <= x < w and 0 <= y < h):
            sys.exit("mkowsave: %d,%d is off %s's %dx%d map" % (x, y, town,
                                                               w, h))
    area = AREA_CASTLE if town == "castle" else AREA_TOWN
    px, py = places[home_place]
    return [area, ISLE_HOME, index, 0, x, y], HOME_X + px, HOME_Y + py


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("seed", nargs="?", type=int, default=7)
    ap.add_argument("--town", help="start in this town (assets/towns.txt's "
                    "id, or gen)")
    ap.add_argument("--at", help="X,Y: the square in the town to start on "
                    "(default: the map's $)")
    args = ap.parse_args()
    out = os.path.abspath(args.out)
    os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    seed = args.seed
    place, wx, wy = where(seed, args.town, args.at)
    b = actor("Alaric", 22) + bytes(ACTOR * 3)
    b += bytes([MAGIC, seed, 1, 1])                 # one member, level 1
    b += struct.pack("<HHH", 0, 9, 1)               # xp, gold, turn (not 0: a new game)
    b += bytes([1, 0, Q_HEART, KEYS]) + bytes(LOOTED)
    b += bytes(place)
    b += struct.pack("<HB", wx, wy)
    b += bytes([0, 0]) + bytes(16 + 8)  # no boat, ships moored
    b += bytes([CALM])                   # music and effects on, no monsters
    b += bytes([tunic("Alaric"), 0, 0, 0])   # each member's colour
    b += bytes(1 + 2 * FOES)             # no dungeon's monsters placed
    b += bytes([0])                      # none fled from
    assert len(b) == OWG_SIZE
    with open(out, "wb") as f:
        f.write(struct.pack("<H", ADDR))
        f.write(b)


if __name__ == "__main__":
    main()
