#include <string.h>

#include <q4k/rle.h>

void rle_unpack(unsigned char *dst, const unsigned char *src,
                unsigned int size)
{
    unsigned char c, n;

    while (size) {
        c = *src++;
        if (c & 0x80) {
            n = c & 0x7F;
            memset(dst, *src++, n);
            dst += n;
            size -= n;
        } else {
            *dst++ = c;
            --size;
        }
    }
}
