#include <conio.h>
#include <c64.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/fov.h>
#include <q4k/game.h>
#include <q4k/isle.h>
#include <q4k/charset.h>
#include <q4k/sprite.h>
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

unsigned char town_drawn;

/* Come back from a screen something else owned, keeping the log. */
void view_restore(void)
{
    town_drawn = 0;
    clrscr();
    view_chrome();
    redraw_log();
}

/* The charset is installed by then (boot.c, before the title screen), and the
   code that did it may be gone. */
void view_init(void)
{
    VIC.bordercolor = COLOR_BLACK;
    VIC.bgcolor0    = COLOR_BLACK;
    VIC.addr        = CHARSET_D018;

    clrscr();
    memset(log_lines, 0, sizeof(log_lines));
    view_chrome();
    sprite_init();
}

#define VAL_X  (PANEL_X + 9)

/* Values sit in a fixed column, cleared first so a shorter number never
   leaves a digit of the previous one behind. */
void view_num(unsigned char x, unsigned char y, unsigned int value)
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

    /* To the screen edge and no further: one more and conio wraps onto the
       next row, blanking the map square at its left end every turn. */
    cclearxy(VAL_X, y, SCR_W - VAL_X);
    cputsxy(VAL_X, y, buf);
}

/* Key items, packed so they read as a list of what you have rather than a
   column of mostly-blank slots. Dragonbane is not one of them: it is the
   weapon line (weapon_names[DRAGONBANE]), and a seventh row would land on
   the rule at RULE_Y. */
static void put_items(unsigned char y)
{
    static const char *const labels[6] = {
        "Iron Sigil", "Tidal Key", "Ashen Key",
        "Emberstone", "Silver Tear", "Dragon Heart",
    };
    unsigned char i, bit = 1, row = y;

    for (i = 0; i < 6; ++i, bit <<= 1) {
        if (!(items & bit)) {
            continue;
        }
        cclearxy(PANEL_X, row, PANEL_W);
        textcolor(COLOR_LIGHTGREEN);
        cputsxy(PANEL_X, row, labels[i]);
        ++row;
    }
    while (row <= 18) {
        cclearxy(PANEL_X, row++, PANEL_W);
    }
}

/* The panel is eighteen columns by nineteen rows and every one is spoken
   for, so the blank lines between groups are the only spacing there is. */
unsigned char view_fighting;

/* Where you are (draw.c): only the map's engine knows how to name it, and
   that engine is on the disk, so the core is told where it is when the engine
   starts. The fight never draws it, and a program with no map (the arena)
   never sets it. */
void (*view_place_fn)(void);

void view_draw_panel(void)
{
    textcolor(COLOR_YELLOW);
    cputsxy(PANEL_X, 0, hero.name);

    textcolor(COLOR_GRAY1);
    put_label(1, "HP");
    put_label(2, "Level");
    put_label(3, "Exp");
    put_label(4, "Gold");
    put_label(5, "Draughts");

    textcolor(COLOR_WHITE);
    put_hp(1);
    view_num(VAL_X, 2, hero.level);
    view_num(VAL_X, 3, hero.xp);
    view_num(VAL_X, 4, gold);
    view_num(VAL_X, 5, potions);

    /* What you are actually carrying, which the shop screen otherwise
       keeps to itself. */
    textcolor(COLOR_CYAN);
    cclearxy(PANEL_X, 7, PANEL_W);
    cputsxy(PANEL_X, 7, weapon_names[weapon_lv]);
    cclearxy(PANEL_X, 8, PANEL_W);
    cputsxy(PANEL_X, 8, armour_names[armour_lv]);
    cclearxy(PANEL_X, 9, PANEL_W);
    if (bow_lv) {
        cputsxy(PANEL_X, 9, bow_names[bow_lv]);
        view_num(PANEL_X + 10, 9, arrows);
    }

    if (!view_fighting && view_place_fn) {
        view_place_fn();
    }

    put_items(13);
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
