#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""A speaker's portrait, a vic2orious .v2i, as a file for the open world's disk.

    tools/mkportrait.py PICTURE.v2i OUT

owtalk (src/owtalk.c, src/ow/talkpic.c) loads it at PIC_ADDR, outside VIC
bank 1, where v2i.inc unpacks it, and it has to stay there while it is
shown, since the picture's display code (its kernel) runs where it was
loaded. Room there is short, so the thumbnail and the metadata, which
nothing reads, are taken out (and their flags cleared), and the rest is
checked: a version 6 .v2i, hires, at the top left of the screen, no taller
than PIC_ROWS character rows (the talk's topics are below it) nor wider
than PIC_COLS (its words are beside it), and no longer than PIC_ROOM.
"""

import struct
import sys

PIC_ADDR = 0x8000
PIC_ROOM = 0x3000        # up to the talk's BSS at $B000 (cfg/c64-owtalk.cfg)
PIC_ROWS = 16
PIC_COLS = 24

F_META, F_MC, F_THUMB = 1, 2, 4


def main():
    src, out = sys.argv[1:3]
    raw = open(src, "rb").read()
    d = raw[2:]
    if d[:3] != b"V2I" or d[3] != 6:
        sys.exit("mkportrait: %s is not a version 6 .v2i" % src)
    cols, rows, col, row = d[4:8]
    flags = d[25]
    if flags & F_MC:
        sys.exit("mkportrait: %s is multicolour; the talk draws text on a hires one" % src)
    if col or row:
        sys.exit("mkportrait: %s is at column %d, row %d: it must be at 0, 0" % (src, col, row))
    if rows > PIC_ROWS or cols > PIC_COLS:
        sys.exit("mkportrait: %s is %dx%d cells: at most %dx%d"
                 % (src, cols, rows, PIC_COLS, PIC_ROWS))
    off = 26
    if flags & F_THUMB:
        off += 4 + 64 * d[off + 3]
    if flags & F_META:
        off += 2 + struct.unpack_from("<H", d, off)[0]
    head = bytearray(d[:26])
    head[25] = flags & ~(F_THUMB | F_META)
    body = bytes(head) + d[off:]
    if len(body) > PIC_ROOM:
        sys.exit("mkportrait: %s is %d bytes without its thumbnail and metadata: "
                 "at most %d" % (src, len(body), PIC_ROOM))
    open(out, "wb").write(struct.pack("<H", PIC_ADDR) + body)


main()
