/*
 * q4k -- the bestiary: everything there is to fight, and how strong.
 *
 * Out of battle.c, where it began, so that whatever else stages a fight --
 * the multicolour arena (src/owarena.c, src/fight/) -- has the same monsters
 * with the same numbers. In the game it is part of the fight's engine, as it
 * was (the Makefile's ENG_FIGHT). Nothing here changed in the move.
 */

#include <c64.h>

#include <q4k/battle.h>
#include <q4k/isle.h>
#include <q4k/sprite.h>
#include <q4k/tiles.h>

/* Kept in order of `from`: a fight picks evenly from the ones you have
   reached, and battle_wander() stops at the first you have not. */
const Wanderer wanderers[] = {
    { 1, { "A rat",        4,  2, 0,  2,  2, T_RAT,      TC_RAT,      13, 1 } },
    { 1, { "A goblin",     7,  3, 1,  4,  6, T_GOBLIN,   TC_GOBLIN,   10, 1 } },
    { 2, { "A wolf",       9,  4, 1,  6,  5, T_WOLF,     TC_WOLF,     15, 1 } },
    { 2, { "A giant wasp", 5,  2, 0,  4,  0, T_WASP,     TC_WASP,     22, 1 } },
    { 3, { "A cutthroat", 12,  5, 2,  9, 15, T_BANDIT,   COLOR_RED,   10, 1 } },
    { 4, { "A bandit",    12,  5, 2,  9, 15, T_BANDIT,   TC_BANDIT,   10, 1,
           3, 6 } },
    { 4, { "A skeleton",  15,  6, 3, 12, 12, T_SKELETON, TC_SKELETON,  8, 1 } },
    { 5, { "A bone archer",
                          13,  4, 2, 14, 14, T_SKELETON, COLOR_GRAY3,  8, 1,
           5, 8 } },
    { 5, { "An ogre",     22,  8, 3, 18, 25, T_OGRE,     TC_OGRE,      7, 1 } },
    { 6, { "A wraith",    26, 10, 4, 25, 30, T_WRAITH,   TC_WRAITH,   12, 1 } },
    { 7, { "A drake",     32, 12, 5, 35, 45, T_DRAKE,    TC_DRAKE,    17, 1 } },
};
const unsigned char num_wanderers = sizeof(wanderers) / sizeof(wanderers[0]);

/* The continents' own, met nowhere else: two to a land, by what it is mostly
   made of (BIOME_* order), one that closes in and one that shoots. These are
   their strengths on the first continent the story opens; native_fight()
   makes them tougher on each one after. */
const Monster natives[BIOMES * 2] = {
    { "A gnoll",       16, 6, 2, 12, 14, T_GOBLIN,  COLOR_YELLOW,    11, 1 },
    { "A centaur",     14, 5, 2, 14, 16, T_BANDIT,  COLOR_BROWN,     14, 1,
      6, 6 },
    { "A briar troll", 20, 7, 3, 14, 12, T_OGRE,    COLOR_GREEN,      8, 1 },
    { "A wisp",        10, 4, 1, 14,  8, T_WRAITH,  COLOR_YELLOW,    16, 1,
      6, 5, SHOT_FIRE },
    { "A mire leech",  14, 6, 1, 11,  4, T_RAT,     COLOR_BROWN,      9, 1 },
    { "A bog hag",     14, 4, 2, 14, 16, T_WRAITH,  COLOR_GREEN,     10, 1,
      6, 6, SHOT_WATER },
    { "A sand wyrm",   20, 7, 3, 14, 10, T_DRAKE,   COLOR_YELLOW,    12, 1 },
    { "An efreet",     14, 5, 2, 16, 18, T_MONSTER, COLOR_ORANGE,    12, 1,
      7, 6, SHOT_FIRE },
    { "A rock troll",  22, 7, 4, 15, 14, T_OGRE,    COLOR_GRAY2,      7, 1 },
    { "A storm giant", 18, 5, 3, 16, 20, T_MONSTER, COLOR_CYAN,       9, 1,
      7, 7, SHOT_BOLT },
};

/* One keeper per deep place, in the order the quest opens them. Each is meant
   to be a near thing for a hero with the best their land sells and the level
   they are likely to have by then -- the Gaoler for the Short sword and Chain
   mail at about level 4, the Drowned King for the War axe and Platemail at
   about 6, the Pit Warden for the Runeblade and Runed plate at about 8.
   Played out in the arena (src/arena.c), each took two or three draughts,
   drinking whenever one more round could have killed. Against a party they
   are made a match by LONE_HP, LONE_ATK and their turns between the party's
   (fight()); tools/balance.py has the arena fight them out with its
   autoplayer and counts the draughts. */
const Monster keepers[NUM_DUNGEONS] = {
    { "The Gaoler",       45,  8, 5,  60,  80, T_MONSTER, TC_MONSTER, 10, 1 },
    { "The Drowned King", 85, 15, 6,  90, 120, T_MONSTER, TC_MONSTER, 10, 1,
      14, 4, SHOT_WATER },
    { "The Pit Warden",  150, 22, 8, 140, 180, T_MONSTER, TC_MONSTER, 10, 1 },
};

/* Against Dragonbane and Runed plate at level 10, five or six draughts, played
   out the same way. His health is a byte, like everyone's, so 250 is near all
   it can be; the rest is in how hard he hits. */
const Monster maldrath = {
    "Maldrath", 250, 24, 10, 255, 0, T_DRAGONTL, TC_DRAGONTL, 10, 2,
    18, 4, SHOT_FIRE
};
