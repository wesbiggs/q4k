/*
 * Quest for the King -- treasure.
 *
 * Some squares hide something: a chest in the castle, a spot an innkeeper
 * saw someone digging, a chest dungeon_build() left on a level. Stand there
 * and look. Each is found once; which have been taken is looted[], and it
 * is saved with the game.
 */

#ifndef Q4K_LOOT_H
#define Q4K_LOOT_H

/* Look at the square the hero is standing on, and take whatever is there. */
void look(void);

/* A monster on the overworld may drop a map to something buried on the land
   you are standing on: the hero keeps up to MAPS of them, each giving the
   distance east or west and north or south of a town, in leagues (a league is
   a square). Called when a fight is won; says so if one drops. */
void treasure_drop(void);

/* List the maps carried, and where each one's treasure lies, until a key;
   then put the map back. */
void treasure_maps(void);

#endif /* Q4K_LOOT_H */
