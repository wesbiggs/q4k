#include <conio.h>
#include <c64.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/game.h>
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

/* Towns and dungeon levels are exactly one viewport, so they never scroll:
   walk in and the whole place is in front of you. */
static void draw_local(void)
{
    unsigned char x, y, i, code;
    unsigned int  offset;

    for (y = 0; y < LOCAL_H; ++y) {
        for (x = 0; x < LOCAL_W; ++x) {
            offset = (unsigned int)y * SCR_W + x;
            code = local[(unsigned int)y * LOCAL_W + x];
            SCREEN_RAM[offset] = code;
            COLOR_RAM[offset]  = (code >= TILE_BASE + TILE_COUNT)
                               ? COLOR_LIGHTRED       /* the keeper */
                               : tile_colors[code - TILE_BASE];
        }
    }

    if (scene == SCENE_TOWN) {
        for (i = town_ent_first[place]; i < town_ent_first[place + 1]; ++i) {
            offset = (unsigned int)town_ents[i].y * SCR_W + town_ents[i].x;
            SCREEN_RAM[offset] = T_VILLAGER;
            COLOR_RAM[offset]  = (town_ents[i].kind == ENT_SHOP)
                               ? COLOR_YELLOW : COLOR_CYAN;
        }
    }

    offset = (unsigned int)hero.y * SCR_W + hero.x;
    SCREEN_RAM[offset] = hero.glyph;
    COLOR_RAM[offset]  = hero.color;
}

static void draw_world(void)
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

void view_draw(void)
{
    if (scene == SCENE_WORLD) {
        draw_world();
    } else {
        draw_local();
    }
}

#define VAL_X  (PANEL_X + 9)

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

/* Key items, packed so they read as a list of what you have rather than a
   column of mostly-blank slots. */
static void put_items(unsigned char y)
{
    static const char *const labels[7] = {
        "IRON SIGIL", "TIDAL KEY", "ASHEN KEY",
        "EMBERSTONE", "SILVER TEAR", "DRAGON HEART", "DRAGONBANE",
    };
    unsigned char i, bit = 1, row = y;

    for (i = 0; i < 7; ++i, bit <<= 1) {
        if (!(items & bit)) {
            continue;
        }
        cclearxy(PANEL_X, row, PANEL_W);
        textcolor(i == 6 ? COLOR_YELLOW : COLOR_LIGHTGREEN);
        cputsxy(PANEL_X, row, labels[i]);
        ++row;
    }
    while (row <= 18) {
        cclearxy(PANEL_X, row++, PANEL_W);
    }
}

/* The panel is eighteen columns by nineteen rows and every one is spoken
   for, so the blank lines between groups are the only spacing there is. */
void view_draw_panel(void)
{
    textcolor(COLOR_YELLOW);
    cputsxy(PANEL_X, 0, hero.name);

    textcolor(COLOR_GRAY1);
    put_label(1, "HP");
    put_label(2, "LEVEL");
    put_label(3, "EXP");
    put_label(4, "GOLD");
    put_label(5, "DRAUGHTS");

    textcolor(COLOR_WHITE);
    put_hp(1);
    put_num(VAL_X, 2, hero.level);
    put_num(VAL_X, 3, hero.xp);
    put_num(VAL_X, 4, gold);
    put_num(VAL_X, 5, potions);

    /* What you are actually carrying, which the shop screen otherwise
       keeps to itself. */
    textcolor(COLOR_CYAN);
    cclearxy(PANEL_X, 7, PANEL_W);
    if (has(IT_BLADE)) {
        cputsxy(PANEL_X, 7, "DRAGONBANE");
    } else {
        cputsxy(PANEL_X, 7, weapon_names[weapon_lv]);
    }
    cclearxy(PANEL_X, 8, PANEL_W);
    cputsxy(PANEL_X, 8, armour_names[armour_lv]);

    /* Where you are: the land, a town by name, or how deep you have gone. */
    cclearxy(PANEL_X, 10, PANEL_W);
    cclearxy(PANEL_X, 11, PANEL_W);
    textcolor(COLOR_LIGHTBLUE);
    if (scene == SCENE_TOWN) {
        cputsxy(PANEL_X, 10, town_names[place]);
    } else if (scene == SCENE_DUNGEON) {
        cputsxy(PANEL_X, 10, dungeon_names[place]);
        cputsxy(PANEL_X, 11, "LEVEL");
        put_num(PANEL_X + 6, 11, depth);
    } else {
        cputsxy(PANEL_X, 10, "KELDMARCH");
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
