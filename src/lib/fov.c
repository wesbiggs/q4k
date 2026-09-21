#include <string.h>

#include <q4k/dungeon.h>
#include <q4k/fov.h>
#include <q4k/game.h>
#include <q4k/tiles.h>

unsigned char lit[LOCAL_W * LOCAL_H];

/* The squares lit last turn, so clearing them costs a walk over what was
 * actually visible -- eighty-odd cells -- instead of over the whole level.
 * A full sweep is the obvious way to write it and measurably the wrong one.
 *
 * Sized for the worst case: the largest room with its walls, the nine
 * squares around you, and four rays the length of the map. If it ever did
 * overflow, `flooded` falls back to the sweep rather than leaving squares
 * lit forever. */
#define MAX_LIT 200
static unsigned int  litlist[MAX_LIT];
static unsigned char litcount;
static unsigned char flooded;

void fov_reset(void)
{
    memset(lit, LIT_UNSEEN, sizeof(lit));
    litcount = 0;
    flooded = 0;
}

/* Coordinates arrive as unsigned, so a step off the left or top wraps to a
   large number and fails the same bounds test as a step off the right. */
static void show(unsigned char x, unsigned char y)
{
    unsigned int i;

    if (x >= LOCAL_W || y >= LOCAL_H) {
        return;
    }
    i = (unsigned int)y * LOCAL_W + x;
    if (lit[i] == LIT_VISIBLE) {
        return;                     /* already lit this turn */
    }
    lit[i] = LIT_VISIBLE;

    if (litcount < MAX_LIT) {
        litlist[litcount++] = i;
    } else {
        flooded = 1;
    }
}

/* Straight down a corridor until something stops the eye. The blocking
   square is shown -- you can see the wall you are about to walk into. */
static void ray(unsigned char x, unsigned char y,
                signed char dx, signed char dy)
{
    for (;;) {
        x += dx;
        y += dy;
        if (x >= LOCAL_W || y >= LOCAL_H) {
            return;
        }
        show(x, y);
        if (local[(unsigned int)y * LOCAL_W + x] == T_WALL) {
            return;
        }
    }
}

static void light_room(unsigned char i)
{
    unsigned char x, y;
    unsigned char x0 = dun_rx[i] ? dun_rx[i] - 1 : 0;
    unsigned char y0 = dun_ry[i] ? dun_ry[i] - 1 : 0;
    unsigned char x1 = dun_rx[i] + dun_rw[i];   /* the wall, not the floor */
    unsigned char y1 = dun_ry[i] + dun_rh[i];

    for (y = y0; y <= y1; ++y) {
        for (x = x0; x <= x1; ++x) {
            show(x, y);
        }
    }
}

void fov_update(unsigned char x, unsigned char y)
{
    unsigned int   i;
    unsigned char  r;

    /* What was lit last turn becomes memory. */
    if (flooded) {
        for (i = 0; i < LOCAL_W * LOCAL_H; ++i) {
            if (lit[i] == LIT_VISIBLE) {
                lit[i] = LIT_REMEMBERED;
            }
        }
        flooded = 0;
    } else {
        for (r = 0; r < litcount; ++r) {
            lit[litlist[r]] = LIT_REMEMBERED;
        }
    }
    litcount = 0;

    /* Always your own square and everything touching it. */
    for (r = 0; r < 9; ++r) {
        show(x - 1 + (r % 3), y - 1 + (r / 3));
    }

    for (r = 0; r < dun_rooms; ++r) {
        if (x >= dun_rx[r] && x < dun_rx[r] + dun_rw[r] &&
            y >= dun_ry[r] && y < dun_ry[r] + dun_rh[r]) {
            light_room(r);
        }
    }

    ray(x, y,  1,  0);
    ray(x, y, -1,  0);
    ray(x, y,  0,  1);
    ray(x, y,  0, -1);
}
