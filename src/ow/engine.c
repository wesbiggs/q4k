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
 * are sprites, and its walls have faces. Walk into someone with something to
 * say and they say it (src/owtalk.c).
 *
 * This is the core: the main loop, the music, and what every area does. The
 * overworld and the dungeons (world.c) and the towns (town.c) are overlays
 * that take turns in one slot of memory, loaded from the disk as the hero
 * goes in and out: engine.h.
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
#include <q4k/town_data.h>
#include <q4k/probe.h>
#include <q4k/sprite.h>
#include <q4k/tiles.h>

#include "engine.h"

/* Where you are: the overworld, or a place, each in its own tile set. A
   place is built the game's way (town_data.c's maps, town_gen(), and
   dungeon_build()) into local[], and copied into WORLD for the engine; the
   overworld is put away while you are in one -- in the REU (OW-REU), or
   else built again from the vault on the way out (OW-CPU). */
unsigned char area;             /* (and game.c's depth: a dungeon's level) */
unsigned char area_w = 64, area_h = 64;

unsigned int  out_wx;           /* where you went into a place */
unsigned char out_wy;
unsigned int  ship_x[SHIPS];
unsigned char ship_y[SHIPS];
unsigned char boat;             /* 1 + the ship you are in, or 0 */

unsigned char town_index;       /* the town to go into, or that you are in */
unsigned char resuming, resume_x, resume_y;     /* back from a talk */
unsigned char dun_index;        /* the dungeon to go into, or that you are in */

/* The overlays (engine.h): the files, by the engine's name, since each
   engine has its own set. In DATA, not RODATA, which is OW-DATA and must
   be the same bytes in both engines. */
#ifdef OW_REU
static char world_file[]   = "ow-reu.world";
static char town_file[]    = "ow-reu.town";
static char dungeon_file[] = "ow-reu.dungeon";
#else
static char world_file[]   = "ow-cpu.world";
static char town_file[]    = "ow-cpu.town";
static char dungeon_file[] = "ow-cpu.dungeon";
#endif
unsigned char ov_want, ov_have;

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

static unsigned char moving, moved;
unsigned char dir, stride;
unsigned char pre_dir = 0xFF;   /* the back screen is shifted that way */
unsigned char frame;            /* the last frame the main loop handled */
/* The step that ended last frame, + 1, until what it brings is seen to (the
   world's ov_after). Not in the frame the step ended, which starts the next
   one too, and has no room for it on a diagonal. */
unsigned char after;

/* Eight ways, a square at a time: sprite.h's four, then the diagonals,
   as smooth.s numbers them. A diagonal faces the way it goes across. */
const signed char dirdx[8] = { 0, 0, -1, 1, -1, 1, -1, 1 };
const signed char dirdy[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
const unsigned char facing[8] = {
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

/* The hero's frame: walking, or a ship facing the way it last went across. */
static unsigned char ship_face = 1;

unsigned char hero_frame(unsigned char d, unsigned char st)
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

void place_camera(void)
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
unsigned char tile_at(unsigned char x, unsigned char y)
{
    if (area != AREA_HOME) {
        return local[LOCAL_AT(x - BIAS, y - BIAS)];
    }
    return WORLD[((unsigned int)(y & 63) << 6) | (x & 63)];
}

unsigned char passable(unsigned char x, unsigned char y)
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

unsigned char __fastcall__ place_step(unsigned char d)
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

void next_frame(void)
{
    unsigned char f = sm_frame;

    while (sm_frame == f) {
    }
}

/* --- The music and the disk -----------------------------------------------

   The game's scenes and its SID files (assets/music.txt, music_map.h): a
   scene plays a song of a file. The bootstrap loaded the file a game starts
   with; one that is another is loaded here, with the screen off and the
   interrupt left with nothing to do but the keyboard. */

static unsigned char song_now = NONE;   /* the song playing, or that would be */
static unsigned char music = 1;         /* the M key */

/* Before the disk: the music stopped (silence has to reach VICE's sound
   buffer before the drive works, or it plays its last one for as long as
   that takes: CLAUDE.md), and the sprites, which take cycles the serial bus
   needs. Returns the sprites to put back. */
static unsigned char disk_on(void)
{
    unsigned char i;

    ow_tune_stop();
    song_now = NONE;
    for (i = 0; i < 12; ++i) {
        next_frame();
    }
    i = VIC.spr_ena;
    VIC.spr_ena = 0;
    return i;
}

static void disk_off(unsigned char sprites)
{
    VIC.spr_ena = sprites;
    frame = sm_frame;           /* those frames were not missed */
}

void play_for(unsigned char s)
{
    unsigned char f = tune_file[s], i;

    if (f != OW->tune) {
        OW->tune = OW_NO_TUNE;
        sm_blank(1);
        i = disk_on();
        if (cbm_load(tune_name[f], 8, 0)) {
            OW->tune = f;
        }
        disk_off(i);
    }
    if (OW->tune == f && tune_song[s] != song_now) {
        song_now = tune_song[s];
        if (music) {
            ow_tune_play(song_now);
        }
    }
}

/* The overlay ov_want asks for, into the slot (the screen is off), and
   started. The game's disk is needed: the border flashes until it is back.
   An overlay's ov_enter() may ask for the other in turn. */
static void ov_switch(void)
{
    unsigned char s;

    while (ov_want != ov_have) {
        s = disk_on();
        while (!cbm_load(ov_want == OV_TOWN ? town_file :
                         ov_want == OV_DUNGEON ? dungeon_file : world_file,
                         8, 0)) {
            ++VIC.bordercolor;
        }
        VIC.bordercolor = COLOR_BLACK;
        disk_off(s);
        ov_have = ov_want;
        ov_enter();
    }
}

/* --- Going in and out of places --------------------------------------- */

/* The screen off. (The world's overlay puts the overworld away too.) */
void leave_area(void)
{
    switch_time = PEEK(0xA2);
    sm_blank(1);
    next_frame();               /* the blank has reached the screen */
}

/* A place's map, from local[], into WORLD, with the people in it, in the
   tile set `set` (which says what is beyond the map's edge). */
void build_area(const unsigned char *set, unsigned char w, unsigned char h)
{
    unsigned char y;

    sm_npc_n = 0;               /* nobody, until the people are shown */
    sm_torus = 0;               /* a place has edges */
    sm_tileset(set);
    memset(WORLD, sm_offtile, 64 * 64);
    for (y = 0; y < h; ++y) {
        memcpy(WORLD + (unsigned int)y * 64, local + LOCAL_AT(0, y), w);
    }
    area_w = w;
    area_h = h;
}

/* The camera on the hero at engine square (x, y) of the new area, drawn,
   and on. */
void __fastcall__ show_area(unsigned char x, unsigned char y)
{
#ifdef OW_REU
    if (area != AREA_HOME)      /* (the REU kept the overworld's) */
#endif
    {
        shore_time = PEEK(0xA2);
        sm_shores();
        shore_time = PEEK(0xA2) - shore_time;
    }
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

/* Something typed: a step, or a setting. PETSCII. */
#pragma code-name (push, "OWTOP")  /* past the sprite blocks */
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

/* The bootstrap's hand-off (ow.h), or a talk's: what the core keeps of it.
   The overlays take the rest. A new game starts on the overworld, where the
   bootstrap says; the end of a talk in the town it began in. */
static void take_block(void)
{
    seed = lore_seed = OW->seed;
    land       = ISLE_HOME;
    home_biome = OW->home_biome;
    memcpy(home_fx, OW->home_fx, NUM_HOME_PLACES);
    memcpy(home_fy, OW->home_fy, NUM_HOME_PLACES);
    memcpy(ship_x, OW->ship_x, sizeof ship_x);
    memcpy(ship_y, OW->ship_y, sizeof ship_y);
    if (OWB->mode == OWB_BACK) {
        out_wx = OWB->out_x;
        out_wy = OWB->out_y;
        land = OWB->land;
        if (land != ISLE_HOME) {
            isle_biome = OW->ibiome[land];
        }
        town_index = OWB->town;
        resuming = 1;
        resume_x = OWB->x;
        resume_y = OWB->y;
        ov_want = OV_TOWN;
    } else {
        out_wx = OW->start_x;
        out_wy = OW->start_y;
        ov_want = OV_WORLD;
    }
    OWB->mode = OWB_NONE;
}

int main(void)
{
    unsigned char d, flipped;

    /* The C stack, which cc65 starts at the top of MAIN, into the tape
       buffer instead ($0334-$03FF, down from $0400), which nothing else
       here uses. (main() keeps nothing on it, and never returns.) */
    sm_stack();                 /* (the screen is off: the loader's doing) */

    if (OW->magic != OW_MAGIC) {
        cputs("\r\nLoad OPENWORLD: it builds the world.");
        for (;;) {
        }
    }
    take_block();
    /* (Nothing is in OWLOW now, the code run at $0400 that was copied out of
       screen A's space here: the roofs' flood and drawing were, and are in
       the town's overlay. Put some there and the copy has to come back.) */

    /* The interrupt going, with the screen off, before the first overlay:
       the disk waits for frames. The overlay draws its area and turns the
       screen on. */
    sm_init();
    sm_sprite(0);
    VIC.bgcolor0 = COLOR_BLACK;
    sm_blank(1);
    sm_show();
    ov_switch();

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
            ov_after();
            after = 0;
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
                if (area != AREA_HOME) {
                    hx += dirdx[dir];
                    hy += dirdy[dir];
                }
                ++steps;
                ov_arrived();   /* (the overworld's step is the world's) */
                ov_switch();    /* into a town or a dungeon, or out */
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
                if (ov_can_step(d)) {
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
                } else {
                    ov_bump(d); /* into a person: a talk, in a town */
                }
            } else if (PEEK(0xC6) == 0 && steps != 0) {
                probe_stage(2);
            }
        }
        /* A frame with nothing built in it: the window's next job. */
        if (area == AREA_HOME) {
            ov_idle();
        }
        flipped = 0;
    }
    return 0;
}
