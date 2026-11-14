/*
 * Open world: a prototype of smooth scrolling for q4k.
 *
 * Keldmarch, built by the game's own generator from a seed, walked with the
 * camera gliding a pixel or two a frame instead of jumping a square at a
 * time: a multicolour tile set (assets/overworld.txt, 16 by 16 pixels a
 * square) on a double-buffered screen, with colour RAM rewritten ahead of the
 * beam. The engine is src/lib/smooth.s, which explains how.
 *
 * Step onto Millford, the castle, the Hollow Crown or home's other town to go
 * in: each has a tile set of its own (assets/town_tiles.txt and the rest),
 * its people are sprites, and its walls have faces.
 *
 * It is a single file that needs no disk, so it runs as is on a TeensyROM+ or
 * anything else. The game itself is untouched: this is where the ideas are
 * tried before they go near its memory map.
 *
 *   cursor keys, WASD or a joystick in port 2   walk
 *   Q, E, Z, C (or the stick's diagonals)       walk diagonally
 *   1 / 2                                        one or two pixels a frame
 *   N                                            the next seed's Keldmarch
 *   R                                            the REU on or off, if there
 *                                                is one (the picture should
 *                                                not change: only the time)
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

extern unsigned char sm_ox, sm_oy, sm_fx, sm_fy, sm_front, sm_colend, sm_reu;
unsigned char sm_reu_detect(void);
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
extern unsigned char sm_sh_x0, sm_sh_y0, sm_sh_x1, sm_sh_y1;
extern unsigned char sm_box_c0, sm_box_c1, sm_box_r0, sm_box_r1;

/* The villager's sprite frame, after the hero's eight (assets/overworld.txt),
   and the robes' colours, one a person. */
#define NPC_FRAME  (64 + 8)
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
   overworld is put away while you are in one -- in the REU if there is one,
   or else built again from the vault on the way out. */
#define AREA_HOME    0
#define AREA_TOWN    1
#define AREA_CASTLE  2
#define AREA_DUNGEON 3
unsigned char area;             /* (and game.c's depth: a dungeon's level) */
static unsigned char area_w = 64, area_h = 64;
static unsigned char out_x, out_y;      /* the overworld square you went in at */
static unsigned char ex, ey;            /* a town's way out */
static unsigned char kept;              /* the overworld is in the REU */

/* What the game's own modules here (the land and town generators, the
   dungeon's, the vault) read and write of game.c's and actor.c's: defined
   here instead, so that neither is linked, nor the view, the sprite and the
   arms they bring with them -- about 6K the prototype has no room for. */
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
unsigned char seed      = 1;    /* lore_seed for the land */
unsigned char speed     = 2;    /* pixels a frame: 1 or 2 */
unsigned char freeze_at = 0;    /* stop this many pixels into a step ... */
unsigned char freeze_after = 0; /* ... once this many steps are done */
unsigned char still     = 0;    /* 1: the water does not flow */
unsigned char use_reu   = 1;    /* 0: the CPU does it all, REU or not */
unsigned char whole     = 0;    /* 1: a roof change redraws everything, for
                                   checking that the box it does is enough */

/* What sm_reu_detect() found: 0 no REU, 1 one that reaches colour RAM, 2
   one that does not (and so is not used). */
unsigned char reu_found;

/* What the screen is doing, for --peek. */
unsigned char hx, hy;           /* the hero's square, biased by BIAS */
unsigned char late;             /* frames the main loop missed */
unsigned char shore_time;       /* jiffies sm_shores() took */
unsigned char switch_time;      /* jiffies the last way in or out took */
unsigned char roof_time;        /* jiffies the last roof change took */
unsigned int  steps;
unsigned long idle;             /* turns of the waiting loop while walking */
unsigned int  walked;           /* ... over this many frames */

static unsigned char moving, dir, moved, stride;
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

#define NONE 0xFF

static void build_world(void)
{
    unsigned char y;

    lore_seed  = seed;
    isle_story = seed & 1;
    isle_build(ISLE_HOME);
    land       = ISLE_HOME;     /* for town_gen(), which follows its biome */
    home_biome = isle_biome;
    for (y = 0; y < 64; ++y) {
        vault_row(y, WORLD + (unsigned int)y * 64);
    }
    shore_time = PEEK(0xA2);
    sm_shores();
    shore_time = PEEK(0xA2) - shore_time;
    hx = isle_fx[0] + BIAS;     /* Millford */
    hy = isle_fy[0] + BIAS;
}

static void place_camera(void)
{
    unsigned int cam;

    cam   = (unsigned int)hx * 16 - HERO_PX;
    sm_ox = (unsigned char)(cam >> 3);
    sm_fx = (unsigned char)cam & 7;
    cam   = (unsigned int)hy * 16 - HERO_PY;
    sm_oy = (unsigned char)(cam >> 3);
    sm_fy = (unsigned char)cam & 7;
}

/* What is really at a square: in a place, local[], where it was built
   (WORLD has its roofs drawn over it); on the overworld, WORLD. */
static unsigned char tile_at(unsigned char x, unsigned char y)
{
    x -= BIAS;
    y -= BIAS;
    if (area != AREA_HOME) {
        return local[LOCAL_AT(x, y)];
    }
    return WORLD[(unsigned int)y * 64 + x];
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

/* Whether a step that way can be taken: onto open ground, and on a
   diagonal not squeezing between two corners, of sea or of mountain. */
static unsigned char can_step(unsigned char d)
{
    signed char dx = dirdx[d], dy = dirdy[d];

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

/* --- Going in and out of places --------------------------------------- */

static void next_frame(void)
{
    unsigned char f = sm_frame;

    while (sm_frame == f) {
    }
}

/* The screen off, and the overworld put away if that is where you are. */
static void leave_area(void)
{
    switch_time = PEEK(0xA2);
    sm_blank(1);
    next_frame();               /* the blank has reached the screen */
    if (area == AREA_HOME) {
        out_x = hx;
        out_y = hy;
        kept = sm_reu;
        if (kept) {
            sm_reu_keep(1);
        }
    }
}

/* A place's map, from local[], into WORLD, with the people in it, in the
   tile set `set` (which says what is beyond the map's edge). */
static void build_area(const unsigned char *set, unsigned char w,
                       unsigned char h)
{
    unsigned char y;

    sm_npc_n = 0;               /* nobody, until add_people() */
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
   Stepping onto its floor takes its roof off and puts back whichever came
   off before, drawn into the hidden screen and flipped to, so it changes
   between one frame and the next. A door's square changes nothing: it is
   the threshold either way. What you can walk on is read from local[],
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
static void roof_check(void)
{
    unsigned char r = region[AT(hx - BIAS, hy - BIAS)];

    if (!r) {
        return;                 /* a door: the threshold */
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
    show_people();
    sm_npcs();
    sm_commit(1);               /* ... and flipped to at the next frame */
    next_frame();
    roof_time = PEEK(0xA2) - roof_time;
    frame = sm_frame;
}

/* The camera on the hero at (x, y) of the new area, drawn, and on. */
static void show_area(unsigned char x, unsigned char y)
{
    if (!(area == AREA_HOME && kept)) {
        sm_shores();            /* (the REU kept the overworld's) */
    }
    hx = x + BIAS;
    hy = y + BIAS;
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
    show_area(hero.x, hero.y);
}

/* Level `depth` of the first dungeon, the Hollow Crown, standing on the
   stairs you came by: up if you came down, down if you came up. */
static void dungeon_level(unsigned char came_down)
{
    dungeon_build(0, depth);
    build_area(ts_dungeon, DUN_W, DUN_H);
    if (came_down) {
        show_area(dun_up_x, dun_up_y);
    } else {
        show_area(dun_dn_x, dun_dn_y);
    }
}

static void back_home(void)
{
    unsigned char y;

    leave_area();
    area = AREA_HOME;
    sm_npc_n = 0;               /* the town's people stay in the town */
    sm_tileset(ts_overworld);
    area_w = area_h = 64;
    if (kept) {
        sm_reu_keep(0);
    } else {
        for (y = 0; y < 64; ++y) {
            vault_row(y, WORLD + (unsigned int)y * 64);
        }
    }
    show_area(out_x - BIAS, out_y - BIAS);
}

/* At the end of a step: whatever the square does. Home's places are, in
   isle_fx's order, Millford, the castle, the Hollow Crown, Maldrath's lair
   (which is the dragon, and not here) and a town of its own. */
static void arrived(void)
{
    unsigned char t = tile_at(hx, hy), i;

    switch (area) {
    case AREA_HOME:
        if (t != T_TOWN && t != T_CASTLE && t != T_DUNGEON) {
            return;
        }
        for (i = 0; i < 5; ++i) {
            if (isle_fx[i] + BIAS == hx && isle_fy[i] + BIAS == hy) {
                break;
            }
        }
        switch (i) {
        case 0: go_town(TOWN_MILLFORD); break;
        case 1: go_town(TOWN_CASTLE);   break;
        case 2:
            leave_area();
            area = AREA_DUNGEON;
            depth = 1;
            dungeon_level(1);
            break;
        case 4: go_town(TOWN_PROC);     break;
        }
        break;
    case AREA_DUNGEON:
        if (t == T_STAIRDN && depth < dungeon_deep[0]) {
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
static unsigned char typed(void)
{
    switch (cbm_k_getin()) {
    case 'w': case 0x91: return DIR_UP;
    case 's': case 0x11: return DIR_DOWN;
    case 'a': case 0x9D: return DIR_LEFT;
    case 'd': case 0x1D: return DIR_RIGHT;
    case 'q': return DIR_UL;
    case 'e': return DIR_UR;
    case 'z': return DIR_DL;
    case 'c': return DIR_DR;
    case '1': speed = 1; break;
    case '2': speed = 2; break;
    case 'r':
        if (reu_found == 1) {
            sm_reu ^= 1;
        }
        break;
    case 'n':
        if (area != AREA_HOME) {
            break;
        }
        ++seed;
        build_world();
        place_camera();
        sm_draw_all();
        frame = sm_frame;       /* seconds went by: not frames missed */
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

int main(void)
{
    unsigned char d, flipped;

    bgcolor(COLOR_BLACK);
    bordercolor(COLOR_BLACK);
    clrscr();
    /* Said before the land is built, which takes a few seconds, so that it
       can be read on the machine itself: whether an REU was found, and
       whether its transfers reach colour RAM, which the TeensyROM's has to
       be trusted to do until this says so. */
    reu_found = sm_reu_detect();
    sm_tileset(ts_overworld);
    if (!use_reu) {
        sm_reu = 0;
    }
    cputs(reu_found == 0 ? "No REU: the CPU scrolls.\r\n" :
          reu_found == 1 ? "REU found, and DMA reaches colour RAM:\r\n"
                           "the REU scrolls (R turns it off).\r\n" :
                           "REU found, but DMA does not reach colour\r\n"
                           "RAM: the CPU scrolls.\r\n");
    cputs("\r\nBuilding Keldmarch...");
    build_world();

    sm_init();
    place_camera();
    sm_draw_all();
    sm_sprite(0);
    sm_show();

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
            if (moving) {
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

        if (moving) {
            if (freeze_at && moved == freeze_at && steps >= freeze_after) {
                publish();
                probe_stage(3);
                for (;;) {
                }
            }
            moved += speed;
            flipped = glide();
            if (sm_npc_n) {
                sm_npcs();      /* the people move with the map */
            }
            if (moved == 8) {
                sm_sprite(facing[dir] * 2 + (stride ^= 1));
            }
            if (flipped) {
                /* The old front is on show until the interrupt: build from
                   next frame, if the step has another character to go. */
                if (moved < 9) {
                    phase = SHIFT;
                }
            } else {
                build_some();
            }
            if (moved == 16) {
                moving = 0;
                hx += dirdx[dir];
                hy += dirdy[dir];
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
                sm_sprite(facing[d] * 2 + stride);
                if (can_step(d)) {
                    dir = d;
                    moving = 1;
                    moved = 0;
                    camx0 = (unsigned int)sm_ox * 8 + sm_fx;
                    camy0 = (unsigned int)sm_oy * 8 + sm_fy;
                    phase = SHIFT;
                    build_some();   /* and the edges in the next frames */
                    sm_sprite(facing[dir] * 2 + (stride ^= 1));
                }
            } else if (PEEK(0xC6) == 0 && steps != 0) {
                probe_stage(2);
            }
        }
        flipped = 0;
    }
    return 0;
}
