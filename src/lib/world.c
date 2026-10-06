#include <c64.h>

#include <q4k/actor.h>
#include <q4k/game.h>
#include <q4k/tiles.h>
#include <q4k/vault.h>
#include <q4k/view.h>
#include <q4k/world.h>

/* One row of home, copied out of the vault: the last row of local[]. That is
   not much of a loss: nothing uses it while you are out on the overworld, and
   the continents, which are built while the ship sails over home, are one row
   short of the buffer to leave this one alone. (The row is the whole of
   local[]'s stride.) */
#define ROW  (local + LOCAL_AT(0, LOCAL_ROWS - 1))

unsigned char world_tile(unsigned char x, unsigned char y)
{
    if (x >= WORLD_SIZE || y >= WORLD_SIZE) {
        return T_WATER;
    }
    return vault_tile(x, y);
}

/* Only the squares inside this rectangle are drawn from the map; everything
   outside it is sea. The ordinary view (world_draw) opens it right out, and the ship's voyage uses it
   to slide a coast off the screen, or on to it. */
unsigned char world_x0, world_y0, world_x1, world_y1;

void world_view(unsigned char x, unsigned char y, unsigned char src)
{
    unsigned char vx, vy, X, code;
    unsigned char *scr, *col;
    const unsigned char *row = 0;

    for (vy = 0; vy < VIEW_H; ++vy, ++y) {
        scr = SCREEN_RAM + (unsigned int)vy * SCR_W;
        col = COLOR_RAM  + (unsigned int)vy * SCR_W;
        row = 0;
        if (y >= world_y0 && y <= world_y1) {
            if (src) {
                row = local + LOCAL_AT(0, y & 31);
            } else if ((unsigned char)(y - WORLD_BIAS) < WORLD_SIZE) {
                vault_row(y & (WORLD_SIZE - 1), ROW);
                row = ROW;
            }
        }

        for (vx = 0, X = x; vx < VIEW_W; ++vx, ++X) {
            code = T_WATER;
            if (row && X >= world_x0 && X <= world_x1
                && (src || (unsigned char)(X - WORLD_BIAS) < WORLD_SIZE)) {
                code = row[X & (WORLD_SIZE - 1)];
            }
            *scr++ = code;
            *col++ = tile_colors[code - TILE_BASE];
        }
    }
}

void world_draw(unsigned char x, unsigned char y)
{
    world_x0 = world_y0 = 0;            /* all of it: the voyage narrows these */
    world_x1 = world_y1 = 255;
    world_view(x + WORLD_BIAS, y + WORLD_BIAS, SRC_HOME);
}
