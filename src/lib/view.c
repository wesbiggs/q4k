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
    sprite_init();
}

/* Where the viewport's top-left corner is in local[]. A dungeon level is
   bigger than the screen, so the view follows you like the overworld's,
   but stops at the edges instead of wrapping: it keeps you centred until
   there is no more map that way, and then you walk off the centre. A town
   is exactly one viewport, so both come out 0 and it never moves. */
static unsigned char cam_x, cam_y;

static unsigned char camera(unsigned char at, unsigned char centre,
                            unsigned char size, unsigned char view)
{
    if (at <= centre) {
        return 0;
    }
    at -= centre;
    return (at + view > size) ? size - view : at;
}

static void draw_local(void)
{
    unsigned char vx, vy, x, y, i, code, row, b, m;
    unsigned char *scr, *col;

    cam_x = camera(hero.x, VIEW_CX, local_w, VIEW_W);
    cam_y = camera(hero.y, VIEW_CY, local_h, VIEW_H);

    for (vy = 0; vy < VIEW_H; ++vy) {
        y   = cam_y + vy;
        row = fov_row[y];
        scr = SCREEN_RAM + (unsigned int)vy * SCR_W;
        col = COLOR_RAM  + (unsigned int)vy * SCR_W;

        for (vx = 0, x = cam_x; vx < VIEW_W; ++vx, ++x, ++scr, ++col) {
            code = local[LOCAL_AT(x, y)];

            /* Underground you draw what you can see and what you remember
               seeing, and nothing else. Above ground the whole place is in
               front of you. */
            if (scene == SCENE_DUNGEON) {
                b = row + (x >> 3);
                m = fov_bit[x & 7];
                if (!(fov_seen[b] & m)) {
                    *scr = 0x20;
                    *col = COLOR_BLACK;
                    continue;
                }
                if (!(fov_lit[b] & m)) {
                    *scr = code;
                    *col = COLOR_GRAY1;
                    continue;
                }
            }

            *scr = code;
            *col = (code >= TILE_BASE + TILE_COUNT)
                 ? COLOR_LIGHTRED       /* the keeper */
                 : tile_colors[code - TILE_BASE];
        }
    }

    /* Only towns have people in them, and towns never scroll. */
    if (scene == SCENE_TOWN) {
        for (i = 0; i < here_n; ++i) {
            x = here_ents[i].x;
            y = here_ents[i].y;
            SCREEN_RAM[(unsigned int)y * SCR_W + x] = T_VILLAGER;
            COLOR_RAM[(unsigned int)y * SCR_W + x]  =
                (here_ents[i].kind == ENT_NPC || here_ents[i].kind == ENT_VILLAGER)
                ? COLOR_CYAN : COLOR_YELLOW;
        }
    }
}

/* The hero is not in the map at all: the square they stand on is drawn like
   any other, and the sprite goes over the top of it. */
void view_draw(void)
{
    if (scene == SCENE_WORLD) {
        cam_x = camera(hero.x, VIEW_CX, WORLD_SIZE, VIEW_W);
        cam_y = camera(hero.y, VIEW_CY, WORLD_SIZE, VIEW_H);
        world_draw(cam_x, cam_y);
    } else {
        draw_local();
    }
    sprite_show(hero.x - cam_x, hero.y - cam_y);
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

    /* To the screen edge and no further: one more and conio wraps onto the
       next row, blanking the map square at its left end every turn. */
    cclearxy(VAL_X, y, SCR_W - VAL_X);
    cputsxy(VAL_X, y, buf);
}

/* Key items, packed so they read as a list of what you have rather than a
   column of mostly-blank slots. */
static void put_items(unsigned char y)
{
    static const char *const labels[7] = {
        "Iron Sigil", "Tidal Key", "Ashen Key",
        "Emberstone", "Silver Tear", "Dragon Heart", "Dragonbane",
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
unsigned char view_fighting;

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
    put_num(VAL_X, 2, hero.level);
    put_num(VAL_X, 3, hero.xp);
    put_num(VAL_X, 4, gold);
    put_num(VAL_X, 5, potions);

    /* What you are actually carrying, which the shop screen otherwise
       keeps to itself. */
    textcolor(COLOR_CYAN);
    cclearxy(PANEL_X, 7, PANEL_W);
    if (has(IT_BLADE)) {
        cputsxy(PANEL_X, 7, "Dragonbane");
    } else {
        cputsxy(PANEL_X, 7, weapon_names[weapon_lv]);
    }
    cclearxy(PANEL_X, 8, PANEL_W);
    cputsxy(PANEL_X, 8, armour_names[armour_lv]);
    cclearxy(PANEL_X, 9, PANEL_W);
    if (bow_lv) {
        cputsxy(PANEL_X, 9, bow_names[bow_lv]);
        put_num(PANEL_X + 10, 9, arrows);
    }

    if (!view_fighting) {
        /* Where you are: the land, a town by name, or how deep you have gone. */
        cclearxy(PANEL_X, 10, PANEL_W);
        cclearxy(PANEL_X, 11, PANEL_W);
        cclearxy(PANEL_X, 12, PANEL_W);     /* a fight's second row of health */
        textcolor(COLOR_LIGHTBLUE);
        if (scene == SCENE_TOWN) {
            cputsxy(PANEL_X, 10, town_name());
        } else if (scene == SCENE_DUNGEON) {
            cputsxy(PANEL_X, 10, dungeon_names[place]);
            cputsxy(PANEL_X, 11, "Level");
            put_num(PANEL_X + 6, 11, depth);
        } else if (scene == SCENE_ISLE) {
            cputsxy(PANEL_X, 10, isle_name());
        } else {
            cputsxy(PANEL_X, 10, "Keldmarch");
        }
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
