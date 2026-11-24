/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Open world: the world's overlay.
 *
 * The overworld -- Keldmarch and the continents across the sea, streamed
 * into a window round the hero as they walk and sail -- and the ships.
 * Loaded into the slot at the start, and again on the way out of a town or
 * a dungeon (engine.h); its BSS is
 * the overworld as it was left, so coming back is putting the window back
 * (from the REU, or built again from the vault) round where the hero went
 * in. Built with --code-name OVWCODE.
 */

#include <stdlib.h>
#include <c64.h>
#include <peekpoke.h>
#include <string.h>

#include <q4k/dungeon.h>
#include <q4k/isle.h>
#include <q4k/names_data.h>
#include <q4k/music.h>
#include <q4k/music_map.h>
#include <q4k/ow.h>
#include <q4k/sprite.h>
#include <q4k/tiles.h>
#include <q4k/town_data.h>

#include "engine.h"

#define SHIP_FRAME (64 + 8)     /* the ship's sprite frames, facing left and
                                   right, after the hero's six and the
                                   villager's two */

/* --- The world -----------------------------------------------------------

   Six cells across and four down, 64 squares each way, wrapping east to
   west and north to south: 384 by 256 squares. Keldmarch fills its cell;
   the four continents across the sea (64 by 31) lie across the middle of
   theirs; the rest is open sea.

        . . N . . .
        W . K . E .
        . . S . . .
        . . . . . .

   Keldmarch is built once, by the bootstrap, and kept in the RAM under the
   I/O area, at $D000, where the game keeps it. Under the KERNAL there is
   room for one continent, at $E000, since the rest is the music's (a SID
   file at $E7C0-$FFF9, as in the game): the one nearest the hero, built
   here with the game's generator as they come nearer it (kept, below).
   WORLD is a window of 64
   by 64 squares round the hero, which wraps: square (X, Y) of the world is
   at WORLD[(Y & 63) * 64 + (X & 63)]. The engine's coordinates are the
   world's taken round 128 squares, which 384 and 256 both divide, so a
   square has the same coordinates, and the same variant, wherever it is
   seen from. A step east brings in the column 32 squares ahead, over the
   one now 33 behind, and works out the edges a column back: a column or a
   row a step, done in frames with time to spare (the jobs, below). */

#define WORLD_W   384u
#define CELL_HOME 5
static const unsigned char grid[4][6] = {
    { 0, 0, 1, 0, 0, 0 },       /* the continents are d + 1, by compass */
    { 4, 0, CELL_HOME, 0, 2, 0 },
    { 0, 0, 3, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
};
#define ISLE_TOP 16             /* a continent's first row in its cell */

unsigned int  wx;               /* the hero's square in the world */
unsigned char wy;

/* Each continent's places (its story place, then its town), its biome. */
static unsigned char ipx[4][2], ipy[4][2], ibiome[4];

/* This run of the engine has the bootstrap's world (take_world()), and the
   window is to be built afresh, not put back: the first time in. */
static unsigned char taken, fresh;

/* --- The continent that is kept ------------------------------------------

   One, in the slot under the KERNAL: `kept`. The window reaches LOAD (16)
   squares from the hero and no two continents are within 65 of each other,
   so only one is ever in reach, and when another becomes the nearest the
   one kept is 33 squares away at least. That is when the other's build
   starts (seek(), after every step), in the frames' spare time
   (ow_gen.s), and it goes into the slot when it is done. If the window
   comes to need it sooner -- the continents north and south are only 17
   squares off Keldmarch's coasts -- need_isle() runs the build to its end
   there and then, and the world stands still for what is left of it. */

static unsigned char kept = NONE;       /* the continent in the slot */
static unsigned char want;              /* the one being built */

/* The middle of each continent: half its column, and its row. */
static const unsigned char isle_mx[4] = { 80, 144, 80, 16 };
static const unsigned char isle_my[4] = { 31, 95, 159, 95 };

/* Which continent the hero is nearest, by the squares to its edge, across
   or down, whichever is more (the world wraps: 192 half columns, and the
   rows as a byte does). */
static unsigned char nearest(void)
{
    unsigned char d, dx, dy, at = 0, best = 255, x = wx >> 1;

    for (d = 0; d < 4; ++d) {
        dx = x > isle_mx[d] ? x - isle_mx[d] : isle_mx[d] - x;
        if (dx > 96) {
            dx = 192 - dx;
        }
        dx = dx > 16 ? (dx - 16) << 1 : 0;
        dy = wy - isle_my[d];
        if (dy & 0x80) {
            dy = -dy;
        }
        dy = dy > 15 ? dy - 15 : 0;
        if (dy > dx) {
            dx = dy;
        }
        if (dx < best) {
            best = dx;
            at = d;
        }
    }
    return at;
}

/* A build that has finished is the continent kept. */
static void settle(void)
{
    if (gen_state == 2) {
        kept = want;
        gen_state = 0;
    }
}

/* Start continent d's build, its story place a town or a dungeon as the
   bootstrap built it (openworld.c): left alone, isle_story is 0 and the
   Drowned Stair and Blackpit come out as towns. */
static void build(unsigned char d)
{
    want = d;
    isle_story = OW->slot[d] & 1;
    gen_start(d);
}

/* After a step: the nearest continent on its way into the slot, unless it
   is there or coming. (Not while one is being copied in: the next step
   asks again.) */
static void seek(void)
{
    unsigned char d = nearest();

    settle();
    if (d != kept && gen_state != 3 && !(gen_state == 1 && want == d)) {
        build(d);
    }
}

/* Continent d in the slot before this returns. */
static void need_isle(unsigned char d)
{
    for (;;) {
        settle();
        if (gen_state != 3) {
            if (kept == d) {
                return;
            }
            if (gen_state != 1 || want != d) {
                build(d);
            }
        }
        gen_run(sm_frame);
    }
}

/* Where square (x, y) of the world is kept, or 0 for the sea; and in `run`,
   how many squares on from it, along its row (or down its column), are
   kept the same way. */
static unsigned char run;

static unsigned char *kept_at(unsigned int x, unsigned char y,
                              unsigned char down)
{
    unsigned char lx = x & 63, ly = y & 63, k = grid[y >> 6][x >> 6];

    run = 64 - (down ? ly : lx);
    if (k == CELL_HOME) {
        return (unsigned char *)0xD000 + ((unsigned int)ly << 6) + lx;
    }
    if (!k || ly >= ISLE_TOP + ISLE_H) {
        return 0;
    }
    if (ly < ISLE_TOP) {
        if (down) {
            run = ISLE_TOP - ly;
        }
        return 0;
    }
    if (down) {
        run = ISLE_TOP + ISLE_H - ly;
    }
    --k;
    if (gen_state >= 2 || kept != k) {
        need_isle(k);
    }
    return (unsigned char *)0xE000 + ((unsigned int)(ly - ISLE_TOP) << 6) + lx;
}

/* A column of the world, from one a world's width either side of it:
   cheaper than %, which is a division. */
static unsigned int wrapx(unsigned int x)
{
    return x >= WORLD_W * 2 ? x - WORLD_W * 2 : x >= WORLD_W ? x - WORLD_W : x;
}

/* n squares of the world from (x, y), along the row or down the column,
   into the window. A run never crosses a cell, so never the window's
   wrap either (both are at multiples of 64). */
static void load_run(unsigned int x, unsigned char y, unsigned char n,
                     unsigned char down)
{
    while (n) {
        vc_src = kept_at(x, y, down);
        if (!vc_src) {
            vc_src = (unsigned char *)T_WATER;  /* sm_vcopy fills with it */
        }
        vc_n = run < n ? run : n;
        vc_dst = WORLD + ((unsigned int)(y & 63) << 6) + (x & 63);
        vc_sstep = vc_dstep = down ? 64 : 1;
        sm_vcopy();
        n -= vc_n;
        if (down) {
            y += vc_n;
        } else {
            x = wrapx(x + vc_n);
        }
    }
}

/* The window is 64 by 64, but only what is LOAD round the hero is kept in
   it: the edges need a square more than theirs (EDGE_X, EDGE_Y), and a few
   steps' lag. */
#define LOAD 16
static void load_col(unsigned int x)
{
    load_run(x, wy - LOAD, LOAD * 2 + 1, 1);
}

static void load_row(unsigned char y)
{
    load_run(wrapx(wx + WORLD_W - LOAD), y, LOAD * 2 + 1, 0);
}

static void fill_window(void)
{
    unsigned char r;

    for (r = 0; r < 64; ++r) {  /* all of it, for sm_shores() */
        load_run(wrapx(wx + WORLD_W - 32), wy - 32 + r, 64, 0);
    }
}

/* --- Streaming: what a step leaves to do ----------------------------------

   Loading a column or a row, and working out the edges of the one behind
   it (in two halves, each under a frame): done one a frame, in frames with
   nothing else to build, in the order they were asked for. The window has
   room to lag: what is loaded is 32 squares off, what is seen 10. */

#define J_COL    0              /* a = the column, in the world */
#define J_ROW    1              /* a = the row */
#define J_ECOL   2              /* edges down column a, from row b: in */
#define J_EROW   5              /* thirds; along row a, from column b */
#define JOBS     32
#define EDGE_X   14             /* the edges are kept this far round the */
#define EDGE_Y   10             /* hero; the view needs 11 by 7 */
static unsigned char job_kind[JOBS];
static unsigned int  job_a[JOBS];
static unsigned char job_b[JOBS];
unsigned char job_head, job_tail;     /* not static: tests peek them */
/* How many raster lines each kind takes, at most: a guess to start with,
   and then the most that jobs() has measured. Small jobs, so that they fit
   what a frame has left. */
unsigned char job_lines[8] = { 60, 60, 90, 90, 90, 90, 90, 90 };

/* The edges of n squares of the window from (x, y) (each 0-63), down or
   across, in as many pieces as the window's wrap cuts them into. */
static void edges(unsigned char x, unsigned char y, unsigned char n,
                  unsigned char down)
{
    unsigned char m;

    while (n) {
        m = 64 - (down ? y : x);
        if (m > n) {
            m = n;
        }
        sm_sh_x0 = x;
        sm_sh_y0 = y;
        if (down) {
            sm_sh_x1 = x;
            sm_sh_y1 = y + m - 1;
            y = 0;
        } else {
            sm_sh_x1 = x + m - 1;
            sm_sh_y1 = y;
            x = 0;
        }
        sm_shores_rect();
        n -= m;
    }
}

/* One job of the window's, the oldest. A column or a row it takes in is
   wherever the window is when it runs; the edges are where it was when they
   were asked for, so that the thirds agree, and they are far enough ahead
   of the view (EDGE_X, EDGE_Y) to be a few steps late. */
static void job_run(void)
{
    unsigned char k, a, b;

    k = job_kind[job_head];
    a = (unsigned char)job_a[job_head] & 63;
    b = job_b[job_head];
    if (k == J_COL) {
        load_col(job_a[job_head]);
    } else if (k == J_ROW) {
        load_row((unsigned char)job_a[job_head]);
    } else if (k < J_EROW) {    /* 21 rows: 7 each */
        k -= J_ECOL;
        edges(a, (b + k * 7) & 63, 7, 1);
    } else {                    /* 29 columns: 10, 10, 9 */
        k -= J_EROW;
        edges((b + k * 10) & 63, a, k == 2 ? 9 : 10, 0);
    }
    job_head = (job_head + 1) % JOBS;
}

static void job_add(unsigned char kind, unsigned int a, unsigned char b)
{
    if ((job_tail + 1) % JOBS == job_head) {
        job_run();              /* behind by more than the queue holds */
    }
    job_kind[job_tail] = kind;
    job_a[job_tail] = a;
    job_b[job_tail] = b & 63;
    job_tail = (job_tail + 1) % JOBS;
}

/* Whatever of the queue fits in what is left of this frame: the raster
   lines before the interrupt at 250, which starts the next. (ov_idle: the
   core calls it at the end of every frame on the overworld.) */
void w_idle(void)
{
    unsigned int r, t, n = PEEK(0x2A6) ? 312 : 263;     /* PAL or NTSC */
    unsigned char k;

    while (job_head != job_tail && sm_frame == frame) {
        r = VIC.rasterline | ((VIC.ctrl1 & 0x80) << 1);
        t = r <= 250 ? 250 - r : 250 + n - r;
        k = job_kind[job_head];
        if (t < job_lines[k]) {
            return;
        }
        job_run();
        /* What it took, if the interrupt did not come in the middle: the
           most yet is what that kind is given from now on. */
        t = VIC.rasterline | ((VIC.ctrl1 & 0x80) << 1);
        t = t >= r ? t - r : t + n - r;
        if (sm_frame == frame && t > job_lines[k] && t < 250) {
            job_lines[k] = (unsigned char)t;
        }
    }
}

/* After a step of (dx, dy): the column and row it brings into the window,
   and the edges of the ones inside them, whose neighbours they are. */
static void stream(signed char dx, signed char dy)
{
    unsigned char k, e;

    if (dx) {
        job_add(J_COL, wrapx(dx > 0 ? wx + LOAD : wx + WORLD_W - LOAD), 0);
    }
    if (dy) {
        job_add(J_ROW, (unsigned char)(dy > 0 ? wy + LOAD : wy - LOAD), 0);
    }
    if (dx) {
        e = wx + (dx > 0 ? EDGE_X : 64 - EDGE_X);
        for (k = 0; k < 3; ++k) {
            job_add(J_ECOL + k, e, wy - EDGE_Y);
        }
    }
    if (dy) {
        e = wy + (dy > 0 ? EDGE_Y : 256 - EDGE_Y);
        for (k = 0; k < 3; ++k) {
            job_add(J_EROW + k, e, wx - EDGE_X);
        }
    }
}

/* --- Ships ----------------------------------------------------------------- */

static unsigned char ship_at(unsigned int x, unsigned char y)
{
    unsigned char i;

    for (i = 0; i < SHIPS; ++i) {
        if (ship_y[i] == y && ship_x[i] == x && boat != i + 1) {
            return i;
        }
    }
    return NONE;
}

/* Whether ship s will not sail yet: one at home's port d waits for the
   story to reach continent d. The continents' own always sail home. */
static unsigned char ship_held(unsigned char s)
{
    return s < 4 && ship_x[s] == OW->ship_x[s] && ship_y[s] == OW->ship_y[s]
        && quest_stage < Q_EMBER + OW->slot[s];
}

/* The ships within sight, up to seven (sm_npcs places them). */
static void show_ships(void)
{
    unsigned char i, n = 0;
    int dx;
    signed char dy;

    for (i = 0; i < SHIPS && n < 7; ++i) {
        if (boat == i + 1) {
            continue;
        }
        dx = (int)(ship_x[i] - wx);
        if (dx < -(int)WORLD_W / 2) {
            dx += WORLD_W;
        } else if (dx >= (int)WORLD_W / 2) {
            dx -= WORLD_W;
        }
        dy = (signed char)(ship_y[i] - wy);
        if (dx < -12 || dx > 12 || dy < -9 || dy > 9) {
            continue;
        }
        sm_npc_x[n] = ship_x[i] & 127;
        sm_npc_y[n] = ship_y[i] & 127;
        sm_npc_ptr[n] = SHIP_FRAME + 1;
        sm_npc_col[n] = COLOR_BROWN;
        ++n;
    }
    sm_npc_n = n;
}

/* The frame after a step: what it brings into the window, the ships in
   view, and which continent to have ready, every fourth step (it is 33
   squares off when it changes: see kept). Not in the frame the step ended:
   on NTSC neither has time to spare. */
void w_after(void)
{
    stream(dirdx[after - 1], dirdy[after - 1]);
    show_ships();
    sm_npcs();
    if (!(steps & 3)) {
        seek();
    }
}

/* --- The overworld --------------------------------------------------------- */

/* On the overworld, the square dx, dy from the hero: on foot, land, or a
   ship to board; in a ship, open water, or land to step off onto. */
static unsigned char world_ok(signed char dx, signed char dy)
{
    unsigned int x = wrapx(wx + WORLD_W + dx);
    unsigned char y = wy + dy, t = tile_at(x, y);

    if (t == T_WATER && ship_at(x, y) != NONE) {
        /* Board it (a ship cannot sail into one), if it may sail: a home
           port's opens when the story reaches its continent (the game's
           captains: Q_EMBER + the continent's slot). */
        return !boat && !ship_held(ship_at(x, y));
    }
    if (boat && t == T_WATER) {
        return 1;
    }
    return ow_pass[(t - 0x60) & 63];
}

/* Whether a step that way can be taken: onto open ground, and on a
   diagonal not squeezing between two corners, of sea or of mountain. */
unsigned char __fastcall__ w_can_step(unsigned char d)
{
    signed char dx = dirdx[d], dy = dirdy[d];

    if (!world_ok(dx, dy)) {
        return 0;
    }
    return !dx || !dy || world_ok(dx, 0) || world_ok(0, dy);
}

/* The overworld's music: the sea in a ship, else the land you are on. */
static void play_world(void)
{
    play_for(boat ? MUSIC_SEA :
          grid[wy >> 6][wx >> 6] == CELL_HOME ? MUSIC_HOME : MUSIC_ISLE);
}

/* The end of a step on the overworld: where the hero is in the world;
   on to a ship, or off one onto land, leaving it at the last of the water;
   and what the window has to take in. */
static void step_world(void)
{
    unsigned char s;
    static unsigned char ashore;

    wx = wrapx(wx + WORLD_W + (signed char)dirdx[dir]);
    wy += dirdy[dir];
    hx = wx & 127;
    hy = wy & 127;
    /* The land underfoot, which what wanders out is chosen by (a land's
       natives, ow_fight.c) and a save keeps: the cell's, or as it was out
       on the open sea, where nothing wanders. */
    s = grid[wy >> 6][wx >> 6];
    if (s) {
        land = s == CELL_HOME ? ISLE_HOME : s - 1;
    }
    if (!boat) {
        s = ship_at(wx, wy);
        if (s != NONE) {
            boat = s + 1;
            VIC.spr0_color = COLOR_BROWN;
            sm_sprite(hero_frame(dir, 0));
            ashore = 1;
        }
    } else if (tile_at(hx, hy) == T_WATER) {
        ship_x[boat - 1] = wx;      /* the ship goes where you sail it */
        ship_y[boat - 1] = wy;
    } else {
        boat = 0;                   /* ashore: it stays where it was */
        VIC.spr0_color = sm_tunic;
        sm_sprite(hero_frame(dir, stride));
        ashore = 1;
    }
    if (ashore) {
        /* Aboard or ashore is the only way the overworld's music changes:
           there is sea between any two lands. */
        ashore = 0;
        play_world();
        sm_blank(0);                /* (if that had to load a tune) */
    }
    after = dir + 1;            /* and the rest next frame: see main() */
}

/* --- Going in and out of places --------------------------------------- */

/* The screen off, and the overworld put away if that is where you are: the
   window finished first, since what the REU keeps it puts back, and a
   place's map must not get the overworld's jobs. A continent on its way
   into the slot is seen in or dropped: a place's map is built in local[],
   where a continent is too. */
static void w_leave(void)
{
    leave_area();
    if (area != AREA_HOME) {
        return;
    }
    if (after) {
        stream(dirdx[after - 1], dirdy[after - 1]);
        after = 0;
    }
    while (job_head != job_tail) {
        job_run();
    }
    out_wx = wx;
    out_wy = wy;
    while (gen_state == 3) {
        gen_run(sm_frame);
    }
    if (gen_state == 1) {
        gen_state = 0;          /* seek() starts it again, back outside */
    }
#ifdef OW_REU
    sm_reu_keep(1);             /* the window, edges and all, into the REU */
#endif
}

/* Into a town or a dungeon: its overlay takes over (town.c's t_enter(),
   dungeon.c's d_enter()). */
static void to_town(unsigned char index)
{
    w_leave();
    town_index = index;
    ov_want = OV_TOWN;
}

static void to_dungeon(unsigned char index)
{
    w_leave();
    dun_index = index;
    ov_want = OV_DUNGEON;
}

/* Back on the overworld where the hero went in (the screen is off): the window as it was, edges and all, from the REU,
   or built again from the vault. The first time in, from the bootstrap's
   world. */
static void back_home(void)
{
    area = AREA_HOME;
    sm_npc_n = 0;               /* the place's people stay in it */
    tileset(ts_overworld);
    sm_torus = 1;
    area_w = area_h = 64;
    wx = out_wx;
    wy = out_wy;
    play_world();
#ifdef OW_REU
    if (!fresh) {
        sm_reu_keep(0);         /* the window as it was, edges and all */
    } else {
        fill_window();
        sm_shores();            /* (show_area leaves them to the REU) */
    }
#else
    fill_window();              /* (show_area works out the edges) */
#endif
    fresh = 0;
    if (boat) {                 /* (started in one: a save, a fight) */
        VIC.spr0_color = COLOR_BROWN;
        sm_sprite(hero_frame(DIR_RIGHT, 0));
    }
    seek();
    show_ships();
    show_area(wx & 127, wy & 127);
}

/* Just loaded into the slot: the overworld, round where the hero went in
   (or starts: the core sets out_wx, out_wy from the hand-off block). */
void w_enter(void)
{
    if (!taken) {
        taken = fresh = 1;
        memcpy(ipx, OW->ipx, sizeof ipx);
        memcpy(ipy, OW->ipy, sizeof ipy);
        memcpy(ibiome, OW->ibiome, sizeof ibiome);
    }
    back_home();
}

/* The refusals' words, in the overlay (OW-DATA has little room). */
#pragma rodata-name (push, "OVWCODE")
static const char not_yet[]  = "The captain will not sail yet.";
static const char sealed[]   = "The way in is sealed.";
static const char the_heat[] = "The heat drives you back.";
static const char dug[]      = "You dig up ";
static const char dug2[]     = " gold and 2 draughts!";
#pragma rodata-name (pop)

/* A step that could not be taken: a refusal, if it was a ship that will
   not sail yet. */
void __fastcall__ w_bump(unsigned char d)
{
    unsigned int x = wrapx(wx + WORLD_W + (signed char)dirdx[d]);
    unsigned char y = wy + dirdy[d], s = ship_at(x, y);

    if (s != NONE && ship_held(s)) {
        refuse(not_yet);
    }
}

/* L: what a treasure map says is buried here (the game block's tmap[],
   which owtown's spoils fill: src/lib/ow_tmap.c), dug up and the map torn
   up, as the game's dig() has it: gold worth more as the story goes on, 16
   a stage, some five fights' worth, for the walk (and the last bit from
   where it lay, so that no two are the same and a test can know), and two
   draughts. Or nothing out here, or now and then the dead. */
#define said ((char *)0x0200)   /* (engine.c's: the line being made up) */

void w_look(void)
{
    unsigned char i, x = wx & 63, y = wy & 63;
    unsigned int t, amount;

    if (!boat) {
        if (land != ISLE_HOME) {
            y -= ISLE_TOP;
        }
        t = OWG_TMAP(land, x, y);
        for (i = 0; i < OWG_MAPS; ++i) {
            if (G->tmap[i] == t) {
                G->tmap[i] = 0;
                amount = ((unsigned int)(quest_stage + 2) << 4)
                       + ((x ^ y) & 15);
                G->gold += amount;
                G->potions += 2;
                ++G->game_turn;
                strcpy(said, dug);
                utoa(amount, said + sizeof dug - 1, 10);
                strcat(said, dug2);
                sm_ink = COLOR_YELLOW;
                sm_say(said);
                return;
            }
        }
    }
    look_nothing(boat ? NONE : tile_at(hx, hy));
}

/* Into dungeon n, if its seal is broken: the Sigil for the Hollow Crown,
   the Tidal and Ashen Keys for the others. Else a refusal, and the hero
   stays on the square. game.h's table, defined here as engine.c defines
   the others of game.c's that the engine uses (game.c is not linked). */
const unsigned char dungeon_key[NUM_DUNGEONS] = {
    IT_SIGIL, IT_TIDAL, IT_ASHEN
};

static void to_dungeon_if(unsigned char n)
{
    if (items & dungeon_key[n]) {
        to_dungeon(n);
    } else {
        refuse(sealed);
    }
}

/* At the end of a step: whatever the square does. Home's places are, in
   isle_fx's order, Millford, the castle, the Hollow Crown, Maldrath's lair
   (which is the dragon, and not here) and a town of its own. */
/* After a step onto `tile`: something wanders out, now and then, as the
   game's encounter_roll() has it (out of 200, by the ground; towns never
   roll, and nor does a ship). Not with OWG_CALM, the tests'. (Here, not in
   the core: only the overworld rolls.) */
#define T_ROAD 0x96             /* the overworld set's own (ow_roads.s) */

static void __fastcall__ wander(unsigned char tile)
{
    unsigned char chance;

    switch (tile) {
    case T_ROAD:   chance = 4;  break;  /* half the grass's: safer */
    case T_GRASS:  chance = 8;  break;
    case T_SHORE:  chance = 7;  break;
    case T_FOREST: chance = 12; break;
    case T_SWAMP:  chance = 16; break;
    case T_FLOOR:  chance = 5;  break;
    default:       return;
    }
    if (enc_always == 1) {
        chance = 200;           /* (and calm or not) */
    } else if (G->quiet & OWG_CALM) {
        return;
    }
    if ((unsigned char)(rand() % 200) < chance) {
        ow_fight_with(OWF_WANDER, tile);
    }
}

/* The place on the hero's square, if any: its name, with what it is
   (PK_TOWN or PK_DUNGEON) in pk and which in pa. Home's places are, in
   isle_fx's order, Millford, the castle, the Hollow Crown, Maldrath's
   lair (which is the dragon, and not here) and a town of its own; a
   continent's its story place (a town or a dungeon, by the seed's
   shuffle), then its own town. The names are the overlay's own copies:
   the towns' and the dungeons' are in their overlays. */
#define PK_TOWN    1
#define PK_DUNGEON 2
static unsigned char pk, pa;

#pragma rodata-name (push, "OVWCODE")
static const char pn_mill[]  = "Millford";
static const char pn_cast[]  = "Castle Matheson";
static const char pn_ward[]  = "Wardlow";
static const char pn_ash[]   = "Ashcombe";
static const char pn_crown[] = "The Hollow Crown";
static const char pn_stair[] = "The Drowned Stair";
static const char pn_pit[]   = "Blackpit";
static const char pg0[] = "Harrowby";
static const char pg1[] = "Thistledown";
static const char pg2[] = "Marrow Ford";
static const char pg3[] = "Oldcombe";
static const char pg4[] = "Saltmere";
static const char pg5[] = "Greywick";
static const char pg6[] = "Fenhallow";
static const char pg7[] = "Stonebridge";
/* names_data.h's TOWN_NAMES, which town_gen_name() picks from */
static const char *const pn_gen[TOWN_COUNT] = {
    pg0, pg1, pg2, pg3, pg4, pg5, pg6, pg7
};
static const char *const pn_town[NUM_TOWNS] = {
    pn_mill, pn_cast, pn_ward, pn_ash
};
static const char *const pn_dun[NUM_DUNGEONS] = {
    pn_crown, pn_stair, pn_pit
};
static const char to_enter[] = " (+ to enter)";
#pragma rodata-name (pop)

static const char *place_here(void)
{
    unsigned char i, k, lx, ly, d, t = tile_at(hx, hy);

    pk = 0;
    if (t != T_TOWN && t != T_CASTLE && t != T_DUNGEON) {
        return 0;
    }
    k = grid[wy >> 6][wx >> 6];
    lx = wx & 63;
    ly = wy & 63;
    if (k == CELL_HOME) {
        for (i = 0; i < 5; ++i) {
            if (home_fx[i] == lx && home_fy[i] == ly) {
                break;
            }
        }
        land = ISLE_HOME;
        switch (i) {
        case 0: pk = PK_TOWN; pa = TOWN_MILLFORD; break;
        case 1: pk = PK_TOWN; pa = TOWN_CASTLE;   break;
        case 2: pk = PK_DUNGEON; pa = 0;          break;
        case 4: pk = PK_TOWN; pa = TOWN_PROC;     break;
        }
    } else if (k) {
        k -= 1;
        ly -= ISLE_TOP;
        for (i = 0; i < 2; ++i) {
            if (ipx[k][i] == lx && ipy[k][i] == ly) {
                break;
            }
        }
        land = k;
        isle_biome = ibiome[k];
        if (i == 0) {
            d = OW->slot[k];  /* which story place */
            if (d & 1) {
                pk = PK_DUNGEON; pa = 1 + (d >> 1);
            } else {
                pk = PK_TOWN; pa = TOWN_WARDLOW + (d >> 1);
            }
        } else if (i == 1) {
            pk = PK_TOWN; pa = TOWN_PROC;
        }
    }
    if (pk == PK_DUNGEON) {
        return pn_dun[pa];
    }
    if (pk == PK_TOWN) {
        return pa < NUM_TOWNS ? pn_town[pa]
             : pn_gen[(lore_seed + TOWN_STRIDE * land) & (TOWN_COUNT - 1)];
    }
    return 0;
}

/* At the end of a step: whatever the square does. A place says its name
   and waits for + (w_use()); Maldrath's lair is a fight at once. */
void w_arrived(void)
{
    const char *name;

    step_world();
    if (tile_at(hx, hy) == T_LAIR && grid[wy >> 6][wx >> 6] == CELL_HOME) {
        /* Maldrath: with Dragonbane, until he is banished. */
        if (quest_stage < Q_VICTORY) {
            if (items & IT_BLADE) {
                ow_fight_with(OWF_DRAGON, 0);
            }
            refuse(the_heat);   /* the heat drives you back */
        }
        return;
    }
    name = place_here();
    if (name) {
        if (G->quiet & OWG_HIDE) {
            return;             /* (a message would bring the window back) */
        }
        strcpy(said, name);
        strcat(said, to_enter);
        sm_ink = COLOR_WHITE;
        sm_say(said);
    } else if (!boat) {
        wander(tile_at(hx, hy));
    }
}

/* + (or fire, k 1): into the place on the hero's square. */
void __fastcall__ w_use(unsigned char k)
{
    if (k != '-' && place_here()) {
        if (pk == PK_TOWN) {
            to_town(pa);
        } else {
            to_dungeon_if(pa);
        }
    }
}
