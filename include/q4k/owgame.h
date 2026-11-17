/*
 * Quest for the King -- the open world's game: what lasts from one program
 * to the next.
 *
 * The open world is several programs taking turns over each other (the
 * bootstrap, the engines, the party's making, the talk ...: q4k/ow.h), so
 * the game itself -- the party, the purse, the story and where everyone
 * is -- lives in one block at a fixed address that every one of them leaves
 * alone: $CE40-$CEFF, between the engines' BSS and the probe page (every
 * cfg of the open world's keeps it out of BSS, and ld65 does not clear it).
 * A program that keeps the game's own variables (actor.c's, game.c's)
 * copies them in from here when it starts and back before it goes
 * (owg_in(), owg_out(): src/lib/owgame.c); the engines read it directly,
 * and their cfg names its party `party`.
 *
 * A saved game is this block, as it is, in OW-SAVE.
 */

#ifndef Q4K_OWGAME_H
#define Q4K_OWGAME_H

#include <q4k/actor.h>
#include <q4k/game.h>

#define OWG_ADDR  0xCE40
#define OWG_MAGIC 0x47          /* a game is made: the party, the rest */
#define OWG_SHIPS 8

/* Where you are (and the engine's `area`). */
#define AREA_HOME    0          /* the overworld */
#define AREA_TOWN    1
#define AREA_CASTLE  2
#define AREA_DUNGEON 3

#define OWG_GATE     0xFF

#define OWG_CALM     4          /* quiet: no wandering monsters (the tests'
                                   quick start and the disk's save) */       /* x: at the town's own gate, not a square */

typedef struct {
    Actor         party[MAX_PARTY];     /* first: the engines' cfg names it */
    unsigned char magic;                /* OWG_MAGIC */
    unsigned char seed;                 /* lore_seed: the lands */
    unsigned char party_count, party_level;
    unsigned int  party_xp, gold, game_turn;
    unsigned char potions, arrows, quest_stage, items;
    unsigned char looted[LOOTED_BYTES];
    /* Where: */
    unsigned char area;                 /* AREA_* */
    unsigned char land;                 /* ISLE_HOME or a compass point */
    unsigned char place;                /* the town (TOWN_*) or dungeon */
    unsigned char depth;                /* a dungeon's level */
    unsigned char x, y;                 /* in a place: its square, or GATE */
    unsigned int  wx;                   /* on the overworld: the square; */
    unsigned char wy;                   /* in a place: where you went in */
    unsigned char boat;                 /* 1 + the ship you are in, or 0 */
    unsigned char ships;                /* 1: the ships below are where */
    unsigned int  ship_x[OWG_SHIPS];    /* they were left; 0: moored */
    unsigned char ship_y[OWG_SHIPS];
    unsigned char quiet;                /* bit 0 music off, bit 1 effects,
                                           OWG_CALM nothing wanders out */
    unsigned char tunic[MAX_PARTY];     /* each member's colour: their
                                           figure's own (the hero, the party
                                           screen, a fight); from the name
                                           at first (ow_party.c), and then
                                           the party screen's to change */
} OwGame;

#define G ((OwGame *)OWG_ADDR)

/* Its size, which tools/mkowsave.py writes too: the engine checks that the
   two agree, and that it fits under the probe page. */
#define OWG_SIZE 143

/* The game's own variables from the block, and back. */
void owg_in(void);
void owg_out(void);

/* A new game into the block: the party asked for and made, at Millford's
   gate, or (quick) the tests' lone warrior outside it (src/lib/ow_party.c, for src/owparty.c). */
void ow_new_game(unsigned char quick);

/* A fight (OWF_*, as OWB has it) with the fight module (q4k/fight.h),
   loaded over the caller's own code above $4000: so the caller's code that
   runs after it must be below (src/lib/ow_fight.c is CODE). What comes of
   it goes into the game's variables -- the party, the pay, a level, a stone,
   or waking in Millford -- and is said on a screen of text. */
void __fastcall__ ow_fight(unsigned char kind, unsigned char arg);

#endif /* Q4K_OWGAME_H */
