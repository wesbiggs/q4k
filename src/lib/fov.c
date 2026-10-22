#include <string.h>

#include <q4k/dungeon.h>
#include <q4k/fov.h>
#include <q4k/game.h>
#include <q4k/tiles.h>

/* The tables below are written out for these sizes. */
#if DUN_W % 8 != 0 || DUN_H != 32 || DUN_W / 8 * DUN_H > 256 || MAX_DEEP != 4
#error "fov_row and fov_level are written out for a 32-row level, 4 deep"
#endif

unsigned char       fov_seen[FOV_BYTES];
/* In the BSS2 area ($CF14, above the RS-232 buffers, beside the probe page):
   fov_enter() clears it before anything reads it, so it needs no zero start. */
#pragma bss-name (push, "BSS2")
unsigned char       fov_lit[FOV_BYTES];
#pragma bss-name (pop)
const unsigned char fov_bit[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };

#define R(y) ((y) * FOV_ROW)
const unsigned char fov_row[DUN_H] = {
    R(0), R(1), R(2), R(3), R(4), R(5), R(6), R(7), R(8), R(9), R(10),
    R(11), R(12), R(13), R(14), R(15), R(16), R(17), R(18), R(19), R(20),
    R(21), R(22), R(23), R(24), R(25), R(26), R(27), R(28), R(29), R(30),
    R(31)
};

/* Every level of the last dungeon you were in. `mem_of` is that dungeon
   plus one, so the zero BSS starts with means nothing is remembered, and
   `mem_at` is the level fov_seen belongs to, less one. A table of where
   each level starts, rather than an index, keeps a multiply -- and the
   runtime helper it would drag into MAIN -- out of it. */
static unsigned char fov_mem[MAX_DEEP][FOV_BYTES];
static unsigned char *const fov_level[MAX_DEEP] = {
    fov_mem[0], fov_mem[1], fov_mem[2], fov_mem[3],
};
static unsigned char mem_of, mem_at;

void fov_enter(unsigned char dungeon, unsigned char level)
{
    if (mem_of == dungeon + 1) {
        memcpy(fov_level[mem_at], fov_seen, FOV_BYTES);
    } else {
        memset(fov_mem, 0, sizeof(fov_mem));
        mem_of = dungeon + 1;
    }
    mem_at = level - 1;
    memcpy(fov_seen, fov_level[mem_at], FOV_BYTES);
    memset(fov_lit, 0, FOV_BYTES);
}

void fov_forget(void)
{
    mem_of = 0;
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
       With a bit per square the whole level is 192 bytes, so wiping it is
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
