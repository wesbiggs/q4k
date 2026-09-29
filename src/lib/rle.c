#include <string.h>

#include <q4k/rle.h>

void rle_unpack(unsigned char *dst, const unsigned char *src,
                unsigned int size)
{
    const unsigned char *palette = src + 1;
    unsigned char c, n;

    src += 1 + *src;
    while (size) {
        c = *src++;
        n = (c >> 4) + 1;
        c &= 0x0F;              /* a byte index, which cc65 does directly */
        memset(dst, palette[c], n);
        dst += n;
        size -= n;
    }
}
