#include <c64.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/sprite.h>

#define SPR_FIGURE  ((unsigned char *)0x0340)   /* block 13 */
#define SPR_EDGE    ((unsigned char *)0x0380)   /* block 14 */
#define SPR_PTRS    ((unsigned char *)0x07F8)   /* screen matrix + $3F8 */

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
   covers the middle of it, so it only shows as a black edge. */
static void build(void)
{
    static unsigned int grown[10];
    const unsigned char *f = frames[facing][stride];
    unsigned char r;
    unsigned int  w;

    memset(grown, 0, sizeof(grown));
    for (r = 0; r < 8; ++r) {
        w = (unsigned int)f[r] << 7;          /* one pixel in from the left */
        SPR_FIGURE[(r + 1) * 3]     = (unsigned char)(w >> 8);
        SPR_FIGURE[(r + 1) * 3 + 1] = (unsigned char)w;

        w |= (w << 1) | (w >> 1);
        grown[r]     |= w;
        grown[r + 1] |= w;
        grown[r + 2] |= w;
    }
    for (r = 0; r < 10; ++r) {
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
}

void sprite_init(void)
{
    memset(SPR_FIGURE, 0, 64);
    memset(SPR_EDGE, 0, 64);

    VIC.spr_ena     &= ~0x03;
    VIC.spr_hi_x    &= ~0x03;   /* the map is cols 0-20: X never passes 255 */
    VIC.spr_mcolor  &= ~0x03;
    VIC.spr_exp_x   &= ~0x03;
    VIC.spr_exp_y   &= ~0x03;
    VIC.spr_bg_prio &= ~0x03;   /* in front of the text */
    VIC.spr1_color   = COLOR_BLACK;

    facing = DIR_DOWN;
    stride = 0;
    build();
}

void sprite_show(unsigned char col, unsigned char row)
{
    /* Sprite pointers sit just past the screen matrix, in bytes clrscr()
       leaves alone -- but set them every time rather than rely on that. */
    SPR_PTRS[0] = (unsigned char)((unsigned int)SPR_FIGURE >> 6);
    SPR_PTRS[1] = (unsigned char)((unsigned int)SPR_EDGE >> 6);

    spr_x = ORIGIN_X + (col << 3);
    spr_y = ORIGIN_Y + (row << 3);
    place();
    VIC.spr0_color = hero.color;
    VIC.spr_ena   |= 0x03;
}

void sprite_hide(void)
{
    VIC.spr_ena &= ~0x03;
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
