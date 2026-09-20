/*
 * Quest for the King -- fighting.
 *
 * One turn-based screen serves wandering monsters, the keeper at the bottom
 * of each deep place, and Maldrath himself. The caller redraws the map
 * afterwards.
 */

#ifndef Q4K_BATTLE_H
#define Q4K_BATTLE_H

#define NUM_WEAPONS 5
#define NUM_ARMOUR  5

extern const char *const   weapon_names[NUM_WEAPONS];
extern const unsigned char weapon_atk[NUM_WEAPONS];
extern const unsigned int  weapon_cost[NUM_WEAPONS];

extern const char *const   armour_names[NUM_ARMOUR];
extern const unsigned char armour_def[NUM_ARMOUR];
extern const unsigned int  armour_cost[NUM_ARMOUR];

#define BATTLE_WON   0
#define BATTLE_FLED  1
#define BATTLE_DEAD  2

/* True if crossing `tile` should bring something out of the scenery. */
unsigned char encounter_roll(unsigned char tile);

unsigned char battle_wander(void);
unsigned char battle_boss(unsigned char dungeon);
unsigned char battle_dragon(void);

void          heal_full(void);

/* Reseed the wandering-monster rolls from the machine itself. cc65's rand()
   starts from a fixed seed, so without this every playthrough meets the same
   monsters at the same squares -- and the opening stretch of that sequence
   happens to be a long quiet one. */
void          reseed(void);

#endif /* Q4K_BATTLE_H */
