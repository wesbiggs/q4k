/*
 * q4k -- the overworld.
 *
 * Home is a 64 by 64 map of screen codes, built when the game starts from the
 * game's seed (see isle.h) and kept under the I/O area, where only vault.s can
 * read it. Nothing wraps: the edges are sea. A continent across the water is a
 * 64 by 31 map in local[]. Both are drawn by world_view().
 *
 * Positions given to world_view() are biased by WORLD_BIAS, a multiple of both
 * their sizes, so that going a little off a map is still a small positive
 * unsigned char, which cc65 does cheaply.
 */

#ifndef Q4K_WORLD_H
#define Q4K_WORLD_H

#define WORLD_SIZE  64
#define WORLD_BIAS  64

#define SRC_HOME    0
#define SRC_ISLE    1

/* Tile at (x,y) of home; sea off the edge. */
unsigned char world_tile(unsigned char x, unsigned char y);

/* Draw the viewport of home with (x,y) at its top-left. */
void world_draw(unsigned char x, unsigned char y);

/* The viewport with (x,y) (biased) at its top-left, from home (SRC_HOME) or
   the continent in local[] (SRC_ISLE); and the window outside which it draws
   only sea. Home is never drawn from outside its own map, whatever the window
   says. See world.c; sail.c is the only user. */
void world_view(unsigned char x, unsigned char y, unsigned char src);
extern unsigned char world_x0, world_y0, world_x1, world_y1;

#endif /* Q4K_WORLD_H */
