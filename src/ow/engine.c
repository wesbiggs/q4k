/*
 * Open world: a prototype of smooth scrolling for q4k -- its engine.
 *
 * Keldmarch and the continents across the sea, built by the game's own
 * generator from a seed, walked and sailed with the camera gliding a pixel or
 * two a frame instead of jumping a square at a time: a multicolour tile set
 * (assets/overworld.txt, 16 by 16 pixels a square) on a double-buffered
 * screen, with colour RAM rewritten ahead of the beam. smooth.s explains how.
 *
 * Step onto Millford, the castle, the Hollow Crown or a town to go in: each
 * has a tile set of its own (assets/town_tiles.txt and the rest), its people
 * are sprites, and its walls have faces.
 *
 * Built twice, as OW-REU (OW_REU: the REU shifts the screens and keeps the
 * overworld while you are in a place) and OW-CPU (the CPU does it all), and
 * loaded by the bootstrap, src/openworld.c, which builds the lands first and
 * leaves the rest of what it found in the hand-off block (q4k/ow.h). The
 * game itself is untouched: this is where the ideas are tried before they go
 * near its memory map.
 *
 *   cursor keys, WASD or a joystick in port 2   walk
 *   Q, E, Z, C (or the stick's diagonals)       walk diagonally
 *   1 / 2                                        one or two pixels a frame
 *   M                                            the music off and on
 *   N                                            the next seed's world (back
 *                                                through the bootstrap)
 *
 * For tests, through tools/screendump.py: steps can be typed (w, a, s, d),
 * stage 2 is published whenever the hero stands still with nothing typed
 * left, and --poke freeze_at=N stops the world N pixels into a step (the
 * first, or the one after freeze_after steps), at stage 3, so a
 * half-scrolled screen can be looked at.
 */

#include <c64.h>
#include <cbm.h>
#include <conio.h>
#include <peekpoke.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/dungeon.h>
#include <q4k/game.h>
#include <q4k/isle.h>
#include <q4k/music_map.h>
#include <q4k/ow.h>
#include <q4k/rle.h>
#include <q4k/town_data.h>
#include <q4k/town_gen.h>
#include <q4k/probe.h>
#include <q4k/sprite.h>
#include <q4k/tiles.h>
#include <q4k/vault.h>

#define WORLD   ((unsigned char *)0xB000)
#define BIAS    32              /* squares: smooth.s's 64 characters */

/* Where the hero's square is on the screen when the camera is still, in
   pixels from the top left of the character matrix (see smooth.s): the middle
   of the 38 by 24 window, four pixels into a character. The sprite is put
   there by sm_show(). */
#define HERO_PX 148
#define HERO_PY 84

extern unsigned char sm_ox, sm_oy, sm_fx, sm_fy, sm_front, sm_colend;
void sm_quit(void);
extern volatile unsigned char sm_frame;
void sm_init(void);
void sm_show(void);
void sm_draw_all(void);
void sm_shores(void);
void sm_animate(void);
void __fastcall__ sm_build(unsigned char dir);
void __fastcall__ sm_shift(unsigned char dir);
void __fastcall__ sm_edge(unsigned char dir);
void __fastcall__ sm_edge_row(unsigned char dir);
void __fastcall__ sm_edge_col(unsigned char dir);
void __fastcall__ sm_commit(unsigned char flip);
void __fastcall__ sm_sprite(unsigned char frame);
void __fastcall__ sm_tileset(const unsigned char *set);
void __fastcall__ sm_blank(unsigned char on);
void __fastcall__ sm_reu_keep(unsigned char out);
extern unsigned char sm_offtile;
extern unsigned char sm_npc_n, sm_npc_x[7], sm_npc_y[7];
void sm_npcs(void);
void sm_redraw_box(void);
void sm_shores_rect(void);
void sm_stack(void);
extern unsigned char sm_sh_x0, sm_sh_y0, sm_sh_x1, sm_sh_y1;
extern unsigned char sm_box_c0, sm_box_c1, sm_box_r0, sm_box_r1;
extern unsigned char sm_torus;
void sm_vcopy(void);
extern unsigned char *vc_src, *vc_dst;
extern unsigned char vc_n, vc_sstep, vc_dstep;

/* A continent built behind the scrolling (src/lib/ow_gen.s). */
void __fastcall__ gen_start(unsigned char d);
void __fastcall__ gen_run(unsigned char frame);
extern volatile unsigned char gen_state;    /* 1 building, 3 copying it into
                                               the slot, 2 there, 0 nothing */

/* The music (src/lib/ow_tune.s). */
void __fastcall__ ow_tune_play(unsigned char song);
void ow_tune_stop(void);

/* The villager's sprite frame, after the hero's eight (assets/overworld.txt),
   and the robes' colours, one a person. */
#define NPC_FRAME  (64 + 8)
#define SHIP_FRAME (64 + 9)     /* and the ship's, facing left and right */
static const unsigned char robes[7] = {
    COLOR_CYAN, COLOR_YELLOW, COLOR_PURPLE, COLOR_BLUE, COLOR_ORANGE,
    COLOR_LIGHTBLUE, COLOR_WHITE
};

/* The tile sets (src/lib/ow_data.s, from the assets' tile files), and which
   tiles can be walked onto, by tile code - $60. */
extern const unsigned char ts_overworld[], ts_town[], ts_castle[],
                           ts_dungeon[];
extern const unsigned char ow_pass[64];

/* Where you are: the overworld, or a place, each in its own tile set. A
   place is built the game's way (town_data.c's maps, town_gen(), and
   dungeon_build()) into local[], and copied into WORLD for the engine; the
   overworld is put away while you are in one -- in the REU (OW-REU), or
   else built again from the vault on the way out (OW-CPU). */
#define AREA_HOME    0
#define AREA_TOWN    1
#define AREA_CASTLE  2
#define AREA_DUNGEON 3
unsigned char area;             /* (and game.c's depth: a dungeon's level) */
static unsigned char area_w = 64, area_h = 64;
static unsigned char ex, ey;            /* a town's way out */
static unsigned char dun_index;         /* which dungeon */

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
static unsigned int out_wx;     /* where you went into a place */
static unsigned char out_wy;

/* Each continent's places (its story place, then its town), its biome. */
static unsigned char ipx[4][2], ipy[4][2], ibiome[4];

/* Ships: one beside each of Keldmarch's ports and each continent's landing,
   where they were left. Any can be taken (this is a demo); the hero becomes
   one on the water and leaves it at the last water square on landing. */
#define SHIPS 8
unsigned int  ship_x[SHIPS];
unsigned char ship_y[SHIPS];
unsigned char boat;             /* 1 + the ship you are in, or 0 */

/* What the game's own modules here (the town and dungeon generators, the
   vault) read and write of game.c's, actor.c's and the land generator's:
   defined here instead, so that none is linked, nor the view, the sprite
   and the arms they bring with them -- about 6K the prototype has no room
   for. (The land generator is linked: its own variables are its own.) */
unsigned char depth, land, local_w, local_h, quest_stage, lore_seed;
unsigned char items, looted[LOOTED_BYTES];
unsigned char home_fx[NUM_HOME_PLACES], home_fy[NUM_HOME_PLACES];
unsigned char home_biome;
const TownEnt *here_ents;
unsigned char  here_n, here_ex, here_ey;
const unsigned char dungeon_deep[NUM_DUNGEONS] = { 3, 3, 4 };
const unsigned char dungeon_prize[NUM_DUNGEONS] = { IT_EMBER, IT_TEAR, IT_HEART };
unsigned char has(unsigned char item)
{
    return items & item;        /* nothing is carried: the keeper is there */
}
Actor party[MAX_PARTY];

/* Poke-able. */
unsigned char seed;             /* lore_seed for the land (the bootstrap's) */
unsigned char speed     = 2;    /* pixels a frame: 1 or 2 */
unsigned char freeze_at = 0;    /* stop this many pixels into a step ... */
unsigned char freeze_after = 0; /* ... once this many steps are done */
unsigned char still     = 0;    /* 1: the water does not flow */
unsigned char whole     = 0;    /* 1: a roof change redraws everything, for
                                   checking that the box it does is enough */

/* What the screen is doing, for --peek. */
unsigned char hx, hy;           /* the hero's square, as the engine has it:
                                   biased by BIAS in a place, the world's
                                   round 128 on the overworld */
unsigned char late;             /* frames the main loop missed */
unsigned char shore_time;       /* jiffies sm_shores() took */
unsigned char switch_time;      /* jiffies the last way in or out took */
unsigned char roof_time;        /* jiffies the last roof change took */
unsigned int  steps;
unsigned long idle;             /* turns of the waiting loop while walking */
unsigned int  walked;           /* ... over this many frames */

#define NONE 0xFF

static unsigned char moving, dir, moved, stride;
static unsigned char pre_dir = 0xFF;   /* the back screen is shifted that way */
static unsigned char frame;     /* the last frame the main loop handled */

/* Eight ways, a square at a time: sprite.h's four, then the diagonals,
   as smooth.s numbers them. A diagonal faces the way it goes across. */
#define DIR_DL 4
#define DIR_DR 5
#define DIR_UL 6
#define DIR_UR 7
static const signed char dirdx[8] = { 0, 0, -1, 1, -1, 1, -1, 1 };
static const signed char dirdy[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
static const unsigned char facing[8] = {
    DIR_DOWN, DIR_UP, DIR_LEFT, DIR_RIGHT,
    DIR_LEFT, DIR_RIGHT, DIR_LEFT, DIR_RIGHT
};

static unsigned int camx0, camy0;   /* the camera when the step began */

/* The back screen's build, in three parts a frame apart (see sm_build):
   which is still to do. */
#define BUILT  0
#define SHIFT  1
#define ROW    2
#define COL    3
static unsigned char phase;

static void build_some(void)
{
    switch (phase) {
    case SHIFT: sm_shift(dir);    phase = ROW;   break;
    case ROW:   sm_edge_row(dir); phase = COL;   break;
    case COL:   sm_edge_col(dir); phase = BUILT; break;
    }
}

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

/* After a step: the nearest continent on its way into the slot, unless it
   is there or coming. (Not while one is being copied in: the next step
   asks again.) */
static void seek(void)
{
    unsigned char d = nearest();

    settle();
    if (d != kept && gen_state != 3 && !(gen_state == 1 && want == d)) {
        want = d;
        gen_start(d);
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
                want = d;
                gen_start(d);
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

/* n squares of the world from (x, y), along the row or down the column,
   into the window. A run never crosses a cell, so never the window's
   wrap either (both are at multiples of 64). */
/* A column of the world, from one a world's width either side of it:
   cheaper than %, which is a division. */
static unsigned int wrapx(unsigned int x)
{
    return x >= WORLD_W * 2 ? x - WORLD_W * 2 : x >= WORLD_W ? x - WORLD_W : x;
}

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
/* The step that ended last frame, + 1, until what it brings is seen to: the
   window's jobs and the ships in view. Not in the frame the step ended,
   which starts the next one too, and has no room for it on a diagonal. */
static unsigned char after;
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
   lines before the interrupt at 250, which starts the next. */
static void jobs(void)
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

/* The ships within sight, as sprites 1-7 (sm_npcs places them). */
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
        POKE(0x43F9 + n, SHIP_FRAME + 1);
        POKE(0x47F9 + n, SHIP_FRAME + 1);
        POKE(0xD028 + n, COLOR_BROWN);
        ++n;
    }
    VIC.spr_mcolor = 0xFF;
    sm_npc_n = n;
}

/* The hero's frame: walking, or a ship facing the way it last went across. */
static unsigned char ship_face = 1;

static unsigned char hero_frame(unsigned char d, unsigned char st)
{
    if (!boat) {
        return facing[d] * 2 + st;
    }
    if (facing[d] == DIR_LEFT) {
        ship_face = 0;
    } else if (facing[d] == DIR_RIGHT) {
        ship_face = 1;
    }
    return 9 + ship_face;
}

/* --- The world, from the bootstrap ------------------------------------- */

extern unsigned char _OWLOW_LOAD__[], _OWLOW_SIZE__[];

/* What the bootstrap built and found (ow.h), into the engine's own: the
   lands are in the vault already. The hero starts where it says: at
   Millford in a new game, wherever a saved one left them. */
static void take_world(void)
{
    seed = lore_seed = OW->seed;
    land       = ISLE_HOME;
    home_biome = OW->home_biome;
    memcpy(home_fx, OW->home_fx, NUM_HOME_PLACES);
    memcpy(home_fy, OW->home_fy, NUM_HOME_PLACES);
    memcpy(ipx, OW->ipx, sizeof ipx);
    memcpy(ipy, OW->ipy, sizeof ipy);
    memcpy(ibiome, OW->ibiome, sizeof ibiome);
    memcpy(ship_x, OW->ship_x, sizeof ship_x);
    memcpy(ship_y, OW->ship_y, sizeof ship_y);
    wx = OW->start_x;
    wy = OW->start_y;
    hx = wx & 127;
    hy = wy & 127;
    sm_torus = 1;
    fill_window();
    seek();
    shore_time = PEEK(0xA2);
    sm_shores();
    shore_time = PEEK(0xA2) - shore_time;
}

static void place_camera(void)
{
    /* The square's top left at (148, 84) on the screen: 2 * square - 18.5
       characters, so four pixels into one. */
    sm_ox = hx * 2 - HERO_PX / 8 - 1;
    sm_fx = 8 - (HERO_PX & 7);
    sm_oy = hy * 2 - HERO_PY / 8 - 1;
    sm_fy = 8 - (HERO_PY & 7);
}

/* What is really at a square: in a place, local[], where it was built
   (WORLD has its roofs drawn over it); on the overworld, the window. */
static unsigned char tile_at(unsigned char x, unsigned char y)
{
    if (area != AREA_HOME) {
        return local[LOCAL_AT(x - BIAS, y - BIAS)];
    }
    return WORLD[((unsigned int)(y & 63) << 6) | (x & 63)];
}

static unsigned char passable(unsigned char x, unsigned char y)
{
    unsigned char i;

    if ((unsigned char)(x - BIAS) >= area_w ||
        (unsigned char)(y - BIAS) >= area_h) {
        return 0;
    }
    for (i = 0; i < sm_npc_n; ++i) {
        if (sm_npc_x[i] == x && sm_npc_y[i] == y) {
            return 0;           /* someone is standing there */
        }
    }
    return ow_pass[(tile_at(x, y) - 0x60) & 63];
}

/* On the overworld, the square dx, dy from the hero: on foot, land, or a
   ship to board; in a ship, open water, or land to step off onto. */
static unsigned char world_ok(signed char dx, signed char dy)
{
    unsigned int x = wrapx(wx + WORLD_W + dx);
    unsigned char y = wy + dy, t = tile_at(x, y);

    if (t == T_WATER && ship_at(x, y) != NONE) {
        return !boat;           /* board it; a ship cannot sail into one */
    }
    if (boat && t == T_WATER) {
        return 1;
    }
    return ow_pass[(t - 0x60) & 63];
}

/* Whether a step that way can be taken: onto open ground, and on a
   diagonal not squeezing between two corners, of sea or of mountain. */
static unsigned char can_step(unsigned char d)
{
    signed char dx = dirdx[d], dy = dirdy[d];

    if (area == AREA_HOME) {
        if (!world_ok(dx, dy)) {
            return 0;
        }
        return !dx || !dy || world_ok(dx, 0) || world_ok(0, dy);
    }
    if (!passable(hx + dx, hy + dy)) {
        return 0;
    }
    return !dx || !dy || passable(hx + dx, hy) || passable(hx, hy + dy);
}

/* A direction held down: the stick, or a key still down in the matrix.
   Q, E, Z and C are the diagonals. */
static unsigned char held(void)
{
    unsigned char k = PEEK(0xCB), shift = PEEK(0x28D) & 1;
    unsigned char j = ~PEEK(0xDC00) & 0x0F;

    if (j == 5  || k == 62) return DIR_UL;
    if (j == 9  || k == 14) return DIR_UR;
    if (j == 6  || k == 12) return DIR_DL;
    if (j == 10 || k == 20) return DIR_DR;
    if ((j & 1) || k == 9)  return DIR_UP;
    if ((j & 2) || k == 13) return DIR_DOWN;
    if ((j & 4) || k == 10) return DIR_LEFT;
    if ((j & 8) || k == 18) return DIR_RIGHT;
    if (k == 7) return shift ? DIR_UP : DIR_DOWN;
    if (k == 2) return shift ? DIR_LEFT : DIR_RIGHT;
    return NONE;
}

/* --- The music ------------------------------------------------------------

   The game's scenes and its SID files (assets/music.txt, music_map.h): a
   scene plays a song of a file. The bootstrap loaded the file a game starts
   with; one that is another is loaded here, with the screen off and the
   interrupt left with nothing to do but the keyboard. */

static unsigned char song_now = NONE;   /* the song playing, or that would be */
static unsigned char music = 1;         /* the M key */

static void next_frame(void);

static void play_for(unsigned char s)
{
    unsigned char f = tune_file[s], i;

    if (f != OW->tune) {
        ow_tune_stop();
        OW->tune = OW_NO_TUNE;
        song_now = NONE;
        sm_blank(1);
        /* Silence has to reach VICE's sound buffer before the drive works,
           or it plays its last one for as long as that takes (CLAUDE.md). */
        for (i = 0; i < 12; ++i) {
            next_frame();
        }
        i = VIC.spr_ena;
        VIC.spr_ena = 0;        /* sprites take cycles the serial bus needs */
        if (cbm_load(tune_name[f], 8, 0)) {
            OW->tune = f;
        }
        VIC.spr_ena = i;
        frame = sm_frame;       /* those frames were not missed */
    }
    if (OW->tune == f && tune_song[s] != song_now) {
        song_now = tune_song[s];
        if (music) {
            ow_tune_play(song_now);
        }
    }
}

/* The overworld's: the sea in a ship, else the land you are on. */
static void play_world(void)
{
    play_for(boat ? MUSIC_SEA :
          grid[wy >> 6][wx >> 6] == CELL_HOME ? MUSIC_HOME : MUSIC_ISLE);
}

/* --- Going in and out of places --------------------------------------- */

static void next_frame(void)
{
    unsigned char f = sm_frame;

    while (sm_frame == f) {
    }
}

/* The screen off, and the overworld put away if that is where you are. A
   continent on its way into the slot is seen in or dropped: a place's map
   is built in local[], where a continent is too. */
#pragma code-name (push, "HICODE")  /* places, roofs, arriving: MAIN is full */
static void leave_area(void)
{
    switch_time = PEEK(0xA2);
    sm_blank(1);
    next_frame();               /* the blank has reached the screen */
    if (area == AREA_HOME) {
        /* The window finished first: what the REU keeps, it puts back,
           and a place's map must not get the overworld's jobs. */
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
            gen_state = 0;      /* seek() starts it again, back outside */
        }
#ifdef OW_REU
        sm_reu_keep(1);         /* the window, edges and all, into the REU */
#endif
    }
}

/* A place's map, from local[], into WORLD, with the people in it, in the
   tile set `set` (which says what is beyond the map's edge). */
static void build_area(const unsigned char *set, unsigned char w,
                       unsigned char h)
{
    unsigned char y;

    sm_npc_n = 0;               /* nobody, until add_people() */
    sm_torus = 0;               /* a place has edges */
    sm_tileset(set);
    memset(WORLD, sm_offtile, 64 * 64);
    for (y = 0; y < h; ++y) {
        memcpy(WORLD + (unsigned int)y * 64, local + LOCAL_AT(0, y), w);
    }
    area_w = w;
    area_h = h;
}

/* --- Roofs -------------------------------------------------------------

   In a town or the castle, a building you are not in shows only its roof,
   as in Ultima VI. A building is a closed area with a door: found once on
   the way in, by filling from where you stand (the outside, region 1)
   through everything that is not a wall or a door, then again from each
   square not yet reached. Its roof covers it and the walls round it, with a
   ridge along its middle row, and hides the people in it; the doors stay.
   Stepping into its doorway (or onto its floor) takes its roof off and puts
   back whichever came off before, drawn into the hidden screen and flipped
   to, so it changes between one frame and the next. A door between two
   rooms keeps open the one already open. What you can walk on is read from local[],
   which has no roofs. */

#define T_ROOF   0x92           /* the tile sets' own codes (mkoverworld.py) */
#define T_RIDGE  0x93
#define MAX_BLD  32
#define AT(x, y) ((unsigned int)(y) * LOCAL_W + (x))

static unsigned char region[LOCAL_W * LOCAL_H];        /* 0 a wall or door */
/* For a wall, the buildings beside it (up to two, 0 for none): it is under
   a roof if either is. Worked out once, so that a roof changing is a pass
   over the town and no more. */
static unsigned char owner1[LOCAL_W * LOCAL_H], owner2[LOCAL_W * LOCAL_H];
static unsigned char has_door[MAX_BLD], ridge_y[MAX_BLD];
static unsigned char top_y[MAX_BLD], bot_y[MAX_BLD];
static unsigned char left_x[MAX_BLD], right_x[MAX_BLD];
static unsigned char inside;    /* the building you are in, or 0 */

static unsigned char is_door(unsigned char t)
{
    return t == T_DOOR || t == T_LOCKEDDOOR;
}

static unsigned char barrier(unsigned char t)
{
    return t == T_WALL || t == T_SECRETDOOR || is_door(t);
}

/* Everything joined to (x, y) that is not a wall or a door, as `r`. The
   queue is in SHORE, which sm_shores() fills afresh afterwards. */
#pragma code-name (push, "OWLOW")   /* run at $0400: see main() */
static void flood(unsigned char x, unsigned char y, unsigned char r)
{
    static const signed char fx[4] = { 1, -1, 0, 0 };
    static const signed char fy[4] = { 0, 0, 1, -1 };
    unsigned char *qx = (unsigned char *)0xA000, *qy = qx + 512;
    unsigned int head = 0, tail = 0;
    unsigned char d, nx, ny;

    region[AT(x, y)] = r;
    qx[tail] = x;
    qy[tail++] = y;
    while (head < tail) {
        x = qx[head];
        y = qy[head++];
        if (y < top_y[r]) top_y[r] = y;
        if (y > bot_y[r]) bot_y[r] = y;
        if (x < left_x[r]) left_x[r] = x;
        if (x > right_x[r]) right_x[r] = x;
        for (d = 0; d < 4; ++d) {
            nx = x + fx[d];
            ny = y + fy[d];
            if (nx >= LOCAL_W || ny >= LOCAL_H || region[AT(nx, ny)] ||
                barrier(local[LOCAL_AT(nx, ny)])) {
                continue;
            }
            region[AT(nx, ny)] = r;
            qx[tail] = nx;
            qy[tail++] = ny;
        }
    }
}

#pragma code-name (pop)

static void find_owners(void);

static void find_buildings(void)
{
    unsigned char x, y, r = 2, t;

    memset(region, 0, sizeof(region));
    memset(has_door, 0, sizeof(has_door));
    memset(top_y, 0xFF, sizeof(top_y));
    memset(bot_y, 0, sizeof(bot_y));
    memset(left_x, 0xFF, sizeof(left_x));
    memset(right_x, 0, sizeof(right_x));
    flood(hero.x, hero.y, 1);                   /* the outside */
    for (y = 0; y < LOCAL_H; ++y) {
        for (x = 0; x < LOCAL_W; ++x) {
            if (!region[AT(x, y)] && !barrier(local[LOCAL_AT(x, y)]) &&
                r < MAX_BLD) {
                flood(x, y, r++);
            }
        }
    }
    for (y = 0; y < LOCAL_H; ++y) {              /* which have a door */
        for (x = 0; x < LOCAL_W; ++x) {
            t = local[LOCAL_AT(x, y)];
            if (!is_door(t)) {
                continue;
            }
            if (x > 0)            has_door[region[AT(x - 1, y)]] = 1;
            if (x + 1 < LOCAL_W)  has_door[region[AT(x + 1, y)]] = 1;
            if (y > 0)            has_door[region[AT(x, y - 1)]] = 1;
            if (y + 1 < LOCAL_H)  has_door[region[AT(x, y + 1)]] = 1;
        }
    }
    has_door[0] = has_door[1] = 0;              /* walls; the outside */
    for (r = 2; r < MAX_BLD; ++r) {
        ridge_y[r] = (top_y[r] + bot_y[r] + 1) / 2;
    }
    inside = 0;
    find_owners();
}

static unsigned char roofed(unsigned char r)
{
    return has_door[r] && r != inside;
}

/* Each wall's buildings: those with a door among the eight round it. */
static void find_owners(void)
{
    unsigned char x, y, dx, dy, nx, ny, r;
    unsigned int i = 0;

    for (y = 0; y < LOCAL_H; ++y) {
        for (x = 0; x < LOCAL_W; ++x, ++i) {
            owner1[i] = owner2[i] = 0;
            if (region[i] || is_door(local[LOCAL_AT(x, y)])) {
                continue;
            }
            for (dy = 0; dy < 3; ++dy) {
                for (dx = 0; dx < 3; ++dx) {
                    nx = x + dx - 1;
                    ny = y + dy - 1;
                    if (nx >= LOCAL_W || ny >= LOCAL_H) {
                        continue;
                    }
                    r = region[AT(nx, ny)];
                    if (!has_door[r] || r == owner1[i]) {
                        continue;
                    }
                    if (!owner1[i]) {
                        owner1[i] = r;
                    } else {
                        owner2[i] = r;
                    }
                }
            }
        }
    }
}

/* WORLD from local[], with a roof on every building you are not in: the
   squares bx0-bx1 by by0-by1 of it (the whole town, or where a roof came
   off and went on). */
static unsigned char bx0, by0, bx1, by1;

#pragma code-name (push, "OWLOW")
static void put_roofs(void)
{
    unsigned char x, y, t, r;
    unsigned char *w = WORLD + (unsigned int)by0 * 64;
    unsigned char *l = local + LOCAL_AT(0, by0);
    unsigned int i;

    for (y = by0; y <= by1; ++y, w += 64, l += LOCAL_STRIDE) {
        i = AT(bx0, y);
        for (x = bx0; x <= bx1; ++x, ++i) {
            t = l[x];
            r = region[i];
            if (!r) {                   /* a wall: either building's roof */
                r = owner1[i];
                if (!roofed(r)) {
                    r = owner2[i];
                }
            }
            if (!roofed(r)) {
                w[x] = t;
            } else {
                w[x] = y == ridge_y[r] ? T_RIDGE : T_ROOF;
            }
        }
    }
}

/* The people, as sprites 1 to 7, but for those under a roof: where they
   stand, and a robe each (by who they are, so it stays theirs). */
#pragma code-name (pop)

static void show_people(void)
{
    const TownEnt *e = here_ents;
    unsigned char i, n = 0;

    for (i = 0; i < here_n && n < 7; ++i, ++e) {
        if (roofed(region[AT(e->x, e->y)])) {
            continue;
        }
        sm_npc_x[n] = e->x + BIAS;
        sm_npc_y[n] = e->y + BIAS;
        POKE(0x43F9 + n, NPC_FRAME);    /* both screens' sprite pointers */
        POKE(0x47F9 + n, NPC_FRAME);
        POKE(0xD028 + n, robes[i % 7]);
        ++n;
    }
    VIC.spr_mcolor = 0xFF;
    sm_npc_n = n;
}

/* The box of squares building r decides, grown to take it in: the
   building, its walls, and a square more. */
static void grow_box(unsigned char r)
{
    unsigned char a;

    if (!r) {
        return;
    }
    a = left_x[r] < 2 ? 0 : left_x[r] - 2;
    if (a < bx0) bx0 = a;
    a = top_y[r] < 2 ? 0 : top_y[r] - 2;
    if (a < by0) by0 = a;
    a = right_x[r] + 2 >= LOCAL_W ? LOCAL_W - 1 : right_x[r] + 2;
    if (a > bx1) bx1 = a;
    a = bot_y[r] + 2 >= LOCAL_H ? LOCAL_H - 1 : bot_y[r] + 2;
    if (a > by1) by1 = a;
}

#pragma code-name (pop)             /* (the rest has room in MAIN again) */

/* That box in characters on the screen, clipped to it (if none of it is
   on the screen, a corner of the screen: the copy is all that matters). */
static void box_on_screen(void)
{
    int c0 = (bx0 + BIAS) * 2 - sm_ox, c1 = (bx1 + BIAS) * 2 + 1 - sm_ox;
    int r0 = (by0 + BIAS) * 2 - sm_oy, r1 = (by1 + BIAS) * 2 + 1 - sm_oy;

    if (c0 < 0) c0 = 0;
    if (r0 < 0) r0 = 0;
    if (c1 > 39) c1 = 39;
    if (r1 > 24) r1 = 24;
    if (c0 > c1 || r0 > r1) {
        c0 = c1 = r0 = r1 = 0;
    }
    if (whole) {
        c0 = r0 = 0;
        c1 = 39;
        r1 = 24;
    }
    sm_box_c0 = c0;
    sm_box_c1 = c1;
    sm_box_r0 = r0;
    sm_box_r1 = r1;
}

/* At the end of a step in a town: in or out of a building? */
/* The building a door at (x, y) opens into: one beside it with a roof, or
   the one already open if it joins two (a door between rooms); 0 if none. */
static unsigned char door_opens(unsigned char x, unsigned char y)
{
    static const signed char dx[4] = { 1, -1, 0, 0 };
    static const signed char dy[4] = { 0, 0, 1, -1 };
    unsigned char d, nx, ny, r, found = 0;

    for (d = 0; d < 4; ++d) {
        nx = x + dx[d];
        ny = y + dy[d];
        if (nx >= LOCAL_W || ny >= LOCAL_H) {
            continue;
        }
        r = region[AT(nx, ny)];
        if (!has_door[r]) {
            continue;
        }
        if (r == inside) {
            return r;
        }
        if (!found) {
            found = r;
        }
    }
    return found;
}

static void roof_check(void)
{
    unsigned char x = hx - BIAS, y = hy - BIAS;
    unsigned char r = region[AT(x, y)];

    if (!r) {
        if (!is_door(local[LOCAL_AT(x, y)])) {
            return;             /* (a wall: never stood on) */
        }
        r = door_opens(x, y);   /* standing in a doorway opens the building */
        if (!r) {
            return;
        }
    }
    if (!has_door[r]) {
        r = 0;                  /* outside, or nowhere with a roof */
    }
    if (r == inside) {
        return;
    }
    roof_time = PEEK(0xA2);
    /* What changes: the building you left and the one you went into, with
       their walls and a square more, whose edges they decide. */
    bx0 = by0 = 0xFF;
    bx1 = by1 = 0;
    grow_box(inside);
    grow_box(r);
    if (whole) {
        bx0 = by0 = 0;
        bx1 = LOCAL_W - 1;
        by1 = LOCAL_H - 1;
    }
    inside = r;
    put_roofs();
    sm_sh_x0 = bx0;             /* their edges ... */
    sm_sh_y0 = by0;
    sm_sh_x1 = bx1;
    sm_sh_y1 = by1;
    sm_shores_rect();
    box_on_screen();            /* ... and their characters, into the hidden */
    sm_redraw_box();            /* screen, the rest copied from the front */
    pre_dir = NONE;             /* (over any shift made for the next step) */
    show_people();
    sm_npcs();
    sm_commit(1);               /* ... and flipped to at the next frame */
    next_frame();
    roof_time = PEEK(0xA2) - roof_time;
    frame = sm_frame;
}

/* The camera on the hero at engine square (x, y) of the new area, drawn,
   and on. */

static void show_area(unsigned char x, unsigned char y)
{
#ifdef OW_REU
    if (area != AREA_HOME)      /* (the REU kept the overworld's) */
#endif
    sm_shores();
    hx = x;
    hy = y;
    pre_dir = NONE;
    place_camera();
    sm_draw_all();
    sm_npcs();
    sm_blank(0);
    switch_time = PEEK(0xA2) - switch_time;
    frame = sm_frame;           /* that took frames, which were not missed */
}

static void go_town(unsigned char index)
{
    unsigned char *src = local + sizeof(local) - LOCAL_W * LOCAL_H;
    unsigned char y;

    leave_area();
    area = index == TOWN_CASTLE ? AREA_CASTLE : AREA_TOWN;
    play_for(area == AREA_CASTLE ? MUSIC_CASTLE : MUSIC_TOWN);
    if (index < NUM_TOWNS) {
        rle_unpack(src, town_map[index], LOCAL_W * LOCAL_H);
        for (y = 0; y < LOCAL_H; ++y, src += LOCAL_W) {
            memmove(local + LOCAL_AT(0, y), src, LOCAL_W);
        }
        here_ents = town_ents + town_ent_first[index];
        here_n    = town_ent_first[index + 1] - town_ent_first[index];
        here_ex   = town_ex[index];
        here_ey   = town_ey[index];
        hero.x    = town_sx[index];
        hero.y    = town_sy[index];
    } else {
        town_gen();             /* the people, the way out and where to stand */
    }
    build_area(area == AREA_CASTLE ? ts_castle : ts_town, LOCAL_W, LOCAL_H);
    find_buildings();
    bx0 = by0 = 0;
    bx1 = LOCAL_W - 1;
    by1 = LOCAL_H - 1;
    put_roofs();
    show_people();
    ex = here_ex;
    ey = here_ey;
    show_area(hero.x + BIAS, hero.y + BIAS);
}

/* Level `depth` of dungeon dun_index (the Hollow Crown at home, the
   Drowned Stair or Blackpit across the sea), standing on the stairs you
   came by: up if you came down, down if you came up. */
static void dungeon_level(unsigned char came_down)
{
    dungeon_build(dun_index, depth);
    build_area(ts_dungeon, DUN_W, DUN_H);
    if (came_down) {
        show_area(dun_up_x + BIAS, dun_up_y + BIAS);
    } else {
        show_area(dun_dn_x + BIAS, dun_dn_y + BIAS);
    }
}

static void go_dungeon(unsigned char index)
{
    leave_area();
    area = AREA_DUNGEON;
    play_for(MUSIC_DUNGEON);
    dun_index = index;
    depth = 1;
    dungeon_level(1);
}

static void back_home(void)
{
    leave_area();
    area = AREA_HOME;
    sm_npc_n = 0;               /* the town's people stay in the town */
    sm_tileset(ts_overworld);
    sm_torus = 1;
    area_w = area_h = 64;
    wx = out_wx;
    wy = out_wy;
    play_world();
#ifdef OW_REU
    sm_reu_keep(0);             /* the window as it was, edges and all */
#else
    fill_window();              /* (show_area works out the edges) */
#endif
    seek();
    show_ships();
    show_area(wx & 127, wy & 127);
}

/* At the end of a step: whatever the square does. Home's places are, in
   isle_fx's order, Millford, the castle, the Hollow Crown, Maldrath's lair
   (which is the dragon, and not here) and a town of its own. */
static void arrived(void)
{
    unsigned char t = tile_at(hx, hy), i, k, lx, ly, d;

    switch (area) {
    case AREA_HOME:
        if (t != T_TOWN && t != T_CASTLE && t != T_DUNGEON) {
            return;
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
            case 0: go_town(TOWN_MILLFORD); break;
            case 1: go_town(TOWN_CASTLE);   break;
            case 2: go_dungeon(0);          break;
            case 4: go_town(TOWN_PROC);     break;
            }
        } else if (k) {
            /* A continent's: its story place (a town or a dungeon, by the
               seed's shuffle), or its own town. */
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
                    go_dungeon(1 + (d >> 1));
                } else {
                    go_town(TOWN_WARDLOW + (d >> 1));
                }
            } else if (i == 1) {
                go_town(TOWN_PROC);
            }
        }
        break;
    case AREA_DUNGEON:
        if (t == T_STAIRDN && depth < dungeon_deep[dun_index]) {
            leave_area();
            ++depth;
            dungeon_level(1);
        } else if (t == T_STAIRUP) {
            if (depth == 1) {
                back_home();
            } else {
                leave_area();
                --depth;
                dungeon_level(0);
            }
        }
        break;
    default:
        if (hx - BIAS == ex && hy - BIAS == ey) {
            back_home();
        } else {
            roof_check();
        }
        break;
    }
}

/* Something typed: a step, or a setting. PETSCII. */
#pragma code-name (push, "OWTOP")  /* past the sprite blocks: MAIN is full */
static unsigned char key_dir(unsigned char k)
{
    switch (k) {
    case 'w': case 0x91: return DIR_UP;
    case 's': case 0x11: return DIR_DOWN;
    case 'a': case 0x9D: return DIR_LEFT;
    case 'd': case 0x1D: return DIR_RIGHT;
    case 'q': return DIR_UL;
    case 'e': return DIR_UR;
    case 'z': return DIR_DL;
    case 'c': return DIR_DR;
    }
    return NONE;
}

static unsigned char typed(void)
{
    unsigned char k = cbm_k_getin(), d = key_dir(k);

    if (d != NONE) {
        return d;
    }
    switch (k) {
    case '1': speed = 1; break;
    case '2': speed = 2; break;
    case 'm':                   /* the music off, or on from the top */
        music ^= 1;
        if (!music) {
            ow_tune_stop();
        } else if (song_now != NONE) {
            ow_tune_play(song_now);
        }
        break;
    case 'n':                   /* the bootstrap builds the next world */
        sm_quit();
        OW->seed = seed + 1;
        ow_run("openworld");
        break;
    }
    return NONE;
}

static void publish(void)
{
    PROBE_SCRATCH[0] = hx - BIAS;
    PROBE_SCRATCH[1] = hy - BIAS;
    PROBE_SCRATCH[2] = late;
    PROBE_SCRATCH[3] = sm_colend;
    PROBE_SCRATCH[4] = sm_ox;
    PROBE_SCRATCH[5] = sm_oy;
    PROBE_SCRATCH[6] = area;
    PROBE_SCRATCH[7] = depth;
}
#pragma code-name (pop)

/* The frame's camera, `moved` pixels into the step, and the screen
   flipped if that takes it into another character. Returns whether it
   flipped.

   A diagonal one way across and the other way down would wrap its two fine
   scrolls on different frames (the camera rests four pixels into a
   character, which is three from wrapping one way and four the other), and
   two flips two frames apart leave no frame to build in between. So the
   axis going the positive way runs a pixel behind until the last frame:
   then both wrap together and every flip is one, as on a straight step. */
static unsigned char glide(void)
{
    signed char dx = dirdx[dir], dy = dirdy[dir];
    unsigned char px = moved, py = moved, ox, oy, flip;
    unsigned int c;

    if (dx && dy && dx != dy && moved != 16) {
        if (dx > 0) {
            px = moved - 1;
        } else {
            py = moved - 1;
        }
    }
    c = dx > 0 ? camx0 + px : dx < 0 ? camx0 - px : camx0;
    ox = (unsigned char)(c >> 3);
    sm_fx = (unsigned char)c & 7;
    c = dy > 0 ? camy0 + py : dy < 0 ? camy0 - py : camy0;
    oy = (unsigned char)(c >> 3);
    sm_fy = (unsigned char)c & 7;

    flip = ox != sm_ox || oy != sm_oy;
    if (flip) {
        /* Never wanted at one or two pixels a frame, but a flip to a screen
           half built would show it: finish it first, late or not. It builds
           from the old origin, so before that moves. */
        build_some();
        build_some();
        build_some();
    }
    sm_ox = ox;
    sm_oy = oy;
    sm_commit(flip);
    return flip;
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
        VIC.spr0_color = COLOR_RED;
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

int main(void)
{
    unsigned char d, flipped;

    /* The C stack, which cc65 starts at the top of MAIN, into the tape
       buffer instead ($0334-$03FF, down from $0400), which nothing else
       here uses: MAIN is full to the last byte. (main() keeps nothing on
       it, and never returns.) */
    sm_stack();

    if (OW->magic != OW_MAGIC) {
        cputs("\r\nLoad OPENWORLD: it builds the world.");
        for (;;) {
        }
    }
    sm_tileset(ts_overworld);
    take_world();
    /* The text screen is done with: the code that runs at $0400 comes out
       of screen A's space, which nothing has drawn on yet. */
    memcpy((void *)0x0400, _OWLOW_LOAD__, (unsigned)_OWLOW_SIZE__);

    sm_init();
    place_camera();
    sm_draw_all();
    sm_sprite(0);
    sm_show();
    show_ships();
    sm_npcs();
    play_world();
    sm_blank(0);

    while (kbhit()) {
        cgetc();
    }
    publish();
    probe_stage(1);

    frame = sm_frame;
    for (;;) {
        /* The next frame after the last one handled: a step's build can
           run past an interrupt, and then this does not wait at all. */
        while (sm_frame == frame) {
            if (gen_state & 1) {
                gen_run(frame); /* a continent's build has the rest of it */
            } else if (moving) {
                ++idle;         /* how much of the frame is left over */
            }
        }
        if (moving) {
            ++walked;
        }
        if ((unsigned char)(sm_frame - frame) > 1) {
            ++late;
        }
        frame = sm_frame;
        PROBE_COUNTER = frame;
        if (after) {
            stream(dirdx[after - 1], dirdy[after - 1]);
            show_ships();
            sm_npcs();
            after = 0;
            /* Which continent to have ready, every fourth step (it is 33
               squares off when it changes: see kept), and here, not in the
               frame the step ended in: on NTSC neither has time to spare. */
            if (!(steps & 3)) {
                seek();
            }
        }

        if (moving) {
            if (freeze_at && moved == freeze_at && steps >= freeze_after) {
                publish();
                probe_stage(3);
                for (;;) {
                }
            }
            moved += speed;
            flipped = glide();
            if (flipped) {
                pre_dir = NONE;     /* the back screen is the old front */
            }
            if (sm_npc_n) {
                sm_npcs();      /* the people move with the map */
            }
            if (moved == 8) {
                sm_sprite(hero_frame(dir, stride ^= 1));
            }
            if (flipped) {
                /* The old front is on show until the interrupt: build from
                   next frame, if the step has another character to go. */
                if (moved < 9) {
                    phase = SHIFT;
                }
            } else if (phase != BUILT) {
                build_some();
            } else if (moved > 12 && pre_dir == NONE) {
                /* Nothing left to build this step, and no flip before it
                   ends: shift the back screen for the next one now, if it
                   can be seen coming, so that the frame that ends this
                   step and starts that one need not (on NTSC it has no
                   room for it). If it goes another way after all, or none,
                   this is wasted, not wrong. */
                d = held();
                if (d == NONE && PEEK(0xC6)) {
                    d = key_dir(PEEK(0x277));
                }
                if (d != NONE) {
                    sm_shift(d);
                    pre_dir = d;
                }
            }
            if (moved == 16) {
                moving = 0;
                if (area == AREA_HOME) {
                    step_world();
                } else {
                    hx += dirdx[dir];
                    hy += dirdy[dir];
                }
                ++steps;
                arrived();
                publish();
            }
        } else if ((frame & 15) == 0 && !still) {
            sm_animate();
        }

        if (!moving && !flipped) {
            if (PEEK(0xC6) != 0) {
                probe_stage(1); /* not still: something is to be read */
            }
            d = held();
            if (d != NONE) {
                POKE(0xC6, 0);  /* a tap is a step, not two */
            } else {
                d = typed();
            }
            if (d != NONE) {
                sm_sprite(hero_frame(d, stride));
                if (can_step(d)) {
                    dir = d;
                    moving = 1;
                    moved = 0;
                    camx0 = (unsigned int)sm_ox * 8 + sm_fx;
                    camy0 = (unsigned int)sm_oy * 8 + sm_fy;
                    if (dir == pre_dir) {
                        phase = ROW;    /* shifted already */
                    } else {
                        phase = SHIFT;
                        build_some();   /* the edges in the next frames */
                    }
                    sm_sprite(hero_frame(dir, stride ^= 1));
                }
            } else if (PEEK(0xC6) == 0 && steps != 0) {
                probe_stage(2);
            }
        }
        /* A frame with nothing built in it: the window's next job. */
        if (area == AREA_HOME) {
            jobs();
        }
        flipped = 0;
    }
    return 0;
}
