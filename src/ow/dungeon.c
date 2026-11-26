/* SPDX-License-Identifier: GPL-3.0-or-later */
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
 *
 * What the party can see is the classic game's (its fov.c, Rogue's model):
 * the room they stand in, walls and all, the squares round them, and
 * straight along the corridors (which the generator cuts along the axes)
 * to the first wall. What they have seen stays on the map; what they have
 * not is dark (the set's `dark`), and so is any monster out of sight. What
 * is seen is kept for every level of the dungeon (owgame.h's OWG_FOG), so a
 * level gone back to, or come back to from a fight, is as far explored as
 * it was.
 */

#include <c64.h>
#include <peekpoke.h>
#include <stdlib.h>
#include <c64.h>
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
typedef char foes_s_has_foe_a_at_cec7[
    2 * (OWG_ADDR + offsetof(OwGame, foe_a) == 0xCEC7) - 1];
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

/* The monsters' pictures (foe_art.s), a block each in the engine's VIC
   bank, the keeper's tenth (foes.s's A_KEEPER). */
extern const unsigned char foe_art[];
#define KEEPER_ART (foe_art + 9 * 64)

/* While the fight loads, the loader's icon is the monster, doubled,
   flashing in its colour (q4k/ow.h): its own picture, which out of sight
   is not the one on its sprite (fog_foes()). */
static unsigned char fptr[FOES] = { 0 };    /* each monster's picture */

/* A fight with monster i, now (ow_fight.c's OWF_FOE). Never returns. */
static void __fastcall__ fight(unsigned char n)
{
    OW_ICON_SRC = 0x4000 + ((unsigned int)fptr[n] << 6);
    OW_ICON_COL = sm_npc_col[n];
    OW_ICON_MODE = OW_ICON_FLASH;
    ow_fight_with(OWF_FOE, base + n);
}

/* --- What the party can see -------------------------------------------- */

/* foes.s: the squares' part. fog_drawn is the level as built (WORLD's rows,
   with the chests opened and the blood, DUN_W a row), what a square is
   drawn as once it is seen (the rest is the set's `dark`); fog_seen a bit
   a square (DUN_W / 8 bytes a row), written with the level's in OWG_FOG
   (fog_at); both in OVDBSS, after the art, in no file. Squares are the
   level's, not the engine's. */
extern unsigned char fog_drawn[], fog_seen[];
extern unsigned char *fog_at;
extern unsigned char fog_ox, fog_oy;            /* the hero */
extern unsigned char fog_x0, fog_y0, fog_x1, fog_y1;       /* fog_rect's */
extern unsigned char fog_reach[4];      /* each ray's length: E, W, S, N */
void fog_near(void);    /* round the hero, and the four rays */
void fog_rect(void);    /* fog_x0-x1 by fog_y0-y1, to be seen ... */
void __fastcall__ fog_lit(unsigned char all);   /* ... a row at a time */
void fog_dark(void);    /* WORLD into fog_drawn; what is unseen, dark */
/* What is new onto the screen in front (0), or the back, past its shift
   for a flip still to come (1), while the frame has the room. */
void __fastcall__ fog_show(unsigned char back);
void fog_forget(void);  /* nothing to draw: show_area() draws it all */
/* The raster lines left in this frame for any of it; and how many fov()
   is given (it takes about 30, and more with long rays: 30 ran over the
   frame on NTSC, and more than 60 left it to the frame the step ends in,
   measured with `late`). */
unsigned char fog_left(void);
#define FOV_LINES 60

static unsigned char room = NONE;       /* the room the hero is in */
static unsigned char room_was = NONE;   /* ... and was in, lit already */

/* Room r's box, with its walls, into fog_x0-x1 by fog_y0-y1. */
static void __fastcall__ room_box(unsigned char r)
{
    fog_x0 = dun_rx[r] - 1;             /* (no room touches the edge) */
    fog_y0 = dun_ry[r] - 1;
    fog_x1 = dun_rx[r] + dun_rw[r];     /* the wall, not the floor */
    fog_y1 = dun_ry[r] + dun_rh[r];
}

/* What the hero sees from (fog_ox, fog_oy), and the boxes of what they
   had not seen before (foes.s's, for fog_show()). The room they stand in is
   lit whole only when they come into it (fog_lit(), over the frames after:
   it is a few hundred squares), since after that there is nothing new in
   it. */
static void fov(void)
{
    fog_near();
    room = NONE;
    for (i = 0; i < dun_rooms; ++i) {
        if ((unsigned char)(fog_ox - dun_rx[i]) < dun_rw[i] &&
            (unsigned char)(fog_oy - dun_ry[i]) < dun_rh[i]) {
            room = i;
        }
    }
    if (room != NONE && room != room_was) {
        fog_lit(1);             /* (the last room's rest, if any) */
        room_box(room);
        fog_rect();
    }
    room_was = room;
}

/* Whether monster n is in the hero's sight: beside them, in their room, or
   straight along a corridor short of where the ray stopped. */
static unsigned char __fastcall__ in_sight(unsigned char n)
{
    unsigned char x, y;

    if (!sm_npc_x[n]) {
        return 0;               /* dead */
    }
    x = sm_npc_x[n] - BIAS;
    y = sm_npc_y[n] - BIAS;
    if ((unsigned char)(x - fog_ox + 1) <= 2 &&
        (unsigned char)(y - fog_oy + 1) <= 2) {
        return 1;
    }
    if (room != NONE) {
        room_box(room);
        if (x >= fog_x0 && x <= fog_x1 && y >= fog_y0 && y <= fog_y1) {
            return 1;
        }
    }
    if (y == fog_oy) {
        return x > fog_ox ? x - fog_ox < fog_reach[0]
                          : fog_ox - x < fog_reach[1];
    }
    if (x == fog_ox) {
        return y > fog_oy ? y - fog_oy < fog_reach[2]
                          : fog_oy - y < fog_reach[3];
    }
    return 0;
}

/* The monsters out of sight drawn as nothing (foe_art.s's foe_blank), the
   rest as themselves. */
extern const unsigned char foe_blank[];

#define BLANK ((unsigned char)(((unsigned int)foe_blank - 0x4000) >> 6))

static void fog_foes(void)
{
    unsigned char n, p;

    if (!sm_npc_n) {
        return;                 /* nobody alive on the level */
    }
    for (n = 0; n < FOES; ++n) {
        p = in_sight(n) ? fptr[n] : BLANK;
        if (sm_npc_ptr[n] != p) {
            sm_npc_ptr[n] = p;
            sm_mxdirty = 1;     /* (the interrupt writes them) */
        }
    }
}

/* A level just built into WORLD, its monsters mustered, the hero to stand
   at engine square (x, y): what has been seen of it from OWG_FOG, the rest
   dark, and what the hero sees from there. The screen is off: show_area()
   draws it all. */
static void __fastcall__ fog_level(unsigned char x, unsigned char y)
{
    fog_at = (unsigned char *)OWG_FOG
           + ((unsigned int)(depth - 1) << 7) + ((unsigned int)(depth - 1) << 6);
    vc_src = fog_at;            /* (from under the KERNAL) */
    vc_dst = fog_seen;
    vc_n = OWG_FOG_LEVEL;
    vc_sstep = vc_dstep = 1;
    sm_vcopy();
    fog_dark();
    memcpy(fptr, sm_npc_ptr, FOES);
    fog_ox = x - BIAS;
    fog_oy = y - BIAS;
    room_was = NONE;
    fov();
    fog_lit(1);
    fog_forget();
    fog_foes();
}

/* A step under way: what the hero will see from where it ends is worked
   out in what is left of one of its frames (d_idle()), not in the frame
   that ends it, which starts the next and has no room to spare; what is
   new is drawn a row at a time while frames have the room: into the back
   screen once it is past its shift for a flip still to come (the camera
   cam_x/cam_y where the step began, two characters on at its end), which
   shows it, or into the front while the hero stands still. */
static unsigned char fog_due = 0;       /* to be worked out */
static unsigned char cam_x = 0, cam_y = 0;

static void fog_ahead(void)
{
    if (fog_due && fog_left() >= FOV_LINES) {
        fov();
        fog_due = 0;
    }
    fog_lit(0);
    if (!moving) {
        fog_show(0);
    } else if (phase != PHASE_SHIFT
               && (sm_ox != (unsigned char)(cam_x + (dirdx[dir] << 1))
                   || sm_oy != (unsigned char)(cam_y + (dirdy[dir] << 1)))) {
        fog_show(1);
    }
}

/* The end of a step: what the hero sees now, if no frame of the step had
   the room to work it out; and the monsters in sight shown, the rest not. */
static void fog_step(void)
{
    if (fog_due) {
        fov();
        fog_due = 0;
    }
    fog_foes();
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
        hx = dun_up_x + BIAS;
        hy = dun_up_y + BIAS;
    } else {
        hx = dun_dn_x + BIAS;
        hy = dun_dn_y + BIAS;
    }
    fog_level(hx, hy);
    show_area(hx, hy);
}

/* Their names, for "Entering ...": the game's dungeon_names[], in the
   overlay (the classic game's game.c had them). */
static const char dn_crown[] = "the Hollow Crown";
static const char dn_stair[] = "the Drowned Stair";
static const char dn_pit[]   = "Blackpit";
static const char dn_cave[]  = "a sea cave";
static const char *const dungeon_names[NUM_DUNGEONS + 1] = {
    dn_crown, dn_stair, dn_pit, dn_cave     /* (the islands' all) */
};

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
        fog_level(resume_x + BIAS, resume_y + BIAS);
        show_area(resume_x + BIAS, resume_y + BIAS);
        return;
    }
    G->foe_made = G->fled = 0;  /* in from the world: all of them anew */
    memset((void *)OWG_FOG, 0, OWG_FOG_SIZE);   /* and all of it unseen */
    entering = dungeon_names[dun_index < NUM_DUNGEONS ? dun_index
                                                      : NUM_DUNGEONS];
    depth = 1;
    dungeon_level(1);
}

/* At the end of a step: a monster that caught up, or the stairs, which
   say which way they go and wait for + or - (d_use()); and the monsters'
   turn, which ov_idle() takes in the frames after. */
static const char st_down[] = "Stairs down (+ to descend)";
static const char st_up[]   = "Stairs up (- to climb)";
static const char st_out[]  = "The way out (- to leave)";

void d_arrived(void)
{
    unsigned char t = tile_at(hx, hy);

    tx = hx;
    ty = hy;
    fog_step();
    if (pending) {
        /* It reached the square the hero was leaving (a key held starts
           the next step before the monsters' turns): one more step after
           them, gliding there, and d_idle() starts the fight once it is
           beside them. Until then the hero cannot step (d_can_step()). */
        i = pending - 1;
        pace[i] = 10;
        foe_turn(i);
        fog_foes();
        return;
    }
    if (G->quiet & OWG_HIDE) {
        /* (said nothing: it would bring the window back) */
    } else if (t == T_STAIRDN) {     /* (never on the bottom level) */
        sm_say(st_down);
    } else if (t == T_STAIRUP) {
        sm_say(depth == 1 ? st_out : st_up);
    }
    if (think >= FOES) {
        think = 0;              /* (one still going carries on) */
    }
}

/* + down the stairs, - up them (or out to the world's overlay from the
   first level); fire (k 1) whichever they are. Not while a monster is
   coming for a fight. */
unsigned char __fastcall__ d_use(unsigned char k)
{
    unsigned char t = tile_at(hx, hy);

    if ((unsigned char)(k - OVU_LOOK) < 4) {  /* L's (OVU_LOOK to OVU_END): */
        return !pending;        /* not while a monster comes for a fight */
    }
    if (pending) {
        return 1;               /* (and no loot) */
    }
    if (t == T_STAIRUP) {
        if (k == '+') {
            return 0;
        }
    } else if (t != T_STAIRDN || k == '-') {
        return 0;
    }
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
    return 1;
}

/* Whether the hero may step that way: not while a monster is coming up
   beside them for a fight. */
unsigned char __fastcall__ d_can_step(unsigned char d)
{
    if (pending || !place_step(d)) {
        return 0;
    }
    fog_ox = hx + dirdx[d] - BIAS;      /* (the step starts: fog_ahead()) */
    fog_oy = hy + dirdy[d] - BIAS;
    fog_due = 1;
    cam_x = sm_ox;
    cam_y = sm_oy;
    return 1;
}

/* Walked into: a monster, or the keeper, who does not step aside. */
void __fastcall__ d_bump(unsigned char d)
{
    i = foe_who(d);
    if (i != NONE) {
        fight(i);
    }
    if (tile_at(hx + dirdx[d], hy + dirdy[d]) == T_MONSTER) {
        OW_ICON_SRC = (unsigned int)KEEPER_ART;     /* (red, as drawn) */
        OW_ICON_COL = COLOR_RED;
        OW_ICON_MODE = OW_ICON_FLASH;
        ow_fight_with(OWF_KEEPER, dun_index);
    }
}

/* Every frame, with what is left of it: the monsters glide on toward their
   squares; take their turns, one at a time while the raster leaves room
   before the next frame; and one that has reached the hero, once it is
   there and the hero stands still, is a fight. Then, with what is left
   after them, what the hero sees (fog_ahead()). */
void d_idle(void)
{
    if (looking) {
        return;                 /* (L: nothing moves, nothing more seen) */
    }
    if (sm_npc_n) {
        if (foe_glide()) {
            if (!moving) {
                sm_npcs();      /* (walking, the core does it) */
            }
        } else if (pending && !moving) {
            fight(pending - 1);
        }
        while (think < FOES && foe_room()) {
            if (foe_turn(think++)) {
                fog_foes();     /* (in sight or out of it) */
            }
        }
    }
    fog_ahead();                /* (what is left of the frame after them) */
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
