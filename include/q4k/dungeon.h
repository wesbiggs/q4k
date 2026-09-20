/*
 * Quest for the King -- the deep places.
 *
 * Levels are generated rather than authored, which costs no ROM and means
 * the three dungeons can be as deep as they like. Generation is seeded from
 * the dungeon and the level number, so a level looks the same every time you
 * walk back into it -- procedural, but not forgetful.
 */

#ifndef Q4K_DUNGEON_H
#define Q4K_DUNGEON_H

/* Where this level's stairs ended up, for placing the player. */
extern unsigned char dun_up_x, dun_up_y;
extern unsigned char dun_dn_x, dun_dn_y;

/* Build level `level` (1 at the mouth) of dungeon `index` into the shared
   local map. The deepest level has the keeper instead of a way down. */
void dungeon_build(unsigned char index, unsigned char level);

#endif /* Q4K_DUNGEON_H */
