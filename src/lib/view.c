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

/* A number on a row already cleared. */
static void put_num(unsigned char x, unsigned char y, unsigned int value)
{
    char buf[6];

    utoa(value, buf, 10);
    cputsxy(x, y, buf);
}

/* "20/20" as one field at (x,y), so the slash tracks the digit count, and
   blanks to the edge of the panel after it. */
static void put_pair(unsigned char x, unsigned char y,
                     unsigned char have, unsigned char of)
{
    char buf[12], num[6];

    utoa(have, buf, 10);
    strcat(buf, "/");
    utoa(of, num, 10);
    strcat(buf, num);

    /* To the screen edge and no further: one more and conio wraps onto the
       next row, blanking the map square at its left end every turn. */
    cclearxy(x, y, SCR_W - x);
    cputsxy(x, y, buf);
}

/* Two rows a member: the name and class, then health and a wizard's
   magic. In a fight, whoever's turn it is has a mark. */
static const char class_tag[NUM_CLASSES][4] = { "War", "Arc", "Wiz" };

static void put_member(unsigned char i)
{
    const Actor *a = &party[i];
    unsigned char y = i << 1;
    char buf[4];

    cclearxy(PANEL_X, y, PANEL_W);
    textcolor(COLOR_WHITE);
    if (party_turn == i) {
        cputcxy(PANEL_X, y, 0x3E);          /* > */
    }
    textcolor(party_turn == i ? COLOR_WHITE : COLOR_YELLOW);
    cputsxy(PANEL_X + 1, y, a->name);
    textcolor(COLOR_GRAY2);
    cputsxy(PANEL_X + 11, y, class_tag[a->cls]);

    textcolor(COLOR_GRAY1);
    cclearxy(PANEL_X, y + 1, PANEL_W);
    cputsxy(PANEL_X + 1, y + 1, "HP");
    textcolor(a->hp ? COLOR_WHITE : COLOR_LIGHTRED);
    put_pair(PANEL_X + 4, y + 1, a->hp, a->hp_max);
    if (a->mp_max) {
        textcolor(COLOR_GRAY1);
        cputsxy(PANEL_X + 12, y + 1, "MP");
        textcolor(COLOR_LIGHTBLUE);
        utoa(a->mp, buf, 10);
        cputsxy(PANEL_X + 15, y + 1, buf);
    }
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
    unsigned char i, y;

    for (i = 0; i < party_count; ++i) {
        put_member(i);
    }

    /* The party's: its level and experience, its purse, its draughts and
       its quiver. */
    y = party_count << 1;
    cclearxy(PANEL_X, y, PANEL_W);
    textcolor(COLOR_GRAY1);
    put_label(y, "Level");
    cputsxy(PANEL_X + 9, y, "Exp");
    textcolor(COLOR_WHITE);
    put_num(PANEL_X + 6, y, party_level);
    put_num(PANEL_X + 13, y, party_xp);

    ++y;
    cclearxy(PANEL_X, y, PANEL_W);
    textcolor(COLOR_GRAY1);
    put_label(y, "Gold");
    textcolor(COLOR_WHITE);
    put_num(PANEL_X + 6, y, gold);
    if (party_has_bow()) {
        textcolor(COLOR_GRAY1);
        cputsxy(PANEL_X + 12, y, "Arr");
        textcolor(COLOR_WHITE);
        put_num(PANEL_X + 16, y, arrows);
    }

    ++y;
    cclearxy(PANEL_X, y, PANEL_W);
    textcolor(COLOR_GRAY1);
    put_label(y, "Draughts");
    textcolor(COLOR_WHITE);
    put_num(VAL_X, y, potions);

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
