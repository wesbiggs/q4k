#include <conio.h>
#include <c64.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/game.h>
#include <q4k/shop.h>
#include <q4k/sprite.h>
#include <q4k/text.h>
#include <q4k/view.h>

#define TITLE_Y   0
#define LIST_Y    3
#define PURSE_Y  16
#define NOTE_Y   18
#define HINT_Y   20

static char line[SCR_W + 1];

unsigned int potion_price(void)
{
    return 10u * hero.level;
}

static void note(const char *literal, unsigned char color)
{
    text_clear(0, NOTE_Y, SCR_W);
    str_lit(line, literal);
    text_put(0, NOTE_Y, line, color);
}

static void draw(unsigned char kind, unsigned char owned)
{
    unsigned char i, rows;
    char *p;

    rows = (kind == SHOP_ITEM) ? 1 : NUM_WEAPONS;

    for (i = 1; i < rows; ++i) {
        p = str_lit(line, "  ");
        p = str_num(p, i);
        p = str_lit(p, "  ");
        p = str_lit(p, kind == SHOP_WEAPON ? weapon_names[i] : armour_names[i]);
        p = str_pad(p, 26, line);
        p = str_num(p, kind == SHOP_WEAPON ? weapon_cost[i] : armour_cost[i]);
        p = str_lit(p, " marks");

        text_clear(0, LIST_Y + i - 1, SCR_W);
        text_put(0, LIST_Y + i - 1, line,
                 i == owned ? COLOR_LIGHTGREEN : COLOR_WHITE);
    }

    if (kind == SHOP_ITEM) {
        p = str_lit(line, "  1  Healing draught");
        p = str_pad(p, 26, line);
        p = str_num(p, potion_price());
        p = str_lit(p, " marks");
        text_clear(0, LIST_Y, SCR_W);
        text_put(0, LIST_Y, line, COLOR_WHITE);
    }

    p = str_lit(line, "You have ");
    p = str_num(p, gold);
    p = str_lit(p, " marks");
    if (kind == SHOP_ITEM) {
        p = str_lit(p, ", ");
        p = str_num(p, potions);
        p = str_lit(p, " draughts");
    }
    p = str_lit(p, ".");
    text_clear(0, PURSE_Y, SCR_W);
    text_put(0, PURSE_Y, line, COLOR_YELLOW);
}

void shop_open(unsigned char kind)
{
    unsigned char key, pick, owned, top;
    unsigned int  cost;

    sprite_hide();
    clrscr();
    textcolor(COLOR_YELLOW);
    cputsxy(0, TITLE_Y, kind == SHOP_WEAPON ? "The Forge"
                      : kind == SHOP_ARMOUR ? "The King's Armoury"
                                            : "The Village Shop");
    textcolor(COLOR_GRAY1);
    chlinexy(0, TITLE_Y + 1, SCR_W);
    chlinexy(0, HINT_Y - 1, SCR_W);

    str_lit(line, "Number to buy, SPACE to leave");
    text_put(0, HINT_Y, line, COLOR_GRAY2);

    top = (kind == SHOP_ITEM) ? 1 : NUM_WEAPONS - 1;

    for (;;) {
        owned = (kind == SHOP_WEAPON) ? weapon_lv
              : (kind == SHOP_ARMOUR) ? armour_lv : 0;
        draw(kind, owned);

        key = cgetc();
        if (key == 0x20 || key == 0x03) {
            return;
        }
        if (key < 0x31 || key > 0x30 + top) {
            continue;
        }
        pick = key - 0x30;

        cost = (kind == SHOP_WEAPON) ? weapon_cost[pick]
             : (kind == SHOP_ARMOUR) ? armour_cost[pick]
                                     : potion_price();

        if (gold < cost) {
            note("You cannot afford that.", COLOR_LIGHTRED);
            continue;
        }
        if (kind != SHOP_ITEM && pick <= owned) {
            note("You have better already.", COLOR_GRAY2);
            continue;
        }

        gold -= cost;
        if (kind == SHOP_WEAPON) {
            weapon_lv = pick;
            note("A fair trade.", COLOR_LIGHTGREEN);
        } else if (kind == SHOP_ARMOUR) {
            armour_lv = pick;
            note("Sign for it on the way out.", COLOR_LIGHTGREEN);
        } else {
            ++potions;
            note("Do not haggle.", COLOR_LIGHTGREEN);
        }
    }
}
