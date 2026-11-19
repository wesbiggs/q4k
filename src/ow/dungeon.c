/*
 * Open world: the dungeon's overlay.
 *
 * The Hollow Crown at home, the Drowned Stair and Blackpit across the sea:
 * built a level at a time by the game's generator (dungeon_build(), whose
 * own copy is linked into this overlay: see the Makefile), its stairs going
 * down and up, and up from the first level out. Loaded into the slot over
 * the world's when the hero steps onto a dungeon (world.c's to_dungeon()),
 * and the way out asks for the world's back (engine.h). Built with
 * --code-name OVDCODE.
 *
 * Its monsters are on the map, not rolled for: OWG_FOES a level, placed
 * the first time the level is seen after the way in from the world and
 * kept in the game block (owgame.h's foe_a/foe_b) for as long as the party
 * stays in, so that a level left and come back to has the same ones where
 * they were, less those killed. Each is one of the engine's people
 * (sm_npc_*, which the multiplexer shows on sprites 3-6, as a town's
 * people are), one for however many the fight brings. One that can see
 * the hero, near enough to be on the screen with nothing but floor
 * between, comes for them after each of their steps, at its pace in the
 * arena (Monster's speed, tenths of a square a turn, a step being a turn),
 * gliding from square to square; one that reaches them, or that they walk
 * into, is a fight (ow_fight.c's OWF_FOE). The keeper, a tile, is fought
 * by walking into it.
 */

#include <c64.h>
#include <peekpoke.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <q4k/dungeon.h>
#include <q4k/game.h>
#include <q4k/isle.h>
#include <q4k/music_map.h>
#include <q4k/ow.h>
#include <q4k/tiles.h>

#include "engine.h"

/* The overlay's constants are in it (RODATA would be OW-DATA, which is
   full), and so are its variables, which are given a value so that they
   are DATA, not BSS (the core's, which is full too): they start again with
   every load, and what has to last is in the game block. Not with
   bss-name: cc65 puts a function's static locals at its start, where they
   are run as code. */
#pragma rodata-name ("OVDCODE")
#pragma data-name ("OVDCODE")

#define FOES OWG_FOES

/* What turns up, by the party's level: bestiary.c's wanderers[], each's
   `from` (foes.s has what the map shows of them). */
#define WANDERERS 11
static const unsigned char w_from[WANDERERS] = {
    1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 7
};

/* foes.s has the game block's monsters at these. */
typedef char foes_s_has_foe_a_at_cecf[
    2 * (OWG_ADDR + offsetof(OwGame, foe_a) == 0xCECF) - 1];
typedef char foes_s_has_foe_b_after[
    2 * (offsetof(OwGame, foe_b) == offsetof(OwGame, foe_a) + 24) - 1];

/* --- This level's monsters ---------------------------------------------- */

/* foes.s: the squares' part, which in C did not fit. */
extern unsigned char tx, ty;            /* where the hero ended a step */
extern unsigned char glide[FOES];       /* squares moved: its glide's pace */
unsigned char __fastcall__ foe_sees(unsigned char i);
unsigned char __fastcall__ foe_closer(unsigned char i);
unsigned char foe_glide(void);
unsigned char foe_room(void);
/* Monster i's turn (foes.s's foe_turn): if it sees the hero, as many
   squares toward them as its pace has banked (pace[], plus its speed a
   turn), gliding there; 1 if it moved, and then its square is in the game
   block, where the next run of the engine, or the next visit to the level,
   finds it. A monster that gets beside the hero sets pending. Its square
   into the game block, if it moved. */
unsigned char __fastcall__ foe_turn(unsigned char i);
/* The level's monsters (from base) onto the screen as people 0 to 5. */
void foe_muster(void);
/* The level's monsters into the game block (from base); none if !any. */
void __fastcall__ foe_spawn(unsigned char any);
extern unsigned char foe_top, foe_nat;
/* The monster next to the hero that way, or NONE. */
unsigned char __fastcall__ foe_who(unsigned char d);

/* Person i is monster base + i of the game block; one that is dead is
   at square (0, 0), which is never on the screen. */
static unsigned char think = FOES;      /* the next to take its turn */
extern unsigned char pending;           /* 1 + the one that reached them */
extern unsigned char pace[FOES];        /* tenths of a square banked */
extern unsigned char speeds[FOES];      /* its speed: tenths a turn */
extern unsigned char base;              /* the game block's first here */
static unsigned char i = 0, a = 0;      /* (-Or: statics are cheapest) */

/* The level's monsters, the first time it is seen since the way in (none
   with OWG_CALM, the tests', unless enc_always says otherwise): foes.s
   places them, of what battle_wander() would choose from. */
static void spawn(void)
{
    unsigned char top;

    a = G->party_level + depth - 1;
    for (top = 0; top < WANDERERS && w_from[top] <= a; ++top) {
    }
    foe_top = top;
    foe_nat = land != ISLE_HOME ? OWG_NATIVE + OW->ibiome[land] * 2 : 0;
    foe_spawn(!(G->quiet & OWG_CALM) || enc_always);
}

/* The level's monsters onto the screen as people: after build_area(),
   which empties the people, and before show_area(), which places them. */
static void muster(void)
{
    unsigned char m = 1 << (depth - 1);

    /* dungeon_build() seeded the dice from the level, the same every
       visit: from here on (the monsters placed, L, a chest) they are new
       with every turn, on every way onto the level, a level seen before
       and a return from a fight or a talk too. */
    srand(G->game_turn + steps + depth);
    st_chests();                /* (the chests emptied, drawn open) */
    base = (depth - 1) * FOES;
    if (!(G->foe_made & m)) {
        G->foe_made |= m;
        spawn();
    }
    foe_muster();
    think = FOES;
}

/* A fight with monster i, now (ow_fight.c's OWF_FOE). Never returns. */
static void __fastcall__ fight(unsigned char n)
{
    ow_fight_with(OWF_FOE, base + n);
}

/* --- The levels --------------------------------------------------------- */

/* Level `depth` of dungeon dun_index, standing on the stairs you came by:
   up if you came down, down if you came up. */
static void dungeon_level(unsigned char came_down)
{
    dungeon_build(dun_index, depth);
    build_area(ts_dungeon, DUN_W, DUN_H);
    muster();
    if (came_down) {
        show_area(dun_up_x + BIAS, dun_up_y + BIAS);
    } else {
        show_area(dun_dn_x + BIAS, dun_dn_y + BIAS);
    }
}

/* Just loaded into the slot: the first level of dun_index, on its way up,
   or (resuming) level depth where the hero stood. The screen is off. */
void d_enter(void)
{
    area = AREA_DUNGEON;
    play_for(MUSIC_DUNGEON);
    if (resuming) {             /* back from another program: as left */
        resuming = 0;
        dungeon_build(dun_index, depth);
        build_area(ts_dungeon, DUN_W, DUN_H);
        muster();
        show_area(resume_x + BIAS, resume_y + BIAS);
        return;
    }
    G->foe_made = G->fled = 0;  /* in from the world: all of them anew */
    depth = 1;
    dungeon_level(1);
}

/* At the end of a step: a monster that caught up, the stairs, down, up, or
   out to the world's overlay from the first level; and the monsters' turn,
   which ov_idle() takes in the frames after. */
void d_arrived(void)
{
    unsigned char t = tile_at(hx, hy);

    tx = hx;
    ty = hy;
    if (pending) {
        /* It reached the square the hero was leaving (a key held starts
           the next step before the monsters' turns): one more step after
           them, gliding there, and d_idle() starts the fight once it is
           beside them. Until then the hero cannot step (d_can_step()). */
        i = pending - 1;
        pace[i] = 10;
        foe_turn(i);
        return;
    }
    if ((t == T_STAIRDN && depth < dungeon_deep[dun_index])
        || t == T_STAIRUP) {
        memset(sm_npc_ox, 0, sizeof sm_npc_ox);  /* (a town's people are */
        memset(sm_npc_oy, 0, sizeof sm_npc_oy);  /* never off their squares) */
        leave_area();
        if (t == T_STAIRDN) {
            ++depth;
            dungeon_level(1);
        } else if (depth == 1) {
            sm_npc_n = 0;
            ov_want = OV_WORLD;
        } else {
            --depth;
            dungeon_level(0);
        }
        return;
    }
    if (think >= FOES) {
        think = 0;              /* (one still going carries on) */
    }
}

/* Whether the hero may step that way: not while a monster is coming up
   beside them for a fight. */
unsigned char __fastcall__ d_can_step(unsigned char d)
{
    return !pending && place_step(d);
}

/* Walked into: a monster, or the keeper, who does not step aside. */
void __fastcall__ d_bump(unsigned char d)
{
    i = foe_who(d);
    if (i != NONE) {
        fight(i);
    }
    if (tile_at(hx + dirdx[d], hy + dirdy[d]) == T_MONSTER) {
        ow_fight_with(OWF_KEEPER, dun_index);
    }
}

/* Every frame, with what is left of it: the monsters glide on toward their
   squares; take their turns, one at a time while the raster leaves room
   before the next frame; and one that has reached the hero, once it is
   there and the hero stands still, is a fight. */
void d_idle(void)
{
    if (!sm_npc_n) {
        return;
    }
    if (foe_glide()) {
        if (!moving) {
            sm_npcs();          /* (walking, the core does it) */
        }
    } else if (pending && !moving) {
        fight(pending - 1);
    }
    while (think < FOES && foe_room()) {
        foe_turn(think++);
    }
}

/* L: a chest on the hero's square, taken once (its looted[] bit, by the
   dungeon, the level and which chest, as the game counts them): even odds a
   draught or gold, the gold worth more with the story and the depth, as the
   game's look() has it. Anywhere else nothing, or now and then the dead. */
void d_look(void)
{
    unsigned char i, x = hx - BIAS, y = hy - BIAS;

    for (i = 0; i < dun_chests; ++i) {
        if (dun_cx[i] == x && dun_cy[i] == y) {
            if (take_bit((dun_index * MAX_DEEP + depth - 1) * DUN_CHESTS + i)) {
                if (rand() & 1) {
                    loot_found(1, 1);
                } else {
                    loot_found(0, (quest_stage << 2) + quest_stage
                                  + ((depth - 1) << 2)
                                  + 1 + (unsigned char)(rand() % 10));
                }
            }
            return;
        }
    }
    look_nothing(T_FLOOR);
}
