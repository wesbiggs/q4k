#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""A splash picture and its text as one file for the prototype's disk.

    tools/mksplash.py PICTURE.v2i TEXT.v2t FONT.inc OUT.prg

The kit in assets/splash-kit has each as a file of its own (with a load
address neither needs: they work wherever they are). One LOAD is quicker than
two, so the disk's SPLASH-P and SPLASH-N are the text, padded to a page, and
then the picture: loaded at OW_SPLASH_AT (include/q4k/ow.h), the text is there
and the picture a page on.

The text has the first half of the page. The second is another .v2t for a disk
with no saved game, where src/lib/ow_splash.s shows it instead: the text's
first line as far as its first run of two spaces ("F1 Start New Game", without
"F3 Load Saved Game"), with spaces before it to put it in the middle of the
screen, as near as the font's space allows. Its header is the text's, so it
colours the same cells. FONT.inc is the kit's font (v2t_font_w), for the
widths.
"""
import re
import sys

PAGE = 256
HALF = PAGE // 2
SCREEN_W = 320


def widths(font_inc):
    """The font's glyph widths, from code 32 on (v2t_font_w)."""
    src = open(font_inc).read()
    m = re.search(r"v2t_font_w:[ \t]*\n((?:[ \t]*\.byte[^\n]*\n)+)", src)
    if not m:
        sys.exit("mksplash: no v2t_font_w in %s" % font_inc)
    return [int(v) for v in re.findall(r"[0-9]+", m.group(1))]


def lone(txt, w):
    """The .v2t `txt` cut to its first line's first part, centred."""
    colour, count = txt[10], txt[11]
    glyphs = txt[12:12 + count]
    cut = glyphs.find(b"  ")
    if cut >= 0:
        glyphs = glyphs[:cut]

    def width(g):
        g &= 0x7F                   # (as v2t.inc draws 128-255)
        return w[g - 0x20] if g >= 0x20 else w[0]

    left = (SCREEN_W - sum(width(g) for g in glyphs)) / 2
    glyphs = b" " * max(0, round(left / w[0])) + glyphs
    header = bytearray(txt[:10])
    header[7] = 1                   # one line
    return bytes(header) + bytes([colour, len(glyphs)]) + glyphs


def main():
    v2i, v2t, font_inc, out = sys.argv[1:5]
    pic = open(v2i, "rb").read()[2:]
    txt = open(v2t, "rb").read()[2:]
    if pic[:3] != b"V2I" or txt[:3] != b"V2T":
        sys.exit("mksplash: %s and %s are not a .v2i and a .v2t" % (v2i, v2t))
    if len(txt) > HALF:
        sys.exit("mksplash: %s is %d bytes, and the text has half a page"
                 % (v2t, len(txt)))
    one = lone(txt, widths(font_inc))
    with open(out, "wb") as f:
        f.write(bytes([0x00, 0x40]))            # not used: loaded to an address
        f.write(txt.ljust(HALF, b"\0"))
        f.write(one.ljust(HALF, b"\0"))
        f.write(pic)


if __name__ == "__main__":
    main()
