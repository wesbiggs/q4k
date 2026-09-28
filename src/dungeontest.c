/*
 * Dungeon generator preview.
 *
 * Builds a level and draws it, so the layout can be looked at without
 * walking halfway across Keldmarch to reach one. Any key steps to the next
 * level, wrapping through all three dungeons.
 *
 * A level is bigger than the screen, so it is drawn at half scale: each
 * character is a 2x2 block of squares, floor if any of them is, and the
 * stairs, a chest or the keeper if one of those is in it.
 *
 *   make shot PROG=dungeontest
 */

#include <conio.h>
#include <c64.h>

#include <q4k/charset.h>
#include <q4k/dungeon.h>
#include <q4k/game.h>
#include <q4k/probe.h>
#include <q4k/text.h>
#include <q4k/tiles.h>
#include <q4k/view.h>

static char line[SCR_W + 1];

#define PANEL  (DUN_W / 2 + 1)

static void draw(unsigned char which, unsigned char level)
{
    unsigned char x, y, i, code, block;
    unsigned int  offset, floors = 0;
    char *p;

    dungeon_build(which, level);

    for (y = 0; y < DUN_H; y += 2) {
        for (x = 0; x < DUN_W; x += 2) {
            block = T_WALL;
            for (i = 0; i < 4; ++i) {
                code = local[LOCAL_AT(x + (i & 1), y + (i >> 1))];
                if (code == T_FLOOR) {
                    ++floors;
                    if (block == T_WALL) {
                        block = T_FLOOR;
                    }
                } else if (code != T_WALL) {
                    block = code;
                }
            }
            offset = (unsigned int)(y / 2) * SCR_W + x / 2;
            SCREEN_RAM[offset] = block;
            COLOR_RAM[offset]  = (block >= TILE_BASE + TILE_COUNT)
                               ? COLOR_LIGHTRED
                               : tile_colors[block - TILE_BASE];
        }
    }

    p = str_lit(line, dungeon_names[which]);
    text_clear(0, DUN_H / 2 + 1, SCR_W);   /* too long for the panel */
    text_put(0, DUN_H / 2 + 1, line, COLOR_YELLOW);

    p = str_lit(line, "LEVEL ");
    p = str_num(p, level);
    p = str_lit(p, " OF ");
    p = str_num(p, dungeon_deep[which]);
    text_clear(PANEL, 3, SCR_W - PANEL);
    text_put(PANEL, 3, line, COLOR_WHITE);

    p = str_lit(line, "FLOOR ");
    p = str_num(p, floors);
    text_clear(PANEL, 5, SCR_W - PANEL);
    text_put(PANEL, 5, line, COLOR_GRAY1);

    p = str_lit(line, "UP   ");
    p = str_num(p, dun_up_x);
    p = str_lit(p, ",");
    p = str_num(p, dun_up_y);
    text_clear(PANEL, 7, SCR_W - PANEL);
    text_put(PANEL, 7, line, COLOR_GRAY1);

    p = str_lit(line, "DOWN ");
    p = str_num(p, dun_dn_x);
    p = str_lit(p, ",");
    p = str_num(p, dun_dn_y);
    text_clear(PANEL, 8, SCR_W - PANEL);
    text_put(PANEL, 8, line, COLOR_GRAY1);

    PROBE_SCRATCH[0] = (unsigned char)floors;
    PROBE_SCRATCH[1] = dun_up_x;
    PROBE_SCRATCH[2] = dun_up_y;
    PROBE_SCRATCH[3] = dun_dn_y;
    PROBE_SCRATCH[4] = local[LOCAL_AT(dun_up_x, dun_up_y)];
    PROBE_SCRATCH[5] = local[LOCAL_AT(dun_dn_x, dun_dn_y)];
}

int main(void)
{
    unsigned char which = 0, level = 1;

    tiles_install();
    VIC.bordercolor = COLOR_BLACK;
    VIC.bgcolor0    = COLOR_BLACK;
    VIC.addr        = CHARSET_D018;
    clrscr();

    while (kbhit()) {   /* the launcher's RUN and RETURN are not input */
        cgetc();
    }

    str_lit(line, "ANY KEY FOR THE NEXT LEVEL");
    text_put(0, 20, line, COLOR_GRAY2);

    for (;;) {
        draw(which, level);
        probe_stage(1);
        cgetc();

        if (++level > dungeon_deep[which]) {
            level = 1;
            if (++which >= NUM_DUNGEONS) {
                which = 0;
            }
        }
    }
    /* not reached; the loop is the program */
}
