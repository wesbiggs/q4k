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

/* What sm_reu_detect() found: 0 no REU, 1 one that reaches colour RAM, 2
   one that does not (and so is not used). */
unsigned char reu_found;

/* What the screen is doing, for --peek. */
unsigned char hx, hy;           /* the hero's square, biased by BIAS */
unsigned char late;             /* frames the main loop missed */
unsigned char shore_time;       /* jiffies sm_shores() took */
unsigned char switch_time;      /* jiffies the last way in or out took */
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

static unsigned char tile_at(unsigned char x, unsigned char y)
{
    return WORLD[(unsigned int)(y - BIAS) * 64 + (unsigned char)(x - BIAS)];
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

/* The people, as sprites 1 to 7: where they stand, a robe each. The map
   under them is left as it is; passable() keeps you off them. */
static void add_people(const TownEnt *e, unsigned char n)
{
    unsigned char i;

    if (n > 7) {
        n = 7;
    }
    for (i = 0; i < n; ++i, ++e) {
        sm_npc_x[i] = e->x + BIAS;
        sm_npc_y[i] = e->y + BIAS;
        POKE(0x43F9 + i, NPC_FRAME);    /* both screens' sprite pointers */
        POKE(0x47F9 + i, NPC_FRAME);
        POKE(0xD028 + i, robes[i]);
    }
    VIC.spr_mcolor = 0xFF;
    sm_npc_n = n;
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
    add_people(here_ents, here_n);
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
