#!/usr/bin/env python3
"""A splash picture and its text as one file for the prototype's disk.

    tools/mksplash.py PICTURE.v2i TEXT.v2t OUT.prg

The kit in assets/splash-kit has each as a file of its own (with a load
address neither needs: they work wherever they are). One LOAD is quicker than
two, so the disk's SPLASH-P and SPLASH-N are the text, padded to a page, and
then the picture: loaded at OW_SPLASH_AT (include/q4k/ow.h), the text is there
and the picture a page on.
"""
import sys

PAGE = 256


def main():
    v2i, v2t, out = sys.argv[1:4]
    pic = open(v2i, "rb").read()[2:]
    txt = open(v2t, "rb").read()[2:]
    if pic[:3] != b"V2I" or txt[:3] != b"V2T":
        sys.exit("mksplash: %s and %s are not a .v2i and a .v2t" % (v2i, v2t))
    if len(txt) > PAGE:
        sys.exit("mksplash: %s is %d bytes, and the text has a page"
                 % (v2t, len(txt)))
    with open(out, "wb") as f:
        f.write(bytes([0x00, 0x40]))            # not used: loaded to an address
        f.write(txt.ljust(PAGE, b"\0"))
        f.write(pic)


if __name__ == "__main__":
    main()
