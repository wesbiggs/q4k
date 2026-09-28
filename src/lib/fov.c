#include <string.h>

#include <q4k/dungeon.h>
#include <q4k/fov.h>
#include <q4k/game.h>
#include <q4k/tiles.h>

unsigned char       fov_seen[FOV_BYTES];
unsigned char       fov_lit[FOV_BYTES];
const unsigned char fov_bit[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };

void fov_reset(void)
{
    memset(fov_seen, 0, FOV_BYTES);
    memset(fov_lit, 0, FOV_BYTES);
}

/* Coordinates arrive as unsigned, so a step off the left or top wraps to a
   large number and fails the same bounds test as a step off the right. */
static void show(unsigned char x, unsigned char y)
{
    unsigned char b, m;

    if (x >= DUN_W || y >= DUN_H) {
        return;
    }
    b = FOV_BYTE(x, y);
    m = fov_bit[x & 7];
    fov_lit[b]  |= m;
    fov_seen[b] |= m;
}

/* Straight down a corridor until something stops the eye. The blocking
   square is shown -- you can see the wall you are about to walk into. */
static void ray(unsigned char x, unsigned char y,
                signed char dx, signed char dy)
{
    for (;;) {
        x += dx;
        y += dy;
        if (x >= DUN_W || y >= DUN_H) {
            return;
        }
        show(x, y);
        if (local[LOCAL_AT(x, y)] == T_WALL) {
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
    unsigned char r;

    /* What was lit last turn becomes memory: it is already in fov_seen.
       With a bit per square the whole level is 256 bytes, so wiping it is
       cheaper than the list of lit squares that used to be kept, and has
       no worst case to overflow. */
    memset(fov_lit, 0, FOV_BYTES);

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
