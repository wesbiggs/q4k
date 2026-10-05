#include <c64.h>

#include <q4k/actor.h>
#include <q4k/game.h>
#include <q4k/tiles.h>
#include <q4k/view.h>
#include <q4k/world.h>

/* One row of palette indices, in the last row of local[]. That is not much
   of a loss: nothing uses it while you are out on the overworld, and the
   continents, which are built while the ship sails over it, are one row
   short of the buffer to leave this one alone. (The row is the whole of
   local[]'s stride.) */
#define ROW  (local + LOCAL_AT(0, LOCAL_ROWS - 1))

/* Unpack row y (wrapped) into ROW. */
static void world_row(unsigned char y)
{
    const unsigned char *s;
    unsigned char *d = ROW;
    unsigned char c, n, i, page;

    y &= WORLD_MASK;
    page = (y >= world_pages[0]) + (y >= world_pages[1]) + (y >= world_pages[2]);
    s = world_runs + ((unsigned int)page << 8 | world_rows[y]);
    i = 0;
    do {
        c = *s++;
        n = (c >> 4) + 1;
        c &= 0x0F;
        i += n;
        do {
            *d++ = c;
        } while (--n);
    } while (i < WORLD_W);
}

unsigned char world_tile(unsigned char x, unsigned char y)
{
    world_row(y);
    x &= WORLD_MASK;
    return world_tiles[ROW[x]];
}

/* Only the squares inside this rectangle are drawn from the map; everything
   outside it is sea. The ordinary view (world_draw) opens it right out, and the ship's voyage uses it
   to slide a coast off the screen, or on to it, without the far side of the
   world wrapping round to fill the gap. */
unsigned char world_x0, world_y0, world_x1, world_y1;

/* The viewport with (x,y) at its top-left. On the overworld that is any
   position, wrapping. With `isle` set it is the continent in local[], at the
   same 64 squares across but with nothing beyond its edges; both are given a
   bias of 64, which is a multiple of their sizes, so that a position a
   little off the map is still a small positive number. */
void world_view(unsigned char x, unsigned char y, unsigned char isle)
{
    unsigned char vx, vy, X, code;
    unsigned char *scr, *col;
    const unsigned char *row = 0;

    for (vy = 0; vy < VIEW_H; ++vy, ++y) {
        scr = SCREEN_RAM + (unsigned int)vy * SCR_W;
        col = COLOR_RAM  + (unsigned int)vy * SCR_W;
        if (y >= world_y0 && y <= world_y1) {
            if (isle) {
                row = local + LOCAL_AT(0, y & 31);
            } else {
                world_row(y);
                row = ROW;
            }
        } else {
            row = 0;
        }

        for (vx = 0, X = x; vx < VIEW_W; ++vx, ++X) {
            code = T_WATER;
            if (row && X >= world_x0 && X <= world_x1) {
                code = row[X & WORLD_MASK];
                if (!isle) {
                    code = world_tiles[code];
                }
            }
            *scr++ = code;
            *col++ = tile_colors[code - TILE_BASE];
        }
    }
}

void world_draw(void)
{
    world_x0 = world_y0 = 0;            /* all of it: the voyage narrows these */
    world_x1 = world_y1 = 255;
    world_view(hero.x - VIEW_CX, hero.y - VIEW_CY, 0);
}
