/*
 * q4k -- the arms, and the bits of chance and healing everyone shares.
 *
 * Split out of battle.c so that the fight can be loaded from the disk on its
 * own (the engines: ENGINES.md) while the map, the panel and
 * the shop, which all read the arms' names and roll the same dice, keep
 * these in memory. Nothing here changed in the move.
 */

#include <stdlib.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/game.h>
#include <q4k/tiles.h>

const char *const weapon_names[NUM_WEAPONS] = {
    "Bare hands", "Dagger", "Short sword", "Long sword", "War axe",
    "Greatsword", "Runeblade", "Dragonbane",
};
const unsigned char weapon_atk[NUM_WEAPONS]  = { 1, 3, 6, 10, 14, 18, 23, 28 };

const char *const armour_names[NUM_ARMOUR] = {
    "Travel clothes", "Leather", "Chain mail", "Scale mail", "Platemail",
    "Mithril mail", "Runed plate",
};
const unsigned char armour_def[NUM_ARMOUR]  = { 0, 2, 4, 7, 10, 12, 15 };
/* Each grade slows you a tenth, up to plate; mithril weighs what chain does,
   and the runes on the last plate bear some of its weight. */
const unsigned char armour_slow[NUM_ARMOUR] = { 0, 1, 2, 3, 4, 2, 3 };

/* A bow hits a little softer than a blade of the same price: what you are
   paying for is not having to be next to it. */
const char *const bow_names[NUM_BOWS + 1] = {
    "No bow", "Shortbow", "Longbow", "Composite Bow", "Arbalest", "10 arrows",
};
const unsigned char bow_atk[NUM_BOWS]   = { 0, 3, 6, 10, 15 };
const unsigned char bow_range[NUM_BOWS] = { 0, 6, 10, 15, 18 };

/* CIA 1's timer A free-runs off the system clock; by the time a person has
   pressed a key it is unpredictable. */
#define CIA1_TIMER (*(volatile unsigned int *)0xDC04)

unsigned int last_seed;
static unsigned int entropy;

void rng_stir(unsigned int spins)
{
    /* Mix rather than replace: a slow decision and a fast one should both
       leave a mark, and the timer catches what the spin count is too coarse
       to see. */
    entropy = (entropy << 3) ^ (entropy >> 13) ^ spins ^ CIA1_TIMER;
}

void reseed(void)
{
    last_seed = entropy;
    srand(last_seed);
}

#define POTION_SHARE 40   /* per cent of full health */
#define POTION_FLOOR 12   /* ...but never less than the old flat amount */

unsigned char potion_heal(void)
{
    unsigned int amount = ((unsigned int)hero.hp_max * POTION_SHARE) / 100;

    return (unsigned char)(amount < POTION_FLOOR ? POTION_FLOOR : amount);
}

unsigned char drink_potion(void)
{
    unsigned char amount;

    if (!potions || hero.hp == hero.hp_max) {
        return 0;
    }
    --potions;
    amount = potion_heal();
    if (hero.hp_max - hero.hp < amount) {
        amount = hero.hp_max - hero.hp;
    }
    hero.hp += amount;
    return amount;
}

void heal_full(void)
{
    hero.hp = hero.hp_max;
}

unsigned char encounter_roll(unsigned char tile)
{
    unsigned char chance;

    /* Open ground is safer than the places nothing watches. */
    switch (tile) {
        case T_GRASS:  chance = 8;  break;
        case T_SHORE:  chance = 7;  break;
        case T_FOREST: chance = 12; break;
        case T_SWAMP:  chance = 16; break;
        case T_FLOOR:  chance = 5;  break;   /* dungeon corridors */
        default:       chance = 0;  break;
    }
    /* Half what it was: the odds are out of 200 now. */
    return chance && (unsigned char)(rand() % 200) < chance;
}

