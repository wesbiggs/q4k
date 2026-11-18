#!/usr/bin/env python3
"""Turn the game's own font into a proportional GEOS font, for the v2t library.

The game's letters are `src/lib/font_data.c` (`tools/mkfont.py`: the
character ROM, made bold and given feet), eight by eight and monospaced.
vic2orious draws its text (`.v2t`, `lib/v2t.inc` in `assets/splash-kit`)
with a proportional font in the GEOS point-size record layout, read from a
GEOS font file in Convert format: `build --font FILE.cvt`. This writes one.

Each glyph is trimmed to the columns it has ink in and given one column of
spacing; a blank glyph (space) is four wide. That is exactly how vic2's own
`rom_font()` treats the character ROM, so the two fonts differ only in the
letters. ASCII 32-127 map to the mixed-case set's screen codes as vic2's
`screen_code()` has them; the codes the set has no glyph for (backslash,
caret, underscore, backquote, braces, bar, tilde, DEL) are drawn as spaces.

The file is a VLIR font with one point size, 8 (record 8), and a full info
block (icon, class, font ID, point size and set length tables), so GEOS
itself can use it too.

    tools/mkcvt.py [--font src/lib/font_data.c] [--out assets/q4k-serif.cvt]
"""

import argparse
import re

FIRST, COUNT = 32, 96
SPACE_ADVANCE = 4
SPACING = 1
HEIGHT = 8
BASELINE = 6            # rows above the baseline, as vic2's ROM font has it
POINT = 8
FONT_ID = 0x1F4         # 500: a GEOS font ID nothing well known uses
NAME = "Q4K Serif"
CLASS = "Q4K Serif   V1.0"   # 12 characters of name, then the version
AUTHOR = "q4k"
BLOCK = 254             # a disk block's data, which is what Convert stores


def screen_code(o):
    """ASCII -> mixed-case screen code (vic2/font.py's screen_code)."""
    if 97 <= o <= 122:
        return o - 96
    if 65 <= o <= 90 or 32 <= o <= 63:
        return o
    return {64: 0, 91: 27, 93: 29}.get(o, 32)


def read_image(path):
    text = open(path).read()
    body = text[text.index("{"):text.index("}")]
    image = bytes(int(h, 16) for h in re.findall(r"0x([0-9A-Fa-f]{2})", body))
    if len(image) != 2048:
        raise SystemExit("mkcvt: %s holds %d bytes, not a 2K charset" % (path, len(image)))
    return image


def record(image):
    """The GEOS point-size record: header, index table, bitstream."""
    rows, offsets, x = [0] * HEIGHT, [], 0
    for c in range(FIRST, FIRST + COUNT):
        g = image[screen_code(c) * 8:][:8]
        cols = [i for i in range(8) if any(r & (0x80 >> i) for r in g)]
        if cols:
            left, w = min(cols), max(cols) - min(cols) + 1 + SPACING
        else:
            left, w = 0, SPACE_ADVANCE
        offsets.append(x)
        for r in range(HEIGHT):         # w columns, ink left aligned
            rows[r] = rows[r] << w | ((g[r] << left) & 0xFF) << 8 >> (16 - w)
        x += w
    offsets.append(x)
    bpr = (x + 7) // 8
    idx = 8
    data = idx + 2 * len(offsets)
    rec = bytearray((BASELINE, bpr & 0xFF, bpr >> 8, HEIGHT,
                     idx, 0, data & 0xFF, data >> 8))
    for o in offsets:
        rec += o.to_bytes(2, "little")
    for r in rows:
        rec += (r << (8 * bpr - x)).to_bytes(bpr, "big")
    return bytes(rec), offsets, rows, bpr


def icon(offsets, rows, bpr):
    """24 by 21: a page with "Aa" on it in the font itself."""
    pix = [[0] * 24 for _ in range(21)]
    for y in range(21):
        for xx in range(24):
            if y in (0, 20) or xx in (0, 23):
                pix[y][xx] = 1
    x0 = 4
    for ch in "Aa":
        i = ord(ch) - FIRST
        o, w = offsets[i], offsets[i + 1] - offsets[i]
        for r in range(HEIGHT):
            for b in range(w):
                bit = o + b
                if rows[r] >> (8 * bpr - 1 - bit) & 1:
                    pix[7 + r][x0 + b] = 1
        x0 += w
    out = bytearray()
    for y in range(21):
        for k in range(3):
            out.append(sum(0x80 >> b for b in range(8) if pix[y][8 * k + b]))
    return bytes(out)


def petscii(s, n, pad):
    b = s.encode("ascii")[:n]
    return b + bytes([pad]) * (n - len(b))


def cvt(rec, icon_bits):
    """A VLIR file in Convert format: the directory entry and signature, the
    info block, the record table, then each record in whole blocks."""
    nblocks = (len(rec) + BLOCK - 1) // BLOCK
    last = len(rec) - (nblocks - 1) * BLOCK + 1     # index of its last byte

    dirent = bytearray(30)
    dirent[0] = 0x83                                 # USR, closed
    dirent[3:19] = petscii(NAME, 16, 0xA0)
    dirent[21] = 1                                   # VLIR
    dirent[22] = 8                                   # GEOS type: FONT
    dirent[23:28] = bytes((126, 11, 18, 12, 0))      # 2026-11-18 12:00
    total = 1 + 1 + nblocks                          # info, record table, data
    dirent[28:30] = total.to_bytes(2, "little")
    head = bytes(dirent) + b"PRG formatted GEOS file V1.0"
    head += bytes(BLOCK - len(head))

    info = bytearray(256)                            # as on disk, link bytes too
    info[0:2] = b"\x00\xff"
    info[2], info[3], info[4] = 3, 21, 0x80 | 63
    info[5:5 + 63] = icon_bits
    info[0x44], info[0x45], info[0x46] = 0x83, 8, 1
    info[0x4D:0x4D + 16] = CLASS.encode("ascii")     # then a 0: the class ends
    info[0x61:0x63] = len(rec).to_bytes(2, "little")  # set lengths, one a size
    info[0x80:0x82] = FONT_ID.to_bytes(2, "little")
    info[0x82:0x84] = (FONT_ID << 6 | POINT).to_bytes(2, "little")
    desc = b"The Quest for the King font, proportional. By " + AUTHOR.encode() + b"."
    info[0xA0:0xA0 + len(desc)] = desc
    info = bytes(info[2:])

    table = bytearray(BLOCK)
    for r in range(127):
        if r == POINT:
            table[2 * r], table[2 * r + 1] = nblocks, last
        elif r < POINT:
            table[2 * r], table[2 * r + 1] = 0, 0xFF     # an empty record
        # past the last record: 00 00, the end of the table
    return head + info + bytes(table) + rec


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--font", default="src/lib/font_data.c")
    ap.add_argument("--out", default="assets/q4k-serif.cvt")
    args = ap.parse_args()
    rec, offsets, rows, bpr = record(read_image(args.font))
    with open(args.out, "wb") as f:
        f.write(cvt(rec, icon(offsets, rows, bpr)))
    widths = [offsets[i + 1] - offsets[i] for i in range(COUNT)]
    letters = [widths[c - FIRST] for c in list(range(65, 91)) + list(range(97, 123))]
    print("mkcvt: %s, %s %d, %d bytes a row, letters %.1f pixels on average"
          % (args.out, NAME, POINT, bpr, sum(letters) / len(letters)))


if __name__ == "__main__":
    main()
