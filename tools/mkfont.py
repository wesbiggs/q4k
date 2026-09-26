#!/usr/bin/env python3
"""Build the character set the game runs on.

The C64's letters are ours to change because the game draws from a copy in
RAM, not from ROM -- that copy is also where the tiles go. This emits the
whole 2K image so the machine only has to copy it into place.

Two sources:

  --source rom          the machine's own font, optionally restyled
  --source <file.bin>   a 2K or 4K C64 charset binary, used as it is

Eight by eight is too small for real blackletter drawn by transformation --
the strokes collide and it stops being readable -- so the ROM styles here
only add weight and a foot on the stems. A font actually designed at this
size does far better, which is what a .bin source is for.

Only the mixed-case half of a 4K file is used: that is the set with both
cases, which is what the interface needs.
"""

import argparse
import os
import shutil
import sys

CHARGEN = "chargen-901225-01.bin"
SET_BYTES = 2048              # 256 glyphs of eight rows
MIXED_CASE = 2048             # offset of the mixed-case half in a 4K file

# Which codes the ROM styles touch: letters and digits. Punctuation reads
# better left alone, and $60-$7B is where the tiles land anyway.
STYLED = [(0x01, 0x1A), (0x41, 0x5A), (0x30, 0x39)]

# Glyphs the transforms spoil, left exactly as the ROM draws them.
#
# All four are the same failure: the letter is defined by narrow gaps
# between diagonals, and thickening a stroke to the right closes them. W
# becomes a solid band, M's inner V fills in until rows one to three are
# identical bars, and N loses its diagonal. The ROM already drew them as
# heavily as six pixels allow.
KEEP_AS_IS = {
    0x57, 0x17,   # W w
    0x4D, 0x0D,   # M m
    0x4E, 0x0E,   # N n
}


def bolden(rows):
    """Thicken every stroke one pixel to the right."""
    return [(r | (r >> 1)) & 0xFF for r in rows]


def runs(row):
    """The spans of set pixels in one row, as (first, last) bit positions."""
    out, start = [], None
    for b in range(8):
        if row & (0x80 >> b):
            if start is None:
                start = b
        elif start is not None:
            out.append((start, b - 1))
            start = None
    if start is not None:
        out.append((start, 7))
    return out


def serif(rows):
    """Put a foot on the end of a stem, and only there.

    The first attempt widened whatever sat in the top and bottom rows, which
    is wrong twice over. A capital N's top row is two separate stems, and
    widening each by a pixel welds them into a solid bar across the letter;
    E, T and R have a bar there already, which then overhangs the stem below
    it. It also worked on row 7, which is blank in most glyphs, so the feet
    it was supposed to add never appeared at all.

    So: find the rows that actually have ink, and only touch a span that is
    a single narrow stem with room either side. Anything ambiguous is left
    alone. That limits the effect to letters like I and T, which is honest --
    there is no room for a serif on both stems of an N at this size.
    """
    out = list(rows)
    inked = [i for i, r in enumerate(out) if r]
    if not inked:
        return out

    for edge in (inked[0], inked[-1]):
        spans = runs(out[edge])
        if len(spans) != 1:
            continue                     # two stems, or a stem and a bowl
        first, last = spans[0]
        if last - first + 1 > 2:
            continue                     # a bar, not a stem
        row = out[edge]
        if first - 1 >= 0 and not (row & (0x80 >> (first - 1))):
            row |= 0x80 >> (first - 1)
        if last + 1 <= 7 and not (row & (0x80 >> (last + 1))):
            row |= 0x80 >> (last + 1)
        out[edge] = row & 0xFF
    return out


def load(path):
    data = open(path, "rb").read()
    if len(data) == SET_BYTES:
        return bytearray(data)
    if len(data) == SET_BYTES * 2:
        return bytearray(data[MIXED_CASE:MIXED_CASE + SET_BYTES])
    sys.exit("mkfont: %s is %d bytes; expected a 2K or 4K charset"
             % (path, len(data)))


def find_rom():
    """The character ROM VICE ships, wherever this machine keeps it.

    $Q4K_CHARGEN wins; then the data directory beside whichever x64sc is on
    the PATH (a Homebrew install, or a VICE built from source in CI); then
    the usual system locations.
    """
    candidates = [os.environ.get("Q4K_CHARGEN", "")]
    x64sc = shutil.which("x64sc")
    if x64sc:
        prefix = os.path.dirname(os.path.dirname(os.path.realpath(x64sc)))
        candidates.append(os.path.join(prefix, "share/vice/C64", CHARGEN))
    for share in ("/opt/homebrew/share", "/usr/local/share", "/usr/share"):
        candidates.append(os.path.join(share, "vice/C64", CHARGEN))
    for path in candidates:
        if path and os.path.exists(path):
            return path
    sys.exit("mkfont: no character ROM (%s) found; set Q4K_CHARGEN" % CHARGEN)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source", default="rom",
                   help="'rom', or a path to a C64 charset binary")
    ap.add_argument("--style", default="bold",
                    choices=("none", "serif", "bold"),
                    help="rom source only: serif puts a foot on the stems, "
                         "bold thickens the strokes and then does the same")
    args = ap.parse_args()

    if args.source == "rom":
        image = load(find_rom())
        origin = "the character ROM, style=%s" % args.style
        if args.style != "none":
            for first, last in STYLED:
                for code in range(first, last + 1):
                    if code in KEEP_AS_IS:
                        continue
                    rows = list(image[code * 8:code * 8 + 8])
                    if args.style == "bold":
                        rows = bolden(rows)
                    rows = serif(rows)
                    image[code * 8:code * 8 + 8] = bytes(rows)
    else:
        image = load(args.source)
        origin = args.source

    with open("src/lib/font_data.c", "w") as f:
        f.write("/* GENERATED by tools/mkfont.py from %s -- do not edit.\n"
                " * The whole character set, copied into RAM at startup so\n"
                " * the tiles have somewhere to be patched over. */\n\n"
                "#include <q4k/charset.h>\n\n"
                "#pragma rodata-name (push, \"HIDATA\")\n\n"
                "const unsigned char font_image[CHARSET_BYTES] = {\n" % origin)
        for i in range(0, len(image), 16):
            f.write("    " + " ".join("0x%02X," % b for b in image[i:i + 16])
                    + "\n")
        f.write("};\n\n#pragma rodata-name (pop)\n")

    print("mkfont: 2K charset from %s" % origin)


if __name__ == "__main__":
    main()
