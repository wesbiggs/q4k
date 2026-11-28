/* SPDX-License-Identifier: GPL-3.0-or-later */
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
    "Greatsword", "Runeblade", "Dragonbane", "Staff",
};
const unsigned char weapon_atk[NUM_WEAPONS]  = { 1, 3, 6, 10, 14, 18, 23, 28, 2 };

/* Robes go between clothes and leather: what a wizard can wear, and no
   heavier. */
const char *const armour_names[NUM_ARMOUR] = {
    "Travel clothes", "Robes", "Leather", "Chain mail", "Scale mail",
    "Platemail", "Mithril mail", "Runed plate",
};
const unsigned char armour_def[NUM_ARMOUR]  = { 0, 1, 2, 4, 7, 10, 12, 15 };
/* Each grade slows you a tenth, up to plate; mithril weighs what chain does,
   and the runes on the last plate bear some of its weight. */
const unsigned char armour_slow[NUM_ARMOUR] = { 0, 0, 1, 2, 3, 4, 2, 3 };

/* A bow hits a little softer than a blade of the same price: what you are
   paying for is not having to be next to it. */
const char *const bow_names[NUM_BOWS + 1] = {
    "No bow", "Hunting bow", "Shortbow", "Longbow", "Composite Bow",
    "Arbalest", "10 arrows",
};
const unsigned char bow_atk[NUM_BOWS]   = { 0, 2, 3, 6, 10, 15 };
const unsigned char bow_range[NUM_BOWS] = { 0, 5, 6, 10, 15, 18 };

const char *const spell_names[NUM_SPELLS] = {
    "Spark", "Heal", "Ward", "Sleep", "Bolt", "Blaze",
};
const unsigned char spell_cost[NUM_SPELLS] = { 2, 3, 3, 4, 5, 8 };

/* Two prepared at the start, one more every third level, up to five. */
unsigned char spell_slots(void)
{
    unsigned char n = 2 + party_level / 3;

    return n > 5 ? 5 : n;
}

unsigned char spells_ready(unsigned char who)
{
    unsigned char b = party[who].ready, n = 0;

    for (; b; b >>= 1) {
        n += b & 1;
    }
    return n;
}

unsigned char *gear_slot(unsigned char who, unsigned char kind)
{
    Actor *a = &party[who];

    return kind == GEAR_WEAPON ? &a->weapon
         : kind == GEAR_ARMOUR ? &a->armour : &a->bow;
}

unsigned char gear_value(unsigned char kind, unsigned char idx)
{
    return kind == GEAR_WEAPON ? weapon_atk[idx]
         : kind == GEAR_ARMOUR ? armour_def[idx] : bow_atk[idx];
}

unsigned char gear_allowed(unsigned char who, unsigned char kind,
                           unsigned char idx)
{
    unsigned char cls = party[who].cls;

    switch (kind) {
        case GEAR_WEAPON:
            return cls == CL_WARRIOR || idx <= WPN_SHORT || idx == WPN_STAFF;
        case GEAR_ARMOUR:
            return cls != CL_WIZARD || idx <= ARM_ROBES;
        default:
            return cls == CL_ARCHER || idx <= BOW_SHORT;
    }
}

unsigned char gear_better(unsigned char who, unsigned char kind,
                          unsigned char idx)
{
    return gear_allowed(who, kind, idx)
        && gear_value(kind, idx) > gear_value(kind, *gear_slot(who, kind));
}

unsigned char party_has_bow(void)
{
    unsigned char i;

    for (i = 0; i < party_count; ++i) {
        if (party[i].bow) {
            return 1;
        }
    }
    return 0;
}

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


unsigned char potion_heal(unsigned char who)
{
    unsigned int amount = ((unsigned int)party[who].hp_max * POTION_SHARE) / 100;

    return (unsigned char)(amount < POTION_FLOOR ? POTION_FLOOR : amount);
}

unsigned char drink_potion(unsigned char who)
{
    Actor *a = &party[who];
    unsigned char amount;

    if (!potions || !a->hp || a->hp == a->hp_max) {
        return 0;
    }
    --potions;
    amount = potion_heal(who);
    if (a->hp_max - a->hp < amount) {
        amount = a->hp_max - a->hp;
    }
    a->hp += amount;
    return amount;
}


