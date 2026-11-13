#include <conio.h>
#include <c64.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/engine.h>
#include <q4k/game.h>
#include <q4k/isle.h>
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
    return 5u + 5u * hero.level;
}

static void note(const char *literal, unsigned char color)
{
    text_clear(0, NOTE_Y, SCR_W);
    str_lit(line, literal);
    text_put(0, NOTE_Y, line, color);
}

/* The prices, by tier as battle.c names them; nothing sells Dragonbane.
   Here rather than with the names, which every engine reads (arms.c), because
   only the shop needs them. */
static const unsigned int weapon_cost[NUM_WEAPONS] = {
    0, 25, 80, 200, 420, 700, 1000, 0,
};
static const unsigned int armour_cost[NUM_ARMOUR] = {
    0, 30, 110, 260, 500, 800, 1100,
};
static const unsigned int bow_cost[NUM_BOWS + 1] = {
    0, 80, 150, 350, 650, ARROW_PRICE,
};

/* What each land's shops sell, and its monsters drop: home first, then the
   continents in the order the story opens them (slot_of()). Blades and mail
   go by the same tiers; bows have fewer. */
static const unsigned char tier_lo[5] = { 1, 1, 2, 3, 4 };
static const unsigned char tier_hi[5] = { 2, 3, 4, 5, 6 };
static const unsigned char bow_lo[5]  = { 1, 1, 2, 2, 3 };
static const unsigned char bow_hi[5]  = { 1, 2, 3, 3, 4 };

/* What the shop in front of you sells: the forge, the armoury and the
   bowyer each sell one line of goods, tiers `lo` to `hi` of it (0 is what you
   have before you buy anything), and remember the best you own in `level`.
   The bowyer lists the arrows after the bows. The village shop sells
   draughts, and has no `level`. `rows` is how many lines the list has. */
static const char *const *names;
static const unsigned int *costs;
static unsigned char      *level;
static unsigned char       lo, hi, rows, bow;

static void stock(unsigned char kind)
{
    unsigned char t = land == ISLE_HOME ? 0 : 1 + slot_of(land);

    bow   = kind == SHOP_BOW;
    lo    = tier_lo[t];
    hi    = tier_hi[t];
    level = 0;
    rows  = 1;
    if (kind == SHOP_WEAPON) {
        names = weapon_names; costs = weapon_cost;
        level = &weapon_lv;
    } else if (kind == SHOP_ARMOUR) {
        names = armour_names; costs = armour_cost;
        level = &armour_lv;
    } else if (bow) {
        names = bow_names;    costs = bow_cost;
        level = &bow_lv;
        lo = bow_lo[t];
        hi = bow_hi[t];
    }
    if (level) {
        rows = hi - lo + 1 + bow;
    }
}

/* What line `row` (from 1) of the list is. */
static unsigned char item(unsigned char row)
{
    return bow && row == rows ? NUM_BOWS : lo + row - 1;
}

/* Take up tier `pick` of what stock() chose, if it is better than what you
   have; a first bow comes strung. */
static unsigned char better(unsigned char pick)
{
    if (pick <= *level) {
        return 0;
    }
    if (bow && !*level) {
        arrows += ARROW_FREE;
    }
    *level = pick;
    return 1;
}

/* One beaten monster in this many that carries anything leaves a blade, mail
   or a bow of what this land's shops sell. Not constant, so that a test can
   pin it with --poke gear_odds=1. */
unsigned char gear_odds = 12;

/* What gear_drop() used to do once its roll had come up, which the map now
   makes before loading this engine (fightcall.c). */
static void gear_give(void)
{
    unsigned char k, pick;

    k = (unsigned char)(rand() % 3);
    stock(k == 2 ? SHOP_BOW : k);
    pick = lo + (unsigned char)(rand() % (hi - lo + 1));
    if (better(pick)) {
        strcpy(line, "You take up its ");
        strcat(line, names[pick]);
        strcat(line, "!");
    } else {
        strcpy(line, "Its ");
        strcat(line, names[pick]);
        strcat(line, " is no use to you.");
    }
    view_message(line);
}

/* The shop's engine: engine.h. */
unsigned char shop_entry(void)
{
    if (eng_a == SHOP_GEAR) {
        gear_give();
    } else {
        shop_open(eng_a);
    }
    return 0;
}

static void draw(void)
{
    unsigned char i, k;
    char *p;

    if (level) {
        for (i = 1; i <= rows; ++i) {
            k = item(i);
            p = str_lit(line, "  ");
            p = str_num(p, i);
            p = str_lit(p, "  ");
            p = str_lit(p, names[k]);
            p = str_pad(p, 26, line);
            p = str_num(p, costs[k]);
            p = str_lit(p, " marks");

            text_clear(0, LIST_Y + i - 1, SCR_W);
            text_put(0, LIST_Y + i - 1, line,
                     k == *level ? COLOR_LIGHTGREEN : COLOR_WHITE);
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
    cputsxy(0, TITLE_Y, kind == SHOP_WEAPON ? "The Smithy"
                      : kind == SHOP_ARMOUR ? "The Armourer"
                      : kind == SHOP_BOW    ? "The Bowyer"
                                            : "The Apothecary");
    textcolor(COLOR_GRAY1);
    chlinexy(0, TITLE_Y + 1, SCR_W);
    chlinexy(0, HINT_Y - 1, SCR_W);

    str_lit(line, "Number to buy, RUN/STOP leaves");
    text_put(0, HINT_Y, line, COLOR_GRAY2);

    for (;;) {
        draw();

        key = cgetc();
        if (key == 0x03) {
            return;
        }
        if (key < 0x31 || key > 0x30 + rows) {
            continue;
        }
        pick = item(key - 0x30);

        cost = level ? costs[pick] : potion_price();

        if (gold < cost) {
            note("You cannot afford that.", COLOR_LIGHTRED);
            continue;
        }
        if (pick == NUM_BOWS) {
            if (!bow_lv || arrows > ARROW_MAX - ARROW_PACK) {
                note(bow_lv ? "Your quiver is full." : "You have no bow.",
                     COLOR_GRAY2);
            } else {
                gold -= cost;
                arrows += ARROW_PACK;
                note("Straight ones, mind.", COLOR_LIGHTGREEN);
            }
            continue;
        }
        if (level) {
            if (!better(pick)) {
                note("You have better already.", COLOR_GRAY2);
                continue;
            }
        } else {
            ++potions;
        }
        gold -= cost;
        note("A fair trade.", COLOR_LIGHTGREEN);
    }
}
