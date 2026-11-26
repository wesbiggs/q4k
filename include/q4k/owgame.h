/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Quest for the King -- the open world's game: what lasts from one program
 * to the next.
 *
 * The open world is several programs taking turns over each other (the
 * bootstrap, the engines, the party's making, the talk ...: q4k/ow.h), so
 * the game itself -- the party, the purse, the story and where everyone
 * is -- lives in one block at a fixed address that every one of them leaves
 * alone: $CE37-$CEFF, between the engines' BSS and the probe page (every
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

#define OWG_ADDR  0xCE37
#define OWG_MAGIC 0x49          /* a game is made: the party, the rest; the
                                   save's version, too */
#define OWG_MAGIC_V2 0x48       /* a save from before the treasure maps: the
                                   block without them (193 bytes), which the
                                   bootstrap takes with none */
#define OWG_MAGIC_V1 0x47       /* a save from before the party's pronouns
                                   (actor.h's PRON_*), which the bootstrap
                                   takes as they/them for everyone, and
                                   with no treasure maps */
#define OWG_SHIPS 8
#define OWG_FOES  6           /* monsters a dungeon level */
#define OWG_MAPS  4           /* treasure maps carried */

/* Where you are (and the engine's `area`). */
#define AREA_HOME    0          /* the overworld */
#define AREA_TOWN    1
#define AREA_CASTLE  2
#define AREA_DUNGEON 3

#define OWG_GATE     0xFF       /* x: at the town's own gate, not a square */

#define OWG_CALM     4          /* quiet: no wandering monsters (the tests'
                                   quick start and the disk's save) */
#define OWG_HIDE     8          /* quiet: SPACE hid the party's status
                                   under the map (src/ow/stlow.s) */

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
    unsigned char tunic[MAX_PARTY];     /* each member's colours: their
                                           clothes' in the low four bits
                                           (OWG_DRESS) and their skin's in
                                           the high (OWG_SKIN; 0 is light
                                           red, as before there was one),
                                           for the hero, the party screen
                                           and a fight; from the name at
                                           first (ow_party.c), and then the
                                           party screen's to change */
    /* The dungeon's monsters (src/ow/dungeon.c): OWG_FOES a level, placed
       the first time a level is seen after the way in from the world (a
       bit a level in foe_made, cleared on the way in) and kept, where they
       have chased the party to, until then. Each is two bytes: a, its x
       (0: none, as there was no room; a level's column 0 is wall) and the low
       two bits of its kind above it; b, its y and the kind's high three
       bits (OWG_FOE_KIND). A kind is an index into wanderers[], or 16 +
       one into natives[] (bestiary.c), or OWG_FOE_DEAD: killed, and the
       dungeon shows a bloodstain where it was. */
    unsigned char foe_made;
    unsigned char foe_a[MAX_DEEP * OWG_FOES];
    unsigned char foe_b[MAX_DEEP * OWG_FOES];
    /* The dungeon's monster the party last fled from, and how many of it
       were still standing: 1 + its index in foe_a/foe_b (OWG_FLED_WHO), and
       the count in the top three bits; 0 for none. Its next fight is with
       those, not a fresh roll; cleared on the way in from the world. */
    unsigned char fled;
    /* The treasure maps carried (src/lib/ow_tmap.c): each the square of a
       land where something is buried, OWG_TMAP(land, x, y), or 0 for none.
       x and y are the land's own (home's 64 by 64, a continent's 64 by 31),
       not the world's. Last, so that a save from before them (OWG_MAGIC_V2)
       loads as the rest of the block. */
    unsigned int  tmap[OWG_MAPS];
} OwGame;

#define OWG_TMAP(land, x, y) \
    (((unsigned int)((land) + 1) << 12) | ((unsigned int)(y) << 6) | (x))
#define OWG_TMAP_LAND(t) (((t) >> 12) - 1)    /* (of one that is not 0) */
#define OWG_TMAP_X(t)    ((unsigned char)(t) & 63)
#define OWG_TMAP_Y(t)    ((unsigned char)((t) >> 6) & 63)

#define OWG_FLED_WHO 31

#define OWG_FOE_KIND(a, b)  (((a) >> 6) | (((b) >> 5) << 2))
#define OWG_NATIVE 16           /* a kind past this is natives[kind - 16] */
#define OWG_FOE_DEAD 31         /* the kind of one killed, where it fell */

#define G ((OwGame *)OWG_ADDR)

/* What the party has seen of the dungeon they are in (src/ow/dungeon.c's
   fog): a bit a square, DUN_W / 8 bytes a row, a level after another, in
   the RAM under the KERNAL past the music's room ($E800-$FBFF), which
   every program of the open world leaves alone. Written straight through
   (a write under a ROM reaches the RAM), read back with the vault banked
   in. Not in the game block, which has no room, so not in a save: the
   bootstrap clears it, and so does the way into a dungeon from the world. */
#define OWG_FOG       0xFC00

/* And of the land (src/ow/wfog.c): a bit a square, home's 64 by 64 (512
   bytes) and then each continent's 64 by 31 (248), below the dungeons',
   in what was the music's room, the same way (and so not in a save
   either: the bootstrap clears it). Out on the open sea everything is
   seen. Right after it, at $FBE0, is where the party has been on the
   islands (q4k/isles.h's ISL_STATE), which goes into the save with it. */
#define OWG_WFOG       0xF600
#define OWG_WFOG_HOME  512
#define OWG_WFOG_ISLE  248
#define OWG_WFOG_SIZE  (OWG_WFOG_HOME + 4 * OWG_WFOG_ISLE)
#define OWG_FOG_LEVEL (DUN_W / 8 * DUN_H)
#define OWG_FOG_SIZE  (OWG_FOG_LEVEL * MAX_DEEP)

/* Its size, which tools/mkowsave.py writes too: the engine checks that the
   two agree, and that it fits under the probe page. */
#define OWG_SIZE 201

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

/* A member's two colours, from their byte of OwGame's tunic[]. */
#define OWG_DRESS(c) ((c) & 15)
#define OWG_SKIN(c)  ((c) >> 4 ? (c) >> 4 : 10)     /* 10: light red */

#endif /* Q4K_OWGAME_H */
