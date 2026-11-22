/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>

#include <q4k/rle.h>

void rle_unpack(unsigned char *dst, const unsigned char *src,
                unsigned int size)
{
    const unsigned char *palette = src + 1;
    unsigned char c, n, wide = *src > 16;

    src += 1 + *src;
    while (size) {
        c = *src++;
        if (wide) {             /* more than 16 tiles: 3 bits of run, 5 of index */
            n = (c >> 5) + 1;
            c &= 0x1F;
        } else {
            n = (c >> 4) + 1;
            c &= 0x0F;          /* a byte index, which cc65 does directly */
        }
        memset(dst, palette[c], n);
        dst += n;
        size -= n;
    }
}
