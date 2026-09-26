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

#endif /* Q4K_LOOT_H */
