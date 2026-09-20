/*
 * q4k -- hello world / toolchain smoke test
 *
 * Proves the whole pipeline end to end:
 *   cc65 C -> .prg -> BASIC autostart stub -> VIC registers -> conio text
 *   -> direct screen+color RAM writes (the technique the tile engine uses)
 *   -> keyboard input.
 */

#include <conio.h>
#include <c64.h>
#include <string.h>

#include <q4k/probe.h>

/* Default VIC-II bank: screen codes at $0400. COLOR_RAM ($D800) is c64.h's. */
#define SCREEN_RAM ((unsigned char *)0x0400)
#define SCR_W      40
#define SCR_H      25

/* Screen codes for the lowercase/mixed charset (see `make shot PROG=charset`).
   Roguelike-style glyphs for now; the real tile art will be a custom charset
   uploaded to $3000, which is how Ultima III got its look. */
#define SC_SPACE   0x20  /* ' '  */
#define SC_GRASS   0x2E  /* '.'  */
#define SC_TREE    0x54  /* 'T'  (0x41-0x5A are A-Z in this charset) */
#define SC_WATER   0xA0  /* reversed space -- a solid block, coloured blue */
#define SC_WALL    0x23  /* '#'  */
#define SC_PLAYER  0x00  /* '@'  */

/* Poke one cell of the tile map: screen code + color, one shot. */
static void put_tile(unsigned char x, unsigned char y,
                     unsigned char code, unsigned char color)
{
    unsigned int offset = (unsigned int)y * SCR_W + x;
    SCREEN_RAM[offset] = code;
    COLOR_RAM[offset]  = color;
}

/* A 9x7 chunk of "map" drawn straight into screen RAM. */
static void draw_demo_map(unsigned char ox, unsigned char oy)
{
    static const char map[7][10] = {
        "#########",
        "#..~~...#",
        "#..~~..T#",
        "#......T#",
        "#T....TT#",
        "#.......#",
        "#########",
    };
    unsigned char x, y, code, color;

    for (y = 0; y < 7; ++y) {
        for (x = 0; x < 9; ++x) {
            switch (map[y][x]) {
                case '#': code = SC_WALL;  color = COLOR_GRAY2;  break;
                case '~': code = SC_WATER; color = COLOR_BLUE;   break;
                case 'T': code = SC_TREE;  color = COLOR_GREEN;  break;
                default:  code = SC_GRASS; color = COLOR_GREEN;  break;
            }
            put_tile(ox + x, oy + y, code, color);
        }
    }
    /* Party marker, Ultima-style: always centered on its own tile. */
    put_tile(ox + 4, oy + 3, SC_PLAYER, COLOR_WHITE);
}

int main(void)
{
    VIC.bordercolor = COLOR_BLACK;
    VIC.bgcolor0    = COLOR_BLACK;

    /* cc65's conio emits screen codes for the lowercase/mixed charset, where
       $41-$5A are A-Z. The C64 boots in the uppercase/graphics charset, where
       those same codes are line-art -- so conio text comes out as garbage
       until the VIC is pointed at the lowercase set.
       $D018: screen at $0400 (bits 7-4 = %0001), charset at $1800 (bits 3-1
       = %011). */
    VIC.addr = 0x17;

    clrscr();

    textcolor(COLOR_LIGHTBLUE);
    cputsxy(0, 0, "HELLO, WORLD");

    textcolor(COLOR_GRAY1);
    cputsxy(0, 2, "Q4K TOOLCHAIN SMOKE TEST");
    cputsxy(0, 3, "CC65 + VICE, C64 TARGET");

    draw_demo_map(15, 8);

    textcolor(COLOR_YELLOW);
    cputsxy(0, 22, "DIRECT SCREEN RAM WRITES: OK");
    textcolor(COLOR_GRAY2);
    cputsxy(0, 24, "PRESS A KEY");

    probe_stage(1);   /* screen painted -- the harness reads it here */
    cgetc();
    probe_stage(2);   /* key received */

    textcolor(COLOR_LIGHTGREEN);
    cputsxy(0, 24, "KEYBOARD: OK        ");
    return 0;
}
