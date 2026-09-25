"""Run-length encoding for the one-screen maps, matching src/lib/rle.c.

A byte below $80 is itself. A byte of $80 or more is a run: its low seven
bits are a count, and the byte after it is what to repeat. Terrain screen
codes all sit below $80, so a map needs no escaping. Runs shorter than three
are cheaper written out.
"""

import sys

MAX_RUN = 0x7F


def encode(codes):
    out, i = [], 0
    while i < len(codes):
        c = codes[i]
        if c >= 0x80:
            sys.exit("rle: screen code $%02X cannot be stored in a map" % c)
        j = i
        while j < len(codes) and codes[j] == c and j - i < MAX_RUN:
            j += 1
        n = j - i
        if n >= 3:
            out += [0x80 | n, c]
        else:
            out += [c] * n
        i = j
    return out


def decode(data, size):
    out, i = [], 0
    while len(out) < size:
        b = data[i]
        i += 1
        if b & 0x80:
            out += [data[i]] * (b & 0x7F)
            i += 1
        else:
            out.append(b)
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
