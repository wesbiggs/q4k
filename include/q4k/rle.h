/*
 * q4k -- unpacking the one-screen maps.
 *
 * Towns and battle fields are stored run-length encoded (tools/rle.py) and
 * unpacked into a RAM buffer when you arrive. Stored raw they took 3.6K of
 * the HIGH region, which had run out.
 */

#ifndef Q4K_RLE_H
#define Q4K_RLE_H

/* Unpack `size` bytes into `dst`. A map opens with a count and that many
   screen codes, its palette; each byte after that is a run, the high nybble
   its length less one and the low nybble which palette entry (with more
   than 16 in the palette, the top three bits and the low five). */
void rle_unpack(unsigned char *dst, const unsigned char *src,
                unsigned int size);

#endif /* Q4K_RLE_H */
