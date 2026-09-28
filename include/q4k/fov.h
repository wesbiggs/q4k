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

#include <q4k/game.h>   /* LOCAL_STRIDE, DUN_W, DUN_H */

/* One bit per square, eight squares to a byte, a row of the map to every
   eight bytes: with rows LOCAL_STRIDE apart, a square's byte is a shift and
   an OR, and fits in an unsigned char. Bytes rather than bits cost 2K,
   which is more than BSS has.

   fov_seen is every square you have ever seen, drawn dim, and saved with
   the game. fov_lit is what you can see now, drawn in colour. */
#define FOV_BYTES       (LOCAL_STRIDE * LOCAL_ROWS / 8)
#define FOV_BYTE(x, y)  ((unsigned char)(((y) << (LOCAL_SHIFT - 3)) | ((x) >> 3)))

extern unsigned char       fov_seen[FOV_BYTES];
extern unsigned char       fov_lit[FOV_BYTES];
extern const unsigned char fov_bit[8];       /* by x & 7 */

/* Forget the level. Call whenever one is built. */
void fov_reset(void);

/* Recompute what is visible from (x,y). */
void fov_update(unsigned char x, unsigned char y);

#endif /* Q4K_FOV_H */
