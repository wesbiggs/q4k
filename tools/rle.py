"""Run-length encoding for the one-screen maps, matching src/lib/rle.c.

A map opens with its palette: a count, then that many screen codes. After
that every byte is one run, its high nybble the length less one and its low
nybble an index into the palette, so a byte covers from 1 to 16 squares.

No map uses more than sixteen tiles (the busiest town has thirteen), and
runs in a 21-wide map are short: the old format -- a literal byte, or a
count and a byte for three or more -- spent two bytes on the runs that
matter and one on every lone square. This one takes the towns from 837
bytes to 598 and the battle fields from 825 to 532.
"""

import sys

MAX_RUN = 16
MAX_TILES = 16


def encode(codes):
    palette = sorted(set(codes))
    if len(palette) > MAX_TILES:
        sys.exit("rle: a map uses %d different tiles; the format holds %d"
                 % (len(palette), MAX_TILES))
    index = {c: i for i, c in enumerate(palette)}
    out, i = [len(palette)] + palette, 0
    while i < len(codes):
        c = codes[i]
        j = i
        while j < len(codes) and codes[j] == c and j - i < MAX_RUN:
            j += 1
        out.append((j - i - 1) << 4 | index[c])
        i = j
    return out


def decode(data, size):
    palette = data[1:1 + data[0]]
    out = []
    for b in data[1 + data[0]:]:
        out += [palette[b & 0x0F]] * ((b >> 4) + 1)
    assert len(out) == size
    return out


def write_c(f, name, codes, width):
    """Emit one packed map as a C array, checking it unpacks exactly."""
    packed = encode(codes)
    assert decode(packed, len(codes)) == codes
    f.write("static const unsigned char %s[%d] = {\n" % (name, len(packed)))
    for k in range(0, len(packed), width):
        f.write("    " + " ".join("0x%02X," % b for b in packed[k:k + width])
                + "\n")
    f.write("};\n")
    return len(packed)
