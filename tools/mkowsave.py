#!/usr/bin/env python3
"""The prototype's stand-in for a saved game: dist/ow-save.prg.

    tools/mkowsave.py OUT.prg [SEED]

Nothing saves yet, so the disk comes with one: the world of SEED (7), with
the hero at the castle's gate. It is what a save would hold as far as the
open world goes (OwSave, include/q4k/ow.h): a mark, the lands' seed and the
hero's square in the world, 384 by 256, where Keldmarch starts at (128, 64).
The castle is found with the model of the land generator, so this stays right
when the generator changes.
"""
import sys

import isleref

MAGIC = 0x53            # OW_SAVE_MAGIC
HOME_X, HOME_Y = 128, 64
CASTLE = 1              # home's places: Millford, the castle, ...


def main():
    out = sys.argv[1]
    seed = int(sys.argv[2]) if len(sys.argv) > 2 else 7
    x, y = isleref.build(seed, isleref.HOME).places[CASTLE]
    wx, wy = HOME_X + x, HOME_Y + y
    with open(out, "wb") as f:
        f.write(bytes([0x00, 0xC0]))            # not used: loaded to an address
        f.write(bytes([MAGIC, seed, wx & 255, wx >> 8, wy]))


if __name__ == "__main__":
    main()
