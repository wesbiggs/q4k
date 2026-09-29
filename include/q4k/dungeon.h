/*
 * Quest for the King -- the deep places.
 *
 * Levels are generated rather than authored, which costs no ROM and means
 * the three dungeons can be as deep as they like. Generation is seeded from
 * the dungeon, the level number and the game's lore_seed, so a level looks
 * the same every time you walk back into it -- procedural, but not
 * forgetful -- and different in the next game.
 */

#ifndef Q4K_DUNGEON_H
#define Q4K_DUNGEON_H

#include <q4k/game.h>

#define MAX_ROOMS 14

/* The rooms this level ended up with. Line of sight needs them: a room you
   are standing in lights up whole, which is cheaper and truer to the genre
   than casting rays at every wall. */
extern unsigned char dun_rooms;
extern unsigned char dun_rx[MAX_ROOMS], dun_ry[MAX_ROOMS];
extern unsigned char dun_rw[MAX_ROOMS], dun_rh[MAX_ROOMS];

/* Where this level's stairs ended up, for placing the player. */
extern unsigned char dun_up_x, dun_up_y;
extern unsigned char dun_dn_x, dun_dn_y;

/* The chests this level ended up with, 1 to DUN_CHESTS of them. The index
   is which looted bit each one owns, so it must come out the same on every
   visit to the level. */
extern unsigned char dun_chests;
extern unsigned char dun_cx[DUN_CHESTS], dun_cy[DUN_CHESTS];

/* Build level `level` (1 at the mouth) of dungeon `index` into the shared
   local map. The deepest level has the keeper instead of a way down. */
void dungeon_build(unsigned char index, unsigned char level);

#endif /* Q4K_DUNGEON_H */
