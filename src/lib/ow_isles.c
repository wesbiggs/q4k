/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Quest for the King -- the open sea's small islands, made by the
   bootstrap (q4k/isles.h): each cell with no land of its own gets three
   to six, and one of them a town or a dungeon, from lore_seed and the cell,
   into a table under the KERNAL, which the engine's world draws from
   (isl_draw.s, copied there too). A write under the KERNAL reaches the
   RAM, so nothing here banks. */

#include <string.h>

#include <q4k/game.h>
#include <q4k/isles.h>
#include <q4k/tiles.h>

extern unsigned char lore_seed;

/* The cells with land (world.c's grid): home in the middle of the second
   row, the continents two cells off it, a bit a column. */
static const unsigned char has_land[4] = { 0x04, 0x15, 0x04, 0x00 };

/* The grounds an island is of, and the second one some of its squares
   are: mountains only among the second. */
static const unsigned char ground[8] = {
    T_GRASS, T_FOREST, T_SHORE, T_SWAMP, T_GRASS, T_FOREST, T_GRASS, T_SHORE
};
static const unsigned char second[8] = {
    T_FOREST, T_MOUNTAIN, T_GRASS, T_SHORE, T_SWAMP, T_FOREST, T_GRASS,
    T_MOUNTAIN
};
static const unsigned int bit[16] = {
    1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384,
    32768u
};

/* A random byte: an LFSR of its own (as isle_rnd.s's, taps $B400), which
   touches neither rand() nor the land generator's. */
static unsigned int r;

static unsigned char rnd(void)
{
    static unsigned char i;

    for (i = 0; i < 8; ++i) {
        if (r & 1) {
            r = (r >> 1) ^ 0xB400;
        } else {
            r >>= 1;
        }
    }
    return (unsigned char)r;
}

/* 1 to 6, at even odds. */
static unsigned char one_to_six(void)
{
    static unsigned char v;

    do {
        v = rnd() & 7;
    } while (v > 5);
    return v + 1;
}

static unsigned char e[ISL_ENTRY];

void isles_make(void)
{
    unsigned char c, n, i, j, b, t, x, y, k, d, steps;
    unsigned int m;
    unsigned char *p;

    for (c = 0; c < ISL_CELLS; ++c) {
        memset(e, 0, sizeof e);
        if (!(has_land[c / 6] & (unsigned char)bit[c % 6])) {
            r = ((unsigned int)lore_seed << 8) + c * 41 + 1;
            rnd();
            rnd();
            n = 3 + (rnd() & 3);
            e[0] = n;
            p = e + 4;
            for (i = 0; i < n; ++i, p += 7) {
                /* A block of its own, within the cell's edge. */
                do {
                    b = one_to_six() * 8 + one_to_six();
                    for (j = 0; j < i && e[4 + j * 7] != b; ++j) {
                    }
                } while (j < i);
                p[0] = b;
                /* Its shape: a walk round the 4 by 4 from its top row, a
                   square at a time, until it has t of them. */
                do {
                    t = rnd() & 15;
                } while (!t);
                x = rnd() & 3;
                y = 0;
                if (i == 0) {
                    e[1] = rnd() & 1 ? T_TOWN : T_DUNGEON;
                    e[2] = x;           /* the place: k = 0 * 4 + x */
                }
                m = bit[x];
                k = 1;
                for (steps = 0; k < t && steps < 200; ++steps) {
                    d = rnd() & 3;
                    if (d == 0) {
                        if (x < 3) ++x;
                    } else if (d == 1) {
                        if (x) --x;
                    } else if (d == 2) {
                        if (y < 3) ++y;
                    } else if (y) {
                        --y;
                    }
                    if (!(m & bit[y * 4 + x])) {
                        m |= bit[y * 4 + x];
                        ++k;
                    }
                }
                p[1] = (unsigned char)m;
                p[2] = (unsigned char)(m >> 8);
                p[3] = ground[rnd() & 7];
                p[4] = second[rnd() & 7];
                p[5] = rnd() & rnd();   /* a quarter of the squares */
                p[6] = rnd() & rnd();
            }
        }
        memcpy((unsigned char *)ISL_TABLE + c * ISL_ENTRY, e, ISL_ENTRY);
    }
}
