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
 *   1 / 2                                        one or two pixels a frame
 *   N                                            the next seed's Keldmarch
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

extern unsigned char sm_ox, sm_oy, sm_fx, sm_fy, sm_front, sm_colend;
extern volatile unsigned char sm_frame;
void sm_init(void);
void sm_show(void);
void sm_draw_all(void);
void sm_animate(void);
void __fastcall__ sm_build(unsigned char dir);
void __fastcall__ sm_commit(unsigned char flip);
void __fastcall__ sm_sprite(unsigned char frame);

/* Poke-able. */
unsigned char seed      = 1;    /* lore_seed for the land */
unsigned char speed     = 2;    /* pixels a frame: 1 or 2 */
unsigned char freeze_at = 0;    /* stop this many pixels into a step ... */
unsigned char freeze_after = 0; /* ... once this many steps are done */
unsigned char still     = 0;    /* 1: the water does not flow */

/* What the screen is doing, for --peek. */
unsigned char hx, hy;           /* the hero's square, biased by BIAS */
unsigned char late;             /* frames the main loop missed */
unsigned int  steps;

static unsigned char moving, dir, moved, stride;

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

/* A direction held down: the stick, or a key still down in the matrix. */
static unsigned char held(void)
{
    unsigned char k = PEEK(0xCB), shift = PEEK(0x28D) & 1;
    unsigned char j = ~PEEK(0xDC00) & 0x0F;

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
    case '1': speed = 1; break;
    case '2': speed = 2; break;
    case 'n':
        ++seed;
        build_world();
        place_camera();
        sm_draw_all();
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

/* One frame of a step: the camera `speed` pixels on, and the screen flipped
   when the fine scroll wraps. Returns whether it flipped. */
static unsigned char glide(void)
{
    unsigned char flip = 0;

    switch (dir) {
    case DIR_RIGHT:
        sm_fx += speed;
        if (sm_fx >= 8) { sm_fx -= 8; ++sm_ox; flip = 1; }
        break;
    case DIR_LEFT:
        if (sm_fx < speed) { sm_fx += 8; --sm_ox; flip = 1; }
        sm_fx -= speed;
        break;
    case DIR_DOWN:
        sm_fy += speed;
        if (sm_fy >= 8) { sm_fy -= 8; ++sm_oy; flip = 1; }
        break;
    default:
        if (sm_fy < speed) { sm_fy += 8; --sm_oy; flip = 1; }
        sm_fy -= speed;
        break;
    }
    sm_commit(flip);
    return flip;
}

int main(void)
{
    unsigned char d, frame, flipped, need = 0;

    bgcolor(COLOR_BLACK);
    bordercolor(COLOR_BLACK);
    clrscr();
    cputs("Building Keldmarch...");
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
            flipped = glide();
            moved += speed;
            if (moved == 8) {
                sm_sprite(dir * 2 + (stride ^= 1));
            }
            if (flipped) {
                /* The old front is on show until the interrupt: build next
                   frame, if the step has another character to go. */
                need = moved < 9;
            } else if (need) {
                sm_build(dir);
                need = 0;
            }
            if (moved == 16) {
                moving = 0;
                switch (dir) {
                case DIR_RIGHT: ++hx; break;
                case DIR_LEFT:  --hx; break;
                case DIR_DOWN:  ++hy; break;
                default:        --hy; break;
                }
                ++steps;
                publish();
            }
        } else if ((frame & 15) == 0 && !still) {
            sm_animate();
        }

        if (!moving && !flipped) {
            d = held();
            if (d != NONE) {
                POKE(0xC6, 0);  /* a tap is a step, not two */
            } else {
                d = typed();
            }
            if (d != NONE) {
                sm_sprite(d * 2 + stride);
                if (passable(hx + (d == DIR_RIGHT) - (d == DIR_LEFT),
                             hy + (d == DIR_DOWN) - (d == DIR_UP))) {
                    dir = d;
                    moving = 1;
                    moved = 0;
                    sm_build(dir);
                    sm_sprite(dir * 2 + (stride ^= 1));
                }
            } else if (PEEK(0xC6) == 0 && steps != 0) {
                probe_stage(2);
            }
        }
        flipped = 0;
    }
    return 0;
}
