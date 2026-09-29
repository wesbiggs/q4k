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

#include <q4k/game.h>   /* DUN_W, DUN_H, MAX_DEEP */

/* One bit per square, eight squares to a byte, FOV_ROW bytes to a row of
   the level. Bytes rather than bits would cost 1.5K a level, which is more
   than BSS has; bits are small enough to keep a level's worth for every
   level of a dungeon. A square's byte fits in an unsigned char, and its
   row's start comes from a table rather than a multiply.

   fov_seen is every square of this level you have ever seen, drawn dim, and
   saved with the game. fov_lit is what you can see now, drawn in colour. */
#define FOV_ROW         (DUN_W / 8)
#define FOV_BYTES       (FOV_ROW * DUN_H)
#define FOV_BYTE(x, y)  ((unsigned char)(fov_row[y] + ((x) >> 3)))

extern unsigned char       fov_seen[FOV_BYTES];
extern unsigned char       fov_lit[FOV_BYTES];
extern const unsigned char fov_row[DUN_H];   /* by y: y * FOV_ROW */
extern const unsigned char fov_bit[8];       /* by x & 7 */

/* Arrive on level `level` of dungeon `dungeon`, just built.
 *
 * What you have seen is remembered for every level of the last dungeon you
 * were in, so walking back in -- down the stairs you came up, or from
 * Millford after dying down there -- shows the map you had. The level you
 * are leaving is filed away here, on arrival at the next one, not when you
 * leave: dying takes you out of a dungeon without passing through any
 * code of this module's, and fov_seen still holds that level until then.
 * Entering a different dungeon forgets the old one. */
void fov_enter(unsigned char dungeon, unsigned char level);

/* Forget everything remembered: a restored game brings only its own
   level's map. Call before fov_enter() when restoring. */
void fov_forget(void);

/* Recompute what is visible from (x,y). */
void fov_update(unsigned char x, unsigned char y);

#endif /* Q4K_FOV_H */
