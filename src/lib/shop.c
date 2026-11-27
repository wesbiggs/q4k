/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * q4k -- the shop's engine: buying things, and the screens the map opens
 * that are not a place (engine.h).
 *
 * The forge, the armoury and the bowyer each sell one line of goods, a few
 * tiers of it by land; the apothecary sells draughts and the magic shop
 * robes and spellbooks. What is bought goes to whoever in the party it would
 * be better for -- asked, when that is more than one of them -- and so does
 * what a beaten monster drops (gear_drop(), for the open world's spoils:
 * src/lib/ow_tmap.c). The party screen (I) is here too, and the
 * help screen (help.c).
 */

#include <conio.h>
#include <c64.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/engine.h>
#include <q4k/game.h>
#include <q4k/help.h>
#include <q4k/isle.h>
#include <q4k/isles.h>
#include <q4k/shop.h>
#include <q4k/sprite.h>
#include <q4k/text.h>
#include <q4k/view.h>

#define TITLE_Y   0
#define LIST_Y    3
#define PURSE_Y  14
#define NOTE_Y   16     /* two rows: what happened, or who takes it */
#define HINT_Y   20

#define K_STOP    0x03
#define K_T       0x54
#define NONE      0xFF

static char line[SCR_W + 1];

unsigned int potion_price(void)
{
    return 5u + 5u * party_level;
}

static void note(const char *literal, unsigned char color)
{
    text_clear(0, NOTE_Y, SCR_W);
    text_clear(0, NOTE_Y + 1, SCR_W);
    str_lit(line, literal);
    text_put(0, NOTE_Y, line, color);
}

/* The prices, by tier as arms.c names them; nothing sells Dragonbane, the
   staff or the hunting bow. Here rather than with the names, which every
   engine reads, because only the shop needs them. */
static const unsigned int weapon_cost[NUM_WEAPONS] = {
    0, 25, 80, 200, 420, 700, 1000, 0, 0,
};
static const unsigned int armour_cost[NUM_ARMOUR] = {
    0, 40, 30, 110, 260, 500, 800, 1100,
};
static const unsigned int bow_cost[NUM_BOWS + 1] = {
    0, 0, 80, 150, 350, 650, ARROW_PRICE,
};

/* What each land's shops sell, and its monsters drop: home first, then the
   continents in the order the story opens them (slot_of()). Blades and mail
   go by the same tiers (mail one further on, past the robes); bows have
   fewer, and start past the hunting bow. */
static const unsigned char tier_lo[5] = { 1, 1, 2, 3, 4 };
static const unsigned char tier_hi[5] = { 2, 3, 4, 5, 6 };
static const unsigned char bow_lo[5]  = { 2, 2, 3, 3, 4 };
static const unsigned char bow_hi[5]  = { 2, 3, 4, 4, 5 };

/* The magic shops: robes, then books. Every wizard knows the Spark, so
   there is no book of it. Home's town sells the spells up to the Sleep
   (Heal, Ward, Sleep), Ashcombe's (the only other) the Ward to the Blaze:
   `book0` is the first book a shop sells, `books` how many. A book wants
   the party to have reached a level before its wizard can follow it. */
#define BOOKS 5
static const unsigned int  book_cost[BOOKS]  = { 80, 120, 200, 400, 700 };
static const unsigned char book_level[BOOKS] = {  1,   2,   3,   5,   7 };
#define ROBES_COST 40

/* What the shop in front of you sells: GEAR_* of tiers `lo` to `hi` (the
   bowyer lists the arrows after the bows), the magic shop's list, or the
   apothecary's draughts (gear NONE). `rows` is how many lines the list
   has. */
static const char *const *names;
static const unsigned int *costs;
static unsigned char       gear, lo, hi, rows, bow, magic, book0;

/* The tier the place is of: the land's, or for an island's town or sea
   cave the one for the party's level when they first went in (q4k/isles.h),
   or (cave_hoard()) one set beforehand. */
static unsigned char tier_set = NONE;

static unsigned char tier_here(void)
{
    unsigned char lv = isl_level();

    return tier_set != NONE ? tier_set
         : lv              ? ISL_TIER(lv)
         : land == ISLE_HOME ? 0 : 1 + slot_of(land);
}

static void stock(unsigned char kind)
{
    unsigned char t = tier_here();

    bow   = kind == SHOP_BOW;
    magic = kind == SHOP_MAGIC;
    lo    = tier_lo[t];
    hi    = tier_hi[t];
    gear  = NONE;
    rows  = 1;
    if (kind == SHOP_WEAPON) {
        names = weapon_names; costs = weapon_cost;
        gear  = GEAR_WEAPON;
    } else if (kind == SHOP_ARMOUR) {
        names = armour_names; costs = armour_cost;
        gear  = GEAR_ARMOUR;
        ++lo;
        ++hi;
    } else if (bow) {
        names = bow_names;    costs = bow_cost;
        gear  = GEAR_BOW;
        lo = bow_lo[t];
        hi = bow_hi[t];
    }
    if (gear != NONE) {
        rows = hi - lo + 1 + bow;
    } else if (magic) {
        book0 = land != ISLE_HOME;
        rows  = 4 + book0;          /* the robes and 3 books, or 4 */
        lo    = 1;                  /* item(): a line is its row, on any
                                       land's tier */
    }
}

/* The bowyer's arrows: a bundle of ARROW_PACK, or as many as the quiver
   has room for (ARROW_MAX) if that is fewer, at the bundle's price for
   each arrow, rounded to the nearest mark. 0 when it is full. */
static unsigned char arrows_room(void)
{
    return arrows >= ARROW_MAX ? 0
         : ARROW_MAX - arrows < ARROW_PACK ? ARROW_MAX - arrows : ARROW_PACK;
}

static unsigned int arrows_cost(unsigned char n)
{
    if (n == 0 || n == ARROW_PACK) {
        return ARROW_PRICE;
    }
    return (ARROW_PRICE * n + ARROW_PACK / 2) / ARROW_PACK;
}

/* What line `row` (from 1) of the list is. */
static unsigned char item(unsigned char row)
{
    return bow && row == rows ? NUM_BOWS : lo + row - 1;
}

/* --- who takes it -------------------------------------------------------
 *
 * A thing is offered to the party: whoever it would be of use to. With one
 * such, they take it; with more, the player is asked, on the shop's note
 * rows or, for what a monster left, the spoils' (gear_drop()). */

static unsigned char ask_y = NOTE_Y;    /* the two rows asking who takes it */
static unsigned char can[MAX_PARTY];
static unsigned char taker;       /* who the last offer() went to, or NONE */
static unsigned char handed;      /* who took what they had instead, or NONE */
static unsigned char handed_idx;  /* and what that was, in names[] */
static const char   *item_text;   /* what that was, for the note */
/* "Book of " and the longest spell, Sleep: 14 with the NUL. Only a byte
   of BSS are free in the online build (CLAUDE.md): keep this small. */
static char          book_text[14];

static void ask_line(unsigned char row, const char *petscii)
{
    cclearxy(0, ask_y + row, SCR_W);
    textcolor(COLOR_YELLOW);
    cputsxy(0, ask_y + row, petscii);
}

/* Of those can[] marks, which takes `what`: their index, or NONE if there
   is nobody, or if the player thought better of it (RUN/STOP). */
static unsigned char choose(const char *what)
{
    unsigned char i, n = 0, one = NONE, key;
    char num[2];

    for (i = 0; i < party_count; ++i) {
        if (can[i]) {
            ++n;
            one = i;
        }
    }
    if (n < 2) {
        return one;
    }

    strcpy(eng_text, "Who takes the ");
    strcat(eng_text, what);
    strcat(eng_text, "?");
    ask_line(0, eng_text);
    eng_text[0] = '\0';
    num[1] = '\0';
    for (i = 0; i < party_count; ++i) {
        if (can[i]) {
            num[0] = 0x31 + i;
            strcat(eng_text, num);
            strcat(eng_text, " ");
            strcat(eng_text, party[i].name);
            strcat(eng_text, "  ");
        }
    }
    ask_line(1, eng_text);
    for (;;) {
        key = cgetc();
        if (key == K_STOP) {
            return NONE;
        }
        i = key - 0x31;
        if (i < party_count && can[i]) {
            return i;
        }
    }
}

/* Offer tier `idx` of `gear` to whoever it is better for. Returns who took
   it, or NONE. A first bow in the party comes strung, with arrows. What the
   taker had goes on to whoever that is better for (handed), or is sold
   back with the rest. */
static unsigned char offer(unsigned char idx)
{
    unsigned char i, had = party_has_bow();

    for (i = 0; i < party_count; ++i) {
        can[i] = gear_better(i, gear, idx);
    }
    i = choose(names[idx]);
    taker = i;
    handed = NONE;
    if (i != NONE) {
        handed_idx = *gear_slot(i, gear);       /* what they had */
        *gear_slot(i, gear) = idx;
        if (gear == GEAR_BOW && !had) {
            arrows += ARROW_FREE;
        }
        for (i = 0; i < party_count; ++i) {
            can[i] = gear_better(i, gear, handed_idx);
        }
        handed = choose(names[handed_idx]);
        if (handed != NONE) {
            *gear_slot(handed, gear) = handed_idx;
        }
    }
    return taker;
}

/* "Mira takes the old Short sword.", in eng_text, when offer() handed
   something down (names[] is still what was offered, robes too). */
static const char *handed_line(void)
{
    strcpy(eng_text, party[handed].name);
    strcat(eng_text, " takes the old ");
    strcat(eng_text, names[handed_idx]);
    strcat(eng_text, ".");
    return eng_text;
}

/* The note for a sale: who took it, even when there was no one to ask. */
static void fair_trade(void)
{
    if (taker == NONE) {
        note("A fair trade.", COLOR_LIGHTGREEN);
        return;
    }
    text_clear(0, NOTE_Y, SCR_W);
    text_clear(0, NOTE_Y + 1, SCR_W);
    eng_text[0] = '\0';
    strcat(eng_text, party[taker].name);
    strcat(eng_text, " takes the ");
    strcat(eng_text, item_text);
    strcat(eng_text, ".");
    textcolor(COLOR_LIGHTGREEN);
    cputsxy(0, NOTE_Y, eng_text);
    if (handed != NONE) {
        cputsxy(0, NOTE_Y + 1, handed_line());
        return;
    }
    str_lit(line, "A fair trade.");
    text_put(0, NOTE_Y + 1, line, COLOR_LIGHTGREEN);
}

/* One beaten monster in this many that carries anything leaves a blade, mail
   or a bow of what this land's shops sell (the roll is ow_fight.c's). Not
   constant, so that a test can pin it with --poke gear_odds=1. */
unsigned char gear_odds = 12;

/* What a beaten monster left (shop.h): `r` a roll, for which of the three
   and which tier. */
static unsigned char __fastcall__ hand_out(unsigned char pick,
                                          unsigned char y);

unsigned char __fastcall__ gear_drop(unsigned char r, unsigned char y)
{
    unsigned char k = r % 3;

    stock(k == 2 ? SHOP_BOW : k);
    return hand_out(lo + (unsigned char)((r >> 2) % (hi - lo + 1)), y);
}

/* Line `pick` of what stock() chose, offered from row y, and said: who took
   it up, or that it was left. Returns the row after. */
static unsigned char __fastcall__ hand_out(unsigned char pick,
                                          unsigned char y)
{
    unsigned char who, i;

    ask_y = y;
    who = offer(pick);
    ask_y = NOTE_Y;
    cclearxy(0, y, SCR_W);
    cclearxy(0, y + 1, SCR_W);
    gotoxy(1, y);
    textcolor(COLOR_LIGHTGREEN);
    if (who != NONE) {
        cputs(party[who].name);
        cputs(" takes up its ");
        cputs(names[pick]);
        cputs("!");
        if (handed != NONE) {
            cputsxy(1, y + 1, handed_line());
        }
    } else {
        for (i = 0; i < party_count && !can[i]; ++i) {
        }
        textcolor(COLOR_GRAY3);
        cputs(i < party_count ? "You leave its " : "Its ");
        cputs(names[pick]);
        cputs(i < party_count ? "." : " is no use to you.");
    }
    return y + 3;
}

/* --- the shop ------------------------------------------------------------ */

/* Is tier `k` of what is sold in anyone's hands? */
static unsigned char owned(unsigned char k)
{
    unsigned char i;

    for (i = 0; i < party_count; ++i) {
        if (*gear_slot(i, gear) == k) {
            return 1;
        }
    }
    return 0;
}

/* Does any wizard know spell `s`? */
static unsigned char known(unsigned char s)
{
    unsigned char i;

    for (i = 0; i < party_count; ++i) {
        if (party[i].known & (1 << s)) {
            return 1;
        }
    }
    return 0;
}

static void row_out(unsigned char i, unsigned char color)
{
    text_clear(0, LIST_Y + i - 1, SCR_W);
    text_put(0, LIST_Y + i - 1, line, color);
}

static char *row_start(unsigned char i)
{
    char *p = str_lit(line, "  ");

    p = str_num(p, i);
    return str_lit(p, "  ");
}

static char *row_price(char *p, unsigned int cost)
{
    p = str_pad(p, 26, line);
    p = str_num(p, cost);
    return str_lit(p, " marks");
}

static void draw(void)
{
    unsigned char i, k, c;
    char *p;

    if (gear != NONE) {
        for (i = 1; i <= rows; ++i) {
            k = item(i);
            p = row_start(i);
            if (bow && k == NUM_BOWS && arrows_room() &&
                arrows_room() < ARROW_PACK) {
                p = str_num(p, arrows_room());  /* (a quiver nearly full) */
                p = str_lit(p, " arrows");
                row_price(p, arrows_cost(arrows_room()));
            } else {
                p = str_lit(p, names[k]);
                row_price(p, costs[k]);
            }
            c = COLOR_WHITE;
            if (!bow || k != NUM_BOWS) {    /* (the arrows are never had) */
                if (owned(k)) {
                    c = COLOR_LIGHTGREEN;
                }
            }
            row_out(i, c);
        }
    } else if (magic) {
        p = row_start(1);
        p = str_lit(p, armour_names[ARM_ROBES]);
        row_price(p, ROBES_COST);
        gear = GEAR_ARMOUR;
        row_out(1, owned(ARM_ROBES) ? COLOR_LIGHTGREEN : COLOR_WHITE);
        gear = NONE;
        for (i = 2; i <= rows; ++i) {
            k = book0 + i - 2;
            p = row_start(i);
            p = str_lit(p, "Book of ");
            p = str_lit(p, spell_names[SP_HEAL + k]);
            row_price(p, book_cost[k]);
            row_out(i, known(SP_HEAL + k) ? COLOR_LIGHTGREEN
                                          : COLOR_WHITE);
        }
    } else {
        p = row_start(1);
        p = str_lit(p, "Healing draught");
        row_price(p, potion_price());
        row_out(1, COLOR_WHITE);
    }

    p = str_lit(line, "You have ");
    p = str_num(p, gold);
    p = str_lit(p, " marks");
    if (gear == NONE && !magic) {
        p = str_lit(p, ", ");
        p = str_num(p, potions);
        p = str_lit(p, " draughts");
    }
    p = str_lit(p, ".");
    text_clear(0, PURSE_Y, SCR_W);
    text_put(0, PURSE_Y, line, COLOR_YELLOW);
}

/* The magic shop's line `pick` (from 1): robes, or a book. Returns 1 if it
   was taken, having said why not otherwise. */
static unsigned char magic_buy(unsigned char pick)
{
    unsigned char i, s, wiz = 0;

    if (pick == 1) {
        gear  = GEAR_ARMOUR;
        names = armour_names;
        i = offer(ARM_ROBES);
        gear  = NONE;
        if (i == NONE) {
            note("Nobody has need of robes.", COLOR_GRAY2);
            return 0;
        }
        item_text = armour_names[ARM_ROBES];
        return 1;
    }
    pick += book0;              /* the book's place in book_cost[] + 2 */
    s = SP_HEAL + pick - 2;
    for (i = 0; i < party_count; ++i) {
        can[i] = 0;
        if (party[i].cls == CL_WIZARD) {
            wiz = 1;
            can[i] = !(party[i].known & (1 << s));
        }
    }
    if (!wiz) {
        note("None of you can read it.", COLOR_GRAY2);
        return 0;
    }
    if (party_level < book_level[pick - 2]) {
        note("It is past you yet. Come back wiser.", COLOR_GRAY2);
        return 0;
    }
    strcpy(book_text, "Book of ");
    strcat(book_text, spell_names[s]);
    i = choose(book_text);
    if (i == NONE) {
        note("You know that spell already.", COLOR_GRAY2);
        return 0;
    }
    taker = i;
    handed = NONE;              /* (offer()'s: a book replaces nothing) */
    item_text = book_text;
    party[i].known |= 1 << s;
    /* Prepared at once, while there is a slot for it: the party screen
       changes which are. */
    if (spells_ready(i) < spell_slots()) {
        party[i].ready |= 1 << s;
    }
    return 1;
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
                      : magic               ? "The Magic Shop"
                                            : "The Apothecary");
    textcolor(COLOR_GRAY1);
    chlinexy(0, TITLE_Y + 1, SCR_W);
    chlinexy(0, HINT_Y - 1, SCR_W);

    str_lit(line, "Number to buy, RUN/STOP leaves");
    text_put(0, HINT_Y, line, COLOR_GRAY2);

    for (;;) {
        draw();

        key = cgetc();
        if (key == K_STOP) {
            return;
        }
        if (key < 0x31 || key > 0x30 + rows) {
            continue;
        }
        pick = item(key - 0x30);

        cost = gear != NONE ? costs[pick]
             : !magic       ? potion_price()
             : pick == 1    ? ROBES_COST : book_cost[book0 + pick - 2];
        if (bow && pick == NUM_BOWS) {  /* (not a smith's or armourer's
                                           sixth tier, which is 6 too) */
            cost = arrows_cost(arrows_room());
        }

        if (gold < cost) {
            note("You cannot afford that.", COLOR_LIGHTRED);
            continue;
        }
        if (bow && pick == NUM_BOWS) {
            if (!party_has_bow() || !arrows_room()) {
                note(party_has_bow() ? "Your quiver is full."
                                     : "None of you has a bow.",
                     COLOR_GRAY2);
            } else {
                gold -= cost;
                arrows += arrows_room();
                note("Straight ones, mind.", COLOR_LIGHTGREEN);
            }
            continue;
        }
        taker = NONE;
        if (magic) {
            if (!magic_buy(pick)) {
                continue;
            }
        } else if (gear != NONE) {
            item_text = names[pick];
            if (offer(pick) == NONE) {
                note("None of you would be better for it.", COLOR_GRAY2);
                continue;
            }
        } else {
            ++potions;
        }
        gold -= cost;
        fair_trade();
    }
}

/* --- the party screen (I) ------------------------------------------------ */

/* Each member: name, class, health and magic; their weapon and bow; their
   armour; and a wizard's spells, the prepared ones in white. */
unsigned char member_rows(unsigned char i, unsigned char y)
{
    const Actor *a = &party[i];
    unsigned char x;
    char *p;

    p = str_num(line, i + 1);
    p = str_lit(p, " ");
    str_lit(p, a->name);           /* PETSCII, as the title typed it */
    text_put(0, y, line, COLOR_YELLOW);
    p = str_lit(line, class_names[a->cls]);
    p = str_lit(p, "  HP ");
    p = str_num(p, a->hp);
    p = str_lit(p, "/");
    p = str_num(p, a->hp_max);
    if (a->mp_max) {
        p = str_lit(p, "  MP ");
        p = str_num(p, a->mp);
        p = str_lit(p, "/");
        p = str_num(p, a->mp_max);
    }
    text_put(11, y, line, COLOR_GRAY3);

    p = str_lit(line, weapon_names[a->weapon]);
    if (a->bow) {
        p = str_lit(p, ", ");
        p = str_lit(p, bow_names[a->bow]);
    }
    text_put(2, ++y, line, COLOR_CYAN);
    str_lit(line, armour_names[a->armour]);
    text_put(2, ++y, line, COLOR_CYAN);

    if (a->cls == CL_WIZARD) {
        ++y;
        x = 2;
        for (i = 0; i < NUM_SPELLS; ++i) {
            if (a->known & (1 << i)) {
                str_lit(line, spell_names[i]);
                x += text_put(x, y, line, a->ready & (1 << i)
                              ? COLOR_WHITE : COLOR_GRAY1) + 1;
            }
        }
    }
    return y + 2;
}

void party_head(void)
{
    clrscr();
    textcolor(COLOR_YELLOW);
    cputsxy(0, 0, "The party");
    textcolor(COLOR_GRAY1);
    chlinexy(0, 1, SCR_W);
}

void __fastcall__ party_totals(unsigned char y)
{
    register char *p;           /* (not BSS: the online game has 1 byte) */

    p = str_lit(line, "Level ");
    p = str_num(p, party_level);
    p = str_lit(p, "  Exp ");
    p = str_num(p, party_xp);
    p = str_lit(p, "  Gold ");
    p = str_num(p, gold);
    text_put(0, y, line, COLOR_WHITE);
    p = str_lit(line, "Draughts ");
    p = str_num(p, potions);
    p = str_lit(p, "  Arrows ");
    p = str_num(p, arrows);
    text_put(0, y + 1, line, COLOR_WHITE);
}

void party_draw(void)
{
    unsigned char i, y = 2, maps = 0;
    char *p;

    party_head();
    for (i = 0; i < party_count; ++i) {
        y = member_rows(i, y);
    }
    party_totals(y);

    for (i = 0; i < MAPS; ++i) {
        maps += tmap[TM_LAND + i] != 0;
    }
    p = str_lit(line, "T  treasure maps (");
    p = str_num(p, maps);
    str_lit(p, ")");
    text_put(0, 21, line, COLOR_GRAY2);
    for (i = 0; i < party_count && party_spells(i) < 2; ++i) {
    }
    if (i < party_count) {
        str_lit(line, "1-3  a wizard's prepared spells");
        text_put(0, 22, line, COLOR_GRAY2);
    }
    str_lit(line, "Any other key goes back");
    text_put(0, 24, line, COLOR_GRAY2);
}

/* How many spells member `who` knows: preparing is offered only for more
   than one. */
unsigned char __fastcall__ party_spells(unsigned char who)
{
    unsigned char n = 0;

    /* (The argument is counted down in place: it is on the C stack, and
       every static byte is wanted in the online build.) */
    for (who = party[who].known & SPELL_BITS; who; who >>= 1) {
        n += who & 1;
    }
    return n;
}

/* Change which of wizard `who`'s spells are prepared, a key at a time, over
   the bottom of whatever the caller has drawn: a number toggles one, while
   there is a slot for it, and returns 1 for the caller to redraw and call
   again; RETURN or RUN/STOP is done (0). The last one ready stays ready. */
unsigned char __fastcall__ party_prepare(unsigned char who)
{
    Actor *a = &party[who];
    unsigned char i, key, bit, n;
    char *p;

    key = 0;
    for (;;) {
        for (i = 20; i < 25; ++i) {
            text_clear(0, i, SCR_W);
        }
        /* "Zed: 1 of 2 ready": room for as many as the slots, or as are
           known if fewer. */
        n = spell_slots();
        i = party_spells(who);
        if (i < n) {
            n = i;
        }
        p = str_lit(line, a->name);
        p = str_lit(p, ": ");
        p = str_num(p, spells_ready(who));
        p = str_lit(p, " of ");
        p = str_num(p, n);
        str_lit(p, " ready");
        text_put(0, 20, line, COLOR_YELLOW);
        n = 0;
        for (i = 0; i < NUM_SPELLS; ++i) {
            if (a->known & (1 << i)) {
                p = str_num(line, i + 1);
                p = str_lit(p, " ");
                str_lit(p, spell_names[i]);
                text_put((n % 3) * 13, 21 + n / 3, line,
                         a->ready & (1 << i) ? COLOR_WHITE : COLOR_GRAY1);
                ++n;
            }
        }
        if (key == NONE) {      /* the last one ready, refused */
            str_lit(line, "One must stay ready.");
            text_put(0, 23, line, COLOR_LIGHTRED);
        } else {
            str_lit(line, "Number to change, RETURN when done");
            text_put(0, 23, line, COLOR_GRAY2);
        }

        key = cgetc();
        if (key == 0x0D || key == K_STOP) {
            return 0;
        }
        i = key - 0x31;
        if (i >= NUM_SPELLS) {
            continue;
        }
        bit = 1 << i;
        if (!(a->known & bit)) {
            continue;
        }
        if (a->ready & bit) {
            if (a->ready == bit) {
                key = NONE;
                continue;
            }
            a->ready &= ~bit;
        } else if (spells_ready(who) < spell_slots()) {
            a->ready |= bit;
        }
        return 1;
    }
}

/* Returns 1 if the player asked for the treasure maps. */
static unsigned char party_screen(void)
{
    unsigned char key, i;

    sprite_hide();
    for (;;) {
        party_draw();
        key = cgetc();
        if (key == K_T || key == K_T + 0x80) {
            return 1;
        }
        i = key - 0x31;
        if (i < party_count && party_spells(i) > 1) {
            while (party_prepare(i)) {
                party_draw();
            }
            continue;
        }
        return 0;
    }
}

/* The shop's engine: engine.h. */
unsigned char shop_entry(void)
{
    switch (eng_a) {
        case SHOP_HELP:  help_screen();        break;
        case SHOP_PARTY: return party_screen();
        default:         shop_open(eng_a);     break;
    }
    return 0;
}

/* --- a sea cave's chest -------------------------------------------------- */

/* What the chest at a sea cave's end could hold: a blade, mail or a bow
   (k 0-2, as gear_drop() counts them) of the cave's tier, that nobody
   carries and that would be better for one of them; or (k 3) a book of a
   spell no wizard of theirs knows, that their level lets them follow.
   Counts them, and stops at number `want` (NONE: never), its kind in
   hoard_k and which in hoard_x. */
static unsigned char hoard_k, hoard_x;

static unsigned char __fastcall__ hoard_scan(unsigned char want)
{
    unsigned char n = 0, k, x, i;

    for (k = 0; k < 4; ++k) {
        if (k < 3) {
            stock(k == 2 ? SHOP_BOW : k);
        } else {
            lo = 0;
            hi = BOOKS - 1;
        }
        for (x = lo; x <= hi; ++x) {
            for (i = 0; i < party_count; ++i) {
                if (k < 3 ? !owned(x) && gear_better(i, gear, x)
                          : party_level >= book_level[x]
                            && party[i].cls == CL_WIZARD
                            && !(party[i].known & (1 << (SP_HEAL + x)))) {
                    break;
                }
            }
            if (i < party_count) {
                if (n == want) {
                    hoard_k = k;
                    hoard_x = x;
                    return n;
                }
                ++n;
            }
        }
    }
    return n;
}

void __fastcall__ cave_hoard(unsigned char lv)
{
    unsigned char n, i, s, y = 7;
    unsigned int  g = lv * 50u;
    char *p;

    tier_set = ISL_TIER(lv);
    clrscr();
    textcolor(COLOR_YELLOW);
    cputsxy(1, 2, "In the chest at the cave's end:");
    gold += g;
    p = str_num(str_lit(line, " "), g);
    str_lit(p, " marks!");
    text_put(0, 4, line, COLOR_LIGHTGREEN);
    n = hoard_scan(NONE);
    if (n) {
        hoard_scan((unsigned char)(rand() % n));
        if (hoard_k < 3) {
            stock(hoard_k == 2 ? SHOP_BOW : hoard_k);
            y = hand_out(hoard_x, 5);
        } else {
            s = SP_HEAL + hoard_x;
            for (i = 0; i < party_count; ++i) {
                can[i] = party[i].cls == CL_WIZARD
                         && !(party[i].known & (1 << s));
            }
            strcpy(book_text, "Book of ");
            strcat(book_text, spell_names[s]);
            ask_y = 5;
            i = choose(book_text);
            ask_y = NOTE_Y;
            cclearxy(0, 5, SCR_W);
            cclearxy(0, 6, SCR_W);
            gotoxy(1, 5);
            if (i != NONE) {
                party[i].known |= 1 << s;
                if (spells_ready(i) < spell_slots()) {
                    party[i].ready |= 1 << s;
                }
                textcolor(COLOR_LIGHTGREEN);
                cputs(party[i].name);
                cputs(" takes up its ");
            } else {
                textcolor(COLOR_GRAY3);
                cputs("You leave its ");
            }
            cputs(book_text);
            cputs(i != NONE ? "!" : ".");
            y = 8;
        }
    }
    tier_set = NONE;
    textcolor(COLOR_GRAY3);
    cputsxy(1, y, "The cave groans around you.");
    textcolor(COLOR_GRAY2);
    cputsxy(1, 22, "Press a key.");
    cgetc();
}
