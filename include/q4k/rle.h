/*
 * q4k -- unpacking the one-screen maps.
 *
 * Towns and battle fields are stored run-length encoded (tools/rle.py) and
 * unpacked into a RAM buffer when you arrive. Stored raw they took 3.6K of
 * the HIGH region, which had run out.
 */

#ifndef Q4K_RLE_H
#define Q4K_RLE_H

/* Unpack `size` bytes into `dst`. A byte below $80 is itself; $80 or more
   is a run of (byte & $7F) copies of the byte after it. */
void rle_unpack(unsigned char *dst, const unsigned char *src,
                unsigned int size);

#endif /* Q4K_RLE_H */
