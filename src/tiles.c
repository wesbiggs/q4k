/*
 * q4k -- tile set preview.
 *
 * Installs the custom charset and shows every tile next to its name, code and
 * a run of copies so you can see how it looks tiled. Run
 * `make shot PROG=tiles` and look at build/tiles.png.
 */

#include <conio.h>
#include <c64.h>

#include <q4k/charset.h>
#include <q4k/probe.h>
#include <q4k/tiles.h>

#define SCREEN_RAM ((unsigned char *)0x0400)
#define SCR_W      40

static const char hexdigit[] = "0123456789ABCDEF";

static void put_tile(unsigned char x, unsigned char y,
                     unsigned char code, unsigned char color)
{
    unsigned int offset = (unsigned int)y * SCR_W + x;
    SCREEN_RAM[offset] = code;
    COLOR_RAM[offset]  = color;
}

/* A scrap of landscape, to check the tiles read well side by side. Three
   rows, under the list: the list now runs to row 21. */
static const char patch[3][17] = {
    "~~,..T..FFF.^^^^",
    "~,......==...^^.",
    "~~,...%%..D..^..",
};

int main(void)
{
    unsigned char i, x, y, x0, code;

    tiles_install();

    VIC.bordercolor = COLOR_BLACK;
    VIC.bgcolor0    = COLOR_BLACK;
    VIC.addr        = CHARSET_D018;

    clrscr();
    textcolor(COLOR_LIGHTBLUE);
    cputsxy(0, 0, "TILE SET");

    /* Two columns: there are more tiles than rows. */
    for (i = 0; i < TILE_TOTAL; ++i) {
        x0 = (i & 1) ? 20 : 0;
        y = 1 + (i >> 1);
        code = tile_codes[i];

        textcolor(COLOR_GRAY1);
        cputsxy(x0, y, tile_names[i]);

        textcolor(COLOR_YELLOW);
        cputcxy(x0 + 9, y, '$');
        cputcxy(x0 + 10, y, hexdigit[code >> 4]);
        cputcxy(x0 + 11, y, hexdigit[code & 0x0F]);

        for (x = 0; x < 4; ++x) {
            put_tile(x0 + 13 + x, y, code, tile_all_colors[i]);
        }
    }

    for (y = 0; y < 3; ++y) {
        for (x = 0; x < 16; ++x) {
            switch (patch[y][x]) {
                case '~': code = T_WATER;    break;
                case ',': code = T_SHORE;    break;
                case 'F': code = T_FOREST;   break;
                case '^': code = T_MOUNTAIN; break;
                case 'T': code = T_TOWN;     break;
                case 'D': code = T_DUNGEON;  break;
                case '=': code = T_BRIDGE;   break;
                case '%': code = T_SWAMP;    break;
                default:  code = T_GRASS;    break;
            }
            put_tile(22 + x, 22 + y, code, tile_colors[code - TILE_BASE]);
        }
    }
    put_tile(26, 23, T_PLAYER, COLOR_WHITE);

    probe_stage(1);
    cgetc();
    return 0;
}
