/*
 * Quest for the King -- what you can see down there.
 *
 * Rogue's model rather than a full shadowcast: a room lights up when you
 * stand in it, and otherwise you see your own surroundings and straight
 * along the corridor. That is what the corridors are -- the generator only
 * cuts them along the axes -- so four rays capture them exactly, at a
 * fraction of the cost of casting at every wall.
 *
 * Squares you have seen stay on the map, drawn dim. Squares you have not
 * are simply not there.
 */

#ifndef Q4K_FOV_H
#define Q4K_FOV_H

#include <q4k/town_data.h>   /* LOCAL_W, LOCAL_H */

#define LIT_UNSEEN      0
#define LIT_REMEMBERED  1
#define LIT_VISIBLE     2

extern unsigned char lit[LOCAL_W * LOCAL_H];

/* Forget the level. Call whenever one is built. */
void fov_reset(void);

/* Recompute what is visible from (x,y). */
void fov_update(unsigned char x, unsigned char y);

#endif /* Q4K_FOV_H */
