#include <c64.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/sprite.h>

/* Read-only data goes in the charset's spare glyphs: cfg/c64-q4k.cfg. */
#pragma rodata-name ("CHDATA3")

#define SPR_FIGURE  ((unsigned char *)0x0340)   /* block 13 */
#define SPR_EDGE    ((unsigned char *)0x0380)   /* block 14 */
#define SPR_TUNIC   ((unsigned char *)0x03C0)   /* block 15 */
#define SPR_PTRS    ((unsigned char *)0x07F8)   /* screen matrix + $3F8 */

/* The shot has no room in the cassette buffer, but the charset has room to
   spare: $3E00-$3E3F is the glyphs for $C0-$C7, reverse-video characters
   the game never puts on screen. It is in VIC bank 0 and 64-byte aligned,
   so it serves as a sprite block. tiles_install() writes the font over it
   at start-up, which is harmless: the shape is drawn fresh every shot. */
#define SPR_SHOT    ((unsigned char *)0x3E00)
#define N_SHOT      3
#define SHOT_BIT    0x08
#define SHOT_PX     4          /* two frames a square */

/* Lower-numbered sprites are drawn in front: the tunic over the figure,
   the figure over its outline. */
#define N_TUNIC     0
#define N_FIGURE    1
#define N_EDGE      2
#define SPR_MASK    0x07

/* Which of the figure's pixels are the tunic: the middle four columns of
   the shoulder, waist and hip rows. Every frame's body sits there, and the
   hands and a mid-stride arm fall outside it, so they stay bare. */
#define TUNIC_TOP   3
#define TUNIC_ROWS  3
#define TUNIC_COLS  0x3C

/* Screen cell (0,0) starts at sprite coordinate (24,50). The figure sits a
   pixel in from the sprite's corner, leaving room for the outline above and
   to the left of it, so the sprite goes a pixel up and left to match. */
#define ORIGIN_X    23
#define ORIGIN_Y    49

/* A step is eight pixels: four frames of two, about 80ms on PAL. Slower
   than that and holding a cursor key walks noticeably behind the keyboard
   repeat. */
#define GLIDE_STEPS 4
#define GLIDE_PX    2

/* Two frames a direction, alternated step by step so the feet change over.
   Top row first, MSB leftmost, the same as a charset glyph. */
static const unsigned char frames[4][2][8] = {
    {   /* down: the face is the gap in the head */
        { 0x18, 0x24, 0x18, 0x7E, 0xBD, 0x3C, 0x24, 0x64 },
        { 0x18, 0x24, 0x18, 0x7E, 0xBD, 0x3C, 0x24, 0x26 },
    },
    {   /* up: the back of the head is solid */
        { 0x18, 0x3C, 0x18, 0x7E, 0xBD, 0x3C, 0x24, 0x64 },
        { 0x18, 0x3C, 0x18, 0x7E, 0xBD, 0x3C, 0x24, 0x26 },
    },
    {   /* left: full stride with the arms swinging, then half */
        { 0x18, 0x38, 0x18, 0x3C, 0x5A, 0x18, 0x24, 0x42 },
        { 0x18, 0x38, 0x18, 0x1C, 0x3C, 0x18, 0x28, 0x24 },
    },
    {   /* right: the left frames mirrored */
        { 0x18, 0x1C, 0x18, 0x3C, 0x5A, 0x18, 0x24, 0x42 },
        { 0x18, 0x1C, 0x18, 0x38, 0x3C, 0x18, 0x14, 0x24 },
    },
};

/* The shots, by SHOT_* kind, pointing each way: down, up, left, right as
   DIR_* numbers them. The front is always the heavy end: an arrow's head
   with its fletching behind, a flame's body with sparks behind, a gout of
   water with its spray behind. */
static const unsigned char shot_shapes[3][4][8] = {
    {   /* arrow */
        { 0x00, 0x14, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x08 },
        { 0x08, 0x1C, 0x08, 0x08, 0x08, 0x08, 0x14, 0x00 },
        { 0x00, 0x00, 0x41, 0xFF, 0x41, 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0x82, 0xFF, 0x82, 0x00, 0x00, 0x00 },
    },
    {   /* fire */
        { 0x08, 0x14, 0x2A, 0x1C, 0x7F, 0x3E, 0x1C, 0x08 },
        { 0x08, 0x1C, 0x3E, 0x7F, 0x1C, 0x2A, 0x14, 0x08 },
        { 0x00, 0x10, 0x34, 0x7A, 0xFD, 0x7A, 0x34, 0x10 },
        { 0x00, 0x08, 0x2C, 0x5E, 0xBF, 0x5E, 0x2C, 0x08 },
    },
    {   /* water */
        { 0x10, 0x28, 0x10, 0x00, 0x10, 0x38, 0x7C, 0x38 },
        { 0x38, 0x7C, 0x38, 0x10, 0x00, 0x10, 0x28, 0x10 },
        { 0x00, 0x40, 0xE2, 0xF5, 0xE2, 0x40, 0x00, 0x00 },
        { 0x00, 0x02, 0x47, 0xAF, 0x47, 0x02, 0x00, 0x00 },
    },
};
static const unsigned char shot_colors[3] = {
    COLOR_YELLOW, COLOR_ORANGE, COLOR_LIGHTBLUE,
};

static unsigned char facing, stride;
static unsigned char spr_x, spr_y;

/* Wait for the raster to reach line 251, just below the last text row.
   Its low byte does not recur later in the frame on PAL or NTSC, so the
   8-bit register is enough. Changing the sprite from here on means the
   VIC never draws half of one frame and half of the next. */
static void wait_frame(void)
{
    while (VIC.rasterline == 251) {
    }
    while (VIC.rasterline != 251) {
    }
}

/* Copy the current frame into sprite 0, and its outline into sprite 1.
   The outline is the figure grown a pixel in every direction; sprite 0
   covers the middle of it, so it only shows as a black edge.

   Except downwards: a row of black below the feet lands in the square
   underneath, and against a wall that reads as the hero standing in it. */
static void build(void)
{
    static unsigned int grown[10];
    const unsigned char *f = frames[facing][stride];
    unsigned char r;
    unsigned int  w;

    memset(grown, 0, sizeof(grown));
    for (r = 0; r < TUNIC_ROWS; ++r) {
        w = (unsigned int)(f[TUNIC_TOP + r] & TUNIC_COLS) << 7;
        SPR_TUNIC[(TUNIC_TOP + r + 1) * 3]     = (unsigned char)(w >> 8);
        SPR_TUNIC[(TUNIC_TOP + r + 1) * 3 + 1] = (unsigned char)w;
    }

    for (r = 0; r < 8; ++r) {
        w = (unsigned int)f[r] << 7;          /* one pixel in from the left */
        SPR_FIGURE[(r + 1) * 3]     = (unsigned char)(w >> 8);
        SPR_FIGURE[(r + 1) * 3 + 1] = (unsigned char)w;

        w |= (w << 1) | (w >> 1);
        grown[r]     |= w;
        grown[r + 1] |= w;
        grown[r + 2] |= w;
    }
    for (r = 0; r < 9; ++r) {
        SPR_EDGE[r * 3]     = (unsigned char)(grown[r] >> 8);
        SPR_EDGE[r * 3 + 1] = (unsigned char)grown[r];
    }
}

static void place(void)
{
    VIC.spr0_x = spr_x;
    VIC.spr0_y = spr_y;
    VIC.spr1_x = spr_x;
    VIC.spr1_y = spr_y;
    VIC.spr2_x = spr_x;
    VIC.spr2_y = spr_y;
}

/* The hero's colours, from their name: the same name always gets the same
   tunic, and the fourteen colours that are neither black nor white are all
   possible. Black would vanish into the outline and white into the figure.

   Each character is folded in with a rotate, so the order of the letters
   counts as well as which ones they are. */
static unsigned char tunic_color(void)
{
    const unsigned char *c = (const unsigned char *)hero.name;
    unsigned char h = 0;

    while (*c) {
        h = (unsigned char)((h << 1) | (h >> 7)) ^ *c++;
    }
    return (unsigned char)(2 + h % 14);
}

void sprite_init(void)
{
    memset(SPR_FIGURE, 0, 64);
    memset(SPR_EDGE, 0, 64);
    memset(SPR_TUNIC, 0, 64);

    VIC.spr_ena     &= ~SPR_MASK;
    VIC.spr_hi_x    &= ~SPR_MASK;   /* the map is cols 0-20: X never passes 255 */
    VIC.spr_mcolor  &= ~SPR_MASK;
    VIC.spr_exp_x   &= ~SPR_MASK;
    VIC.spr_exp_y   &= ~SPR_MASK;
    VIC.spr_bg_prio &= ~SPR_MASK;   /* in front of the text */
    VIC.spr2_color   = COLOR_BLACK;

    VIC.spr_ena     &= ~SHOT_BIT;
    VIC.spr_hi_x    &= ~SHOT_BIT;
    VIC.spr_mcolor  &= ~SHOT_BIT;
    VIC.spr_exp_x   &= ~SHOT_BIT;
    VIC.spr_exp_y   &= ~SHOT_BIT;
    VIC.spr_bg_prio &= ~SHOT_BIT;

    facing = DIR_DOWN;
    stride = 0;
    build();
}

void sprite_show(unsigned char col, unsigned char row)
{
    /* Sprite pointers sit just past the screen matrix, in bytes clrscr()
       leaves alone -- but set them every time rather than rely on that. */
    SPR_PTRS[N_TUNIC]  = (unsigned char)((unsigned int)SPR_TUNIC >> 6);
    SPR_PTRS[N_FIGURE] = (unsigned char)((unsigned int)SPR_FIGURE >> 6);
    SPR_PTRS[N_EDGE]   = (unsigned char)((unsigned int)SPR_EDGE >> 6);

    spr_x = ORIGIN_X + (col << 3);
    spr_y = ORIGIN_Y + (row << 3);
    place();
    /* Worked out afresh each time: a restored game can change the name. */
    VIC.spr0_color = tunic_color();
    VIC.spr1_color = hero.color;
    VIC.spr_ena   |= SPR_MASK;
}

void sprite_hide(void)
{
    VIC.spr_ena &= ~SPR_MASK;
}

void sprite_face(unsigned char dir)
{
    if (dir == facing) {
        return;
    }
    facing = dir;
    wait_frame();
    build();
}

void sprite_walk(unsigned char dir)
{
    signed char dx = 0, dy = 0;
    unsigned char i;

    switch (dir) {
        case DIR_UP:    dy = -GLIDE_PX; break;
        case DIR_DOWN:  dy =  GLIDE_PX; break;
        case DIR_LEFT:  dx = -GLIDE_PX; break;
        default:        dx =  GLIDE_PX; break;
    }

    facing = dir;
    stride ^= 1;
    for (i = 0; i < GLIDE_STEPS; ++i) {
        spr_x += dx;
        spr_y += dy;
        wait_frame();
        if (i == 0) {
            build();
        }
        place();
    }
}

void sprite_shot(unsigned char col, unsigned char row,
                 unsigned char dir, unsigned char squares,
                 unsigned char kind)
{
    signed char dx = 0, dy = 0;
    unsigned char r, x, y, frames;

    memset(SPR_SHOT, 0, 64);
    for (r = 0; r < 8; ++r) {
        SPR_SHOT[r * 3] = shot_shapes[kind][dir][r];
    }
    SPR_PTRS[N_SHOT] = (unsigned char)((unsigned int)SPR_SHOT >> 6);
    VIC.spr3_color   = shot_colors[kind];

    switch (dir) {
        case DIR_UP:    dy = -SHOT_PX; break;
        case DIR_DOWN:  dy =  SHOT_PX; break;
        case DIR_LEFT:  dx = -SHOT_PX; break;
        default:        dx =  SHOT_PX; break;
    }

    /* It sets off from the shooter's own square. From the hero's that is
       behind the figure (sprite 3 is drawn under sprites 0-2), so it seems
       to leave the bow. */
    x = ORIGIN_X + 1 + (col << 3);
    y = ORIGIN_Y + 1 + (row << 3);
    VIC.spr3_x = x;
    VIC.spr3_y = y;
    wait_frame();
    VIC.spr_ena |= SHOT_BIT;

    for (frames = squares * (8 / SHOT_PX); frames; --frames) {
        x += dx;
        y += dy;
        wait_frame();
        VIC.spr3_x = x;
        VIC.spr3_y = y;
    }
    wait_frame();
    VIC.spr_ena &= ~SHOT_BIT;
}
