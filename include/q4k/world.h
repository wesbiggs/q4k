/*
 * q4k -- the overworld.
 *
 * A wrapping grid of screen codes. Both dimensions are powers of two, so
 * walking off an edge costs a bitmask rather than a division.
 */

#ifndef Q4K_WORLD_H
#define Q4K_WORLD_H

#include <q4k/world_data.h>

/* Tile at (x,y), wrapping. */
unsigned char world_tile(unsigned char x, unsigned char y);

/* TF_PASS / TF_BLOCK / TF_WATER / TF_ENTER for the tile at (x,y). */
unsigned char world_flag(unsigned char x, unsigned char y);

#endif /* Q4K_WORLD_H */
