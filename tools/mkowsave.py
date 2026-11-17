#!/usr/bin/env python3
"""The prototype's saved game as the disk comes with it: dist/ow-save.prg.

    tools/mkowsave.py OUT.prg [SEED]

A save is the open world's game block (OwGame, include/q4k/owgame.h) as it
is in memory, loaded back to $CE40. The game writes its own over this one
(F1); until then the disk has the world of SEED (7) with a lone warrior,
Alaric, at the castle's gate, the keys to every dungeon and the three
stones in hand (as the tests' quick start has it). The castle is found with the model of the land
generator, so this stays right when the generator changes.
"""
import struct
import sys

import isleref

ADDR = 0xCE40           # OWG_ADDR
MAGIC = 0x47            # OWG_MAGIC
HOME_X, HOME_Y = 128, 64
CASTLE = 1              # home's places: Millford, the castle, ...
ACTOR = 21              # sizeof(Actor), include/q4k/actor.h
LOOTED = 5              # LOOTED_BYTES
OWG_SIZE = 143          # include/q4k/owgame.h
Q_HEART = 7
KEYS = 0x3F             # IT_SIGIL ... IT_HEART: as the quick start
AREA_HOME, ISLE_HOME, GATE = 0, 4, 0xFF
CALM = 4                # OWG_CALM: nothing wanders out (a test fixture)


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
    """A member's colour from their name: src/lib/ow_party.c's tunic()."""
    h = 0
    for c in petscii(name):
        h = ((h << 1) | (h >> 7)) & 255 ^ c
    return 2 + h % 14


def main():
    out = sys.argv[1]
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 7
    x, y = isleref.build(seed, isleref.HOME).places[CASTLE]
    wx, wy = HOME_X + x, HOME_Y + y
    b = actor("Alaric", 22) + bytes(ACTOR * 3)
    b += bytes([MAGIC, seed, 1, 1])                 # one member, level 1
    b += struct.pack("<HHH", 0, 9, 0)               # xp, gold, turn
    b += bytes([1, 0, Q_HEART, KEYS]) + bytes(LOOTED)
    b += bytes([AREA_HOME, ISLE_HOME, 0, 0, GATE, 0])
    b += struct.pack("<HB", wx, wy)
    b += bytes([0, 0]) + bytes(16 + 8)  # no boat, ships moored
    b += bytes([CALM])                   # music and effects on, no monsters
    b += bytes([tunic("Alaric"), 0, 0, 0])   # each member's colour
    assert len(b) == OWG_SIZE
    with open(out, "wb") as f:
        f.write(struct.pack("<H", ADDR))
        f.write(b)


if __name__ == "__main__":
    main()
