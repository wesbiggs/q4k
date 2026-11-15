/*
 * Open world: the bootstrap of the smooth-scrolling prototype.
 *
 * Run from its disk (dist/openworld.d64, the first file). It looks for an
 * REU, builds Keldmarch and the four continents with the game's generator
 * into the RAM under the ROMs, moors a ship beside each port and landing,
 * leaves all of that in the hand-off block (include/q4k/ow.h) and loads the
 * engine that suits the machine: OW-REU if there is an REU whose transfers
 * reach colour RAM, OW-CPU otherwise. The engines are src/ow/engine.c,
 * built twice; see there for the keys.
 *
 * An engine comes back here for N, with the next seed in the block, and
 * this builds that world instead of seed 1's.
 */

#include <conio.h>
#include <string.h>

#include <q4k/game.h>
#include <q4k/isle.h>
#include <q4k/ow.h>
#include <q4k/tiles.h>
#include <q4k/vault.h>

/* What the land generator and the vault read and write of game.c's, which
   is not linked: defined here. */
unsigned char lore_seed, land, local_w, local_h, quest_stage;
unsigned char home_fx[NUM_HOME_PLACES], home_fy[NUM_HOME_PLACES];

/* The continents' cells by compass point, and a continent's first row in
   its cell (src/ow/engine.c has the world's map). */
static const unsigned char cell_x[4] = { 2, 4, 2, 0 };
static const unsigned char cell_y[4] = { 0, 1, 2, 1 };
#define ISLE_TOP 16
#define ISLE_ROWS 31

/* Which continent holds which story place: a shuffle of the seed, as
   game.c's slot_of() has it. */
unsigned char slot_of(unsigned char d)
{
    unsigned char p[4], i, j, t, v = lore_seed;

    for (i = 0; i < 4; ++i) {
        p[i] = i;
    }
    for (i = 3; i > 0; --i) {
        v = v * 5 + 3;
        j = (v >> 5) % (i + 1);
        t = p[i]; p[i] = p[j]; p[j] = t;
    }
    return p[d];
}

/* Ship i beside landing (x, y): on the first square of water next to it,
   north, east, south or west. The land is home in the vault, or a
   continent still in local[]; (ox, oy) is where it starts in the world. */
static void moor(unsigned char i, unsigned char x, unsigned char y,
                 unsigned int ox, unsigned char oy, unsigned char home)
{
    static const signed char mx[4] = { 0, 1, 0, -1 };
    static const signed char my[4] = { -1, 0, 1, 0 };
    unsigned char d, nx, ny, t;

    for (d = 0; d < 4; ++d) {
        nx = x + mx[d];
        ny = y + my[d];
        if (nx >= 64 || ny >= (home ? 64 : ISLE_ROWS)) {
            continue;
        }
        t = home ? vault_tile(nx, ny) : local[LOCAL_AT(nx, ny)];
        if (t == T_WATER) {
            break;
        }
    }
    OW->ship_x[i] = ox + nx;
    OW->ship_y[i] = oy + ny;
}

int main(void)
{
    unsigned char d, again = OW->magic == OW_MAGIC;

    bgcolor(COLOR_BLACK);
    bordercolor(COLOR_BLACK);
    textcolor(COLOR_GRAY3);
    clrscr();
    cputs("Quest for the King\r\n"
          "the open world, at sea\r\n\r\n");

    lore_seed = again ? OW->seed : 1;
    OW->magic = 0;              /* until it is all there */
    OW->seed = lore_seed;
    OW->reu = ow_reu_detect();
    cputs(OW->reu == OW_NO_REU ? "No REU: the CPU scrolls.\r\n" :
          OW->reu == OW_REU_OK ? "An REU, and its DMA reaches colour\r\n"
                                 "RAM: the REU scrolls.\r\n" :
                                 "An REU, but its DMA does not reach\r\n"
                                 "colour RAM: the CPU scrolls.\r\n");

    cputs("\r\nBuilding the world");
    for (d = 0; d < 4; ++d) {   /* the continents, into the vault */
        cputc('.');
        OW->slot[d] = slot_of(d);
        isle_story = OW->slot[d] & 1;
        isle_build(d);
        OW->ipx[d][0] = isle_fx[0];
        OW->ipy[d][0] = isle_fy[0];
        OW->ipx[d][1] = isle_fx[1];
        OW->ipy[d][1] = isle_fy[1];
        OW->ibiome[d] = isle_biome;
        moor(4 + d, isle_dock_x, isle_dock_y, cell_x[d] * 64u,
             cell_y[d] * 64 + ISLE_TOP, 0);
        vault_park(d);
    }
    cputc('.');
    isle_build(ISLE_HOME);      /* and home, in place */
    OW->home_biome = isle_biome;
    memcpy(OW->home_fx, isle_fx, OW_PLACES);
    memcpy(OW->home_fy, isle_fy, OW_PLACES);
    for (d = 0; d < 4; ++d) {
        moor(d, isle_port_x[d], isle_port_y[d], 128, 64, 1);
    }
    OW->magic = OW_MAGIC;

    cputs(OW->reu == OW_REU_OK ? "\r\n\r\nLoading OW-REU" :
                                 "\r\n\r\nLoading OW-CPU");
    ow_run(OW->reu == OW_REU_OK ? "ow-reu" : "ow-cpu");
    return 0;
}
