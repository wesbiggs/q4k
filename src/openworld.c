/*
 * Open world: a prototype of smooth scrolling for q4k.
 *
 * Keldmarch, built by the game's own generator from a seed, walked with the
 * camera gliding a pixel or two a frame instead of jumping a square at a
 * time: a multicolour tile set (assets/overworld.txt, 16 by 16 pixels a
 * square) on a double-buffered screen, with colour RAM rewritten ahead of the
 * beam. The engine is src/lib/smooth.s, which explains how.
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

#include <q4k/game.h>
#include <q4k/isle.h>
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

static unsigned char passable(unsigned char x, unsigned char y)
{
    unsigned char t;

    x -= BIAS;
    y -= BIAS;
    if (x >= 64 || y >= 64) {
        return 0;
    }
    t = WORLD[(unsigned int)y * 64 + x];
    return t != T_WATER && t != T_MOUNTAIN && t != T_WALL;
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
