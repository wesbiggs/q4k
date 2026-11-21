/*
 * Continent generator preview and check.
 *
 * Builds the land for a run of game seeds -- the four continents and home -- and keeps a checksum of every map, which tools/checkisle.py
 * reads back (--peek sums:160, town_sums:160) and compares against tools/isleref.py's model
 * of the generator. Then it draws one of them, so the layout can be looked at
 * without sailing there.
 *
 * It is drawn at half scale: each character is a 2x2 block of squares, a
 * port or a town or whatever there is to enter if there is one in it and
 * otherwise whatever is at its top left.
 *
 * It waits for a key before building, so that tools/screendump.py can poke
 * the settings in first, and publishes probe stage 2 when it is done:
 *
 *   tools/screendump.py dist/isletest.prg --poke show_seed=7 --poke show_dir=2 \
 *       --keys x --stage 2
 */

#include <conio.h>
#include <c64.h>

#include <q4k/charset.h>
#include <q4k/game.h>
#include <q4k/isle.h>
#include <q4k/probe.h>
#include <q4k/text.h>
#include <q4k/tiles.h>
#include <q4k/town_gen.h>
#include <q4k/vault.h>
#include <q4k/view.h>

/* Poke-able, so one binary serves every run. */
unsigned char first_seed = 1;     /* the first lore_seed to build   */
unsigned char seed_count = 16;    /* how many in a row, at most 16  */
unsigned char show_seed  = 1;     /* the continent drawn at the end */
unsigned char show_dir   = 1;
unsigned char home_y0;            /* home is shown 31 rows at a time, from this */

/* Where building home leaves its places. The game's game.c defined these;
   the rest of what the generators read is src/ow/owstand.c's, which the
   Makefile links ahead of the library. */
unsigned char home_fx[NUM_HOME_PLACES], home_fy[NUM_HOME_PLACES];

/* One per map: the four compass points of each seed, then home. */
unsigned int sums[80];
/* And the town of each, in the biome (seed + compass point) mod 5. */
unsigned int town_sums[80];

static char line[SCR_W + 1];

/* A checksum in which order matters. tools/checkisle.py does the same. */
static unsigned int checksum(void)
{
    unsigned int sum = 0, i;

    for (i = 0; i < ISLE_W * ISLE_H; ++i) {
        sum = ((sum << 1) | (sum >> 15)) + local[i];
    }
    return sum;
}

/* The same for home, which is not in local[]: row by row, through the last
   row of local[], which nothing else is using. */
static unsigned int checksum_home(void)
{
    unsigned int sum = 0;
    unsigned char y, x;
    unsigned char *row = local + LOCAL_AT(0, ISLE_H);

    for (y = 0; y < 64; ++y) {
        vault_row(y, row);
        for (x = 0; x < ISLE_W; ++x) {
            sum = ((sum << 1) | (sum >> 15)) + row[x];
        }
    }
    return sum;
}

/* The town in local[]: its squares and then its people, byte by byte. */
static unsigned int town_sum(void)
{
    unsigned int sum = 0;
    unsigned char y, x, i;
    const unsigned char *p = (const unsigned char *)here_ents;

    for (y = 0; y < LOCAL_H; ++y) {
        for (x = 0; x < LOCAL_W; ++x) {
            sum = ((sum << 1) | (sum >> 15)) + local[LOCAL_AT(x, y)];
        }
    }
    for (i = 0; i < here_n * 4; ++i) {
        sum = ((sum << 1) | (sum >> 15)) + p[i];
    }
    return sum;
}

/* Squares you can stand on. */
static unsigned int walkable(void)
{
    unsigned int n = 0, i;
    unsigned char f;

    for (i = 0; i < ISLE_W * ISLE_H; ++i) {
        f = tile_flags[local[i] - TILE_BASE];
        if (f == TF_PASS) {
            ++n;
        }
    }
    return n;
}

static void draw(void)
{
    unsigned char x, y, i, code, block;
    unsigned int  offset;
    char *p;

    for (y = 0; y + 1 < ISLE_H; y += 2) {
        for (x = 0; x < ISLE_W; x += 2) {
            block = local[LOCAL_AT(x, y)];
            for (i = 0; i < 4; ++i) {
                code = local[LOCAL_AT(x + (i & 1), y + (i >> 1))];
                if (tile_flags[code - TILE_BASE] == TF_ENTER) {
                    block = code;
                }
            }
            offset = (unsigned int)(y / 2) * SCR_W + x / 2;
            SCREEN_RAM[offset] = block;
            COLOR_RAM[offset]  = tile_colors[block - TILE_BASE];
        }
    }

    p = str_lit(line, "SEED ");
    p = str_num(p, show_seed);
    p = str_lit(p, "  COMPASS ");
    p = str_num(p, show_dir);
    p = str_lit(p, "  WALKABLE ");
    p = str_num(p, walkable());
    text_clear(0, ISLE_H / 2 + 1, SCR_W);
    text_put(0, ISLE_H / 2 + 1, line, COLOR_WHITE);

    p = str_lit(line, "BIOME ");
    p = str_num(p, isle_biome);
    p = str_lit(p, "  LANDING ");
    p = str_num(p, isle_dock_x);
    p = str_lit(p, ",");
    p = str_num(p, isle_dock_y);
    p = str_lit(p, "  SHIP FROM ");
    p = str_num(p, isle_sx);
    p = str_lit(p, ",");
    p = str_num(p, isle_sy);
    text_clear(0, ISLE_H / 2 + 2, SCR_W);
    text_put(0, ISLE_H / 2 + 2, line, COLOR_GRAY2);
}

int main(void)
{
    unsigned char s, d;

    tiles_install();
    VIC.bordercolor = COLOR_BLACK;
    VIC.bgcolor0    = COLOR_BLACK;
    VIC.addr        = CHARSET_D018;
    clrscr();

    while (kbhit()) {
        cgetc();
    }

    text_put(0, 0, "PRESS A KEY TO BUILD", COLOR_GRAY2);
    probe_stage(1);
    cgetc();
    clrscr();

    if (seed_count > 16) {
        seed_count = 16;
    }
    for (s = 0; s < seed_count; ++s) {
        lore_seed = first_seed + s;
        isle_story = lore_seed & 1;
        for (d = 0; d <= ISLE_HOME; ++d) {
            isle_build(d);
            sums[s * 5 + d] = d == ISLE_HOME ? checksum_home() : checksum();
            land = d;
            home_biome = isle_biome = (lore_seed + d) % 5;
            town_gen();
            town_sums[s * 5 + d] = town_sum();
        }
    }

    lore_seed = show_seed;
    isle_story = lore_seed & 1;
    isle_build(show_dir);
    if (show_dir == ISLE_HOME) {
        for (d = 0; d < ISLE_H; ++d) {
            vault_row(home_y0 + d, local + LOCAL_AT(0, d));
        }
    }
    draw();

    PROBE_SCRATCH[0] = seed_count;
    probe_stage(2);
    for (;;) {
    }
    return 0;
}
