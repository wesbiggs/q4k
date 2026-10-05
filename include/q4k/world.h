/*
 * q4k -- the overworld.
 *
 * A wrapping grid of screen codes. Both dimensions are powers of two, so
 * walking off an edge costs a bitmask rather than a division.
 *
 * It is stored as runs, a row at a time (tools/mkmap.py), and a row is
 * unpacked into the last row of local[] when its squares are wanted. (The
 * continents are one row short of local[], for that reason.)
 */

#ifndef Q4K_WORLD_H
#define Q4K_WORLD_H

#include <q4k/world_data.h>

/* Tile at (x,y), wrapping. */
unsigned char world_tile(unsigned char x, unsigned char y);

/* Draw the viewport, centred on the hero. */
void world_draw(void);

/* The viewport with (x,y) at its top-left, on the overworld or -- with `isle`
   set -- on the continent in local[]; and the window outside which it draws
   only sea. See world.c; sail.c is the only user. */
void world_view(unsigned char x, unsigned char y, unsigned char isle);
extern unsigned char world_x0, world_y0, world_x1, world_y1;

#endif /* Q4K_WORLD_H */
