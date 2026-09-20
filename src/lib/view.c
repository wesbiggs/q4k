#include <conio.h>
#include <c64.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/charset.h>
#include <q4k/tiles.h>
#include <q4k/view.h>
#include <q4k/world.h>

extern unsigned int game_turn;   /* defined by the main program */

static char log_lines[MSG_H][SCR_W + 1];

static void redraw_log(void)
{
    unsigned char i;

    textcolor(COLOR_LIGHTGREEN);
    for (i = 0; i < MSG_H; ++i) {
        cclearxy(0, MSG_Y + i, SCR_W);
        cputsxy(0, MSG_Y + i, log_lines[i]);
    }
}

void view_chrome(void)
{
    unsigned char y;

    /* Chrome: a vertical rule beside the map, a horizontal one above the log.
       The vertical bar is written as a raw screen code -- cputc() would treat
       $5D as PETSCII and convert it, landing on ']' instead. */
    for (y = 0; y < VIEW_H; ++y) {
        SCREEN_RAM[(unsigned int)y * SCR_W + SEP_X] = 0x5D;
        COLOR_RAM[(unsigned int)y * SCR_W + SEP_X]  = COLOR_GRAY1;
    }
    textcolor(COLOR_GRAY1);
    chlinexy(0, RULE_Y, SCR_W);
}

/* Come back from a screen something else owned, keeping the log. */
void view_restore(void)
{
    clrscr();
    view_chrome();
    redraw_log();
}

void view_init(void)
{
    tiles_install();

    VIC.bordercolor = COLOR_BLACK;
    VIC.bgcolor0    = COLOR_BLACK;
    VIC.addr        = CHARSET_D018;

    clrscr();
    memset(log_lines, 0, sizeof(log_lines));
    view_chrome();
}

void view_draw_map(void)
{
    unsigned char vx, vy, wx, wy, code;
    unsigned int  row_base;
    unsigned char *scr, *col;

    for (vy = 0; vy < VIEW_H; ++vy) {
        /* Unsigned wraparound is exactly what we want at the world edges:
           the mask folds -1 to WORLD_W-1 for free. */
        wy = (hero.y + vy - VIEW_CY) & WORLD_MASK;
        row_base = (unsigned int)wy << WORLD_SHIFT;

        scr = SCREEN_RAM + (unsigned int)vy * SCR_W;
        col = COLOR_RAM  + (unsigned int)vy * SCR_W;

        for (vx = 0; vx < VIEW_W; ++vx) {
            wx = (hero.x + vx - VIEW_CX) & WORLD_MASK;
            code = world_map[row_base + wx];
            *scr++ = code;
            *col++ = tile_colors[code - TILE_BASE];
        }
    }

    scr = SCREEN_RAM + (unsigned int)VIEW_CY * SCR_W + VIEW_CX;
    col = COLOR_RAM  + (unsigned int)VIEW_CY * SCR_W + VIEW_CX;
    *scr = hero.glyph;
    *col = hero.color;
}

#define VAL_X  (PANEL_X + 7)

/* Values sit in a fixed column, cleared first so a shorter number never
   leaves a digit of the previous one behind. */
static void put_num(unsigned char x, unsigned char y, unsigned int value)
{
    char buf[6];

    utoa(value, buf, 10);
    cclearxy(x, y, 6);
    cputsxy(x, y, buf);
}

static void put_label(unsigned char y, const char *label)
{
    cputsxy(PANEL_X, y, label);
}

/* "20/20" as one field, so the slash tracks the digit count. */
static void put_hp(unsigned char y)
{
    char buf[12], num[6];

    utoa(hero.hp, buf, 10);
    strcat(buf, "/");
    utoa(hero.hp_max, num, 10);
    strcat(buf, num);

    cclearxy(VAL_X, y, 10);
    cputsxy(VAL_X, y, buf);
}

void view_draw_panel(void)
{
    unsigned char code = world_tile(hero.x, hero.y);

    textcolor(COLOR_YELLOW);
    cputsxy(PANEL_X, 0, hero.name);

    textcolor(COLOR_GRAY1);
    put_label(2, "HP");
    put_label(3, "LEVEL");
    put_label(4, "EXP");
    put_label(6, "X");
    put_label(7, "Y");
    put_label(8, "TURN");
    put_label(10, "TERRAIN");

    textcolor(COLOR_WHITE);
    put_hp(2);
    put_num(VAL_X, 3, hero.level);
    put_num(VAL_X, 4, hero.xp);
    put_num(VAL_X, 6, hero.x);
    put_num(VAL_X, 7, hero.y);
    put_num(VAL_X, 8, game_turn);

    textcolor(tile_colors[code - TILE_BASE]);
    cputsxy(PANEL_X, 11, "          ");
    cputsxy(PANEL_X, 11, tile_names[code - TILE_BASE]);
}

void view_message(const char *text)
{
    unsigned char i;

    for (i = 0; i < MSG_H - 1; ++i) {
        strcpy(log_lines[i], log_lines[i + 1]);
    }
    strncpy(log_lines[MSG_H - 1], text, SCR_W);
    log_lines[MSG_H - 1][SCR_W] = '\0';

    redraw_log();
}
