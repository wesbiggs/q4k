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

/* What the shop in front of you sells: the forge, the armoury and the
   bowyer each sell one line of goods, from index 1 up (0 is what you have
   before you buy anything), and remember the best you own in `level`. The
   village shop sells draughts, and has no `level`. */
static const char *const *names;
static const unsigned int *costs;
static unsigned char      *level;
static unsigned char       top;      /* highest index for sale */

static void stock(unsigned char kind)
{
    switch (kind) {
        case SHOP_WEAPON:
            names = weapon_names; costs = weapon_cost;
            level = &weapon_lv;   top = NUM_WEAPONS - 1;
            break;
        case SHOP_ARMOUR:
            names = armour_names; costs = armour_cost;
            level = &armour_lv;   top = NUM_ARMOUR - 1;
            break;
        case SHOP_BOW:
            names = bow_names;    costs = bow_cost;
            level = &bow_lv;      top = NUM_BOWS - 1;
            break;
        default:
            level = 0;            top = 1;
            break;
    }
}

static void draw(void)
{
    unsigned char i;
    char *p;

    if (level) {
        for (i = 1; i <= top; ++i) {
            p = str_lit(line, "  ");
            p = str_num(p, i);
            p = str_lit(p, "  ");
            p = str_lit(p, names[i]);
            p = str_pad(p, 26, line);
            p = str_num(p, costs[i]);
            p = str_lit(p, " marks");

            text_clear(0, LIST_Y + i - 1, SCR_W);
            text_put(0, LIST_Y + i - 1, line,
                     i == *level ? COLOR_LIGHTGREEN : COLOR_WHITE);
        }
    } else {
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
    if (!level) {
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
    unsigned char key, pick;
    unsigned int  cost;

    stock(kind);
    sprite_hide();
    clrscr();
    textcolor(COLOR_YELLOW);
    cputsxy(0, TITLE_Y, kind == SHOP_WEAPON ? "The Forge"
                      : kind == SHOP_ARMOUR ? "The King's Armoury"
                      : kind == SHOP_BOW    ? "The Bowyer"
                                            : "The Village Shop");
    textcolor(COLOR_GRAY1);
    chlinexy(0, TITLE_Y + 1, SCR_W);
    chlinexy(0, HINT_Y - 1, SCR_W);

    str_lit(line, "Number to buy, SPACE to leave");
    text_put(0, HINT_Y, line, COLOR_GRAY2);

    for (;;) {
        draw();

        key = cgetc();
        if (key == 0x20 || key == 0x03) {
            return;
        }
        if (key < 0x31 || key > 0x30 + top) {
            continue;
        }
        pick = key - 0x30;

        cost = level ? costs[pick] : potion_price();

        if (gold < cost) {
            note("You cannot afford that.", COLOR_LIGHTRED);
            continue;
        }
        if (level && pick <= *level) {
            note("You have better already.", COLOR_GRAY2);
            continue;
        }

        gold -= cost;
        if (level) {
            *level = pick;
        } else {
            ++potions;
        }
        note(kind == SHOP_WEAPON ? "A fair trade."
           : kind == SHOP_ARMOUR ? "Sign for it on the way out."
           : kind == SHOP_BOW    ? "Keep the string dry."
                                 : "Do not haggle.", COLOR_LIGHTGREEN);
    }
}
