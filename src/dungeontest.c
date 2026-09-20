/*
 * Dungeon generator preview.
 *
 * Builds a level and draws it, so the layout can be looked at without
 * walking halfway across Keldmarch to reach one. Any key steps to the next
 * level, wrapping through all three dungeons.
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

static void draw(unsigned char which, unsigned char level)
{
    unsigned char x, y, code, floors = 0;
    unsigned int  offset;
    char *p;

    dungeon_build(which, level);

    for (y = 0; y < LOCAL_H; ++y) {
        for (x = 0; x < LOCAL_W; ++x) {
            code = local[(unsigned int)y * LOCAL_W + x];
            offset = (unsigned int)y * SCR_W + x;
            SCREEN_RAM[offset] = code;
            COLOR_RAM[offset]  = (code >= TILE_BASE + TILE_COUNT)
                               ? COLOR_LIGHTRED
                               : tile_colors[code - TILE_BASE];
            if (code == T_FLOOR) {
                ++floors;
            }
        }
    }

    p = str_lit(line, dungeon_names[which]);
    text_clear(22, 1, 18);
    text_put(22, 1, line, COLOR_YELLOW);

    p = str_lit(line, "LEVEL ");
    p = str_num(p, level);
    p = str_lit(p, " OF ");
    p = str_num(p, dungeon_deep[which]);
    text_clear(22, 3, 18);
    text_put(22, 3, line, COLOR_WHITE);

    p = str_lit(line, "FLOOR ");
    p = str_num(p, floors);
    text_clear(22, 5, 18);
    text_put(22, 5, line, COLOR_GRAY1);

    p = str_lit(line, "UP   ");
    p = str_num(p, dun_up_x);
    p = str_lit(p, ",");
    p = str_num(p, dun_up_y);
    text_clear(22, 7, 18);
    text_put(22, 7, line, COLOR_GRAY1);

    p = str_lit(line, "DOWN ");
    p = str_num(p, dun_dn_x);
    p = str_lit(p, ",");
    p = str_num(p, dun_dn_y);
    text_clear(22, 8, 18);
    text_put(22, 8, line, COLOR_GRAY1);

    PROBE_SCRATCH[0] = floors;
    PROBE_SCRATCH[1] = dun_up_x;
    PROBE_SCRATCH[2] = dun_up_y;
    PROBE_SCRATCH[3] = dun_dn_y;
    PROBE_SCRATCH[4] = local[(unsigned int)dun_up_y * LOCAL_W + dun_up_x];
    PROBE_SCRATCH[5] = local[(unsigned int)dun_dn_y * LOCAL_W + dun_dn_x];
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
}
