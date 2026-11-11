#include <conio.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/dungeon.h>
#include <q4k/game.h>
#include <q4k/isle.h>
#include <q4k/loot.h>
#include <q4k/place_near.h>
#include <q4k/sprite.h>
#include <q4k/tiles.h>
#include <q4k/town_gen.h>
#include <q4k/view.h>

/* Read-only data goes in the charset's spare glyphs: cfg/c64-q4k.cfg. */
#pragma rodata-name (push, "CHDATA2")

#define NO_LOOT 0xFF

/* One fight in this many leaves a map behind. Not constant, so that a test
   can pin it with --poke treasure_odds=1. */
unsigned char treasure_odds = 10;

static char msg[42];

/* The authored treasure loot_bit() found, if that is what it found. */
static const Loot *found;

/* Which looted[] bit the treasure on this square owns, or NO_LOOT.
 *
 * Written to keep cc65's runtime helpers out of the program: they link into
 * MAIN, which has almost no room left, whichever module calls them. Hence
 * byte-sized bit numbers and a pointer walk -- indexing six-byte entries
 * costs a multiply. */
static unsigned char loot_bit(void)
{
    unsigned char i, map;

    if (scene == SCENE_DUNGEON) {
        for (i = 0; i < dun_chests; ++i) {
            if (dun_cx[i] == hero.x && dun_cy[i] == hero.y) {
                return (place * MAX_DEEP + depth - 1) * DUN_CHESTS + i;
            }
        }
        return NO_LOOT;
    }

    /* Nothing is hidden out on the land. */
    if (scene != SCENE_TOWN) {
        return NO_LOOT;
    }

    map = place;
    found = loot_table;
    for (i = 0; i < NUM_LOOT; ++i, ++found) {
        if (found->map == map && found->x == hero.x && found->y == hero.y) {
            return DUN_LOOT + i;
        }
    }
    return NO_LOOT;
}

static unsigned char dig(void);

/* Looking where there is nothing to find stirs the dead one time in this
   many, so combing a whole area for loot has a price. Not constant, so that
   a test can pin it with --poke grave_odds=1 (or 0 for never). */
unsigned char grave_odds = 5;

unsigned char look(void)
{
    unsigned char bit, mask, kind;
    unsigned int amount;

    if ((scene == SCENE_WORLD || scene == SCENE_ISLE) && dig()) {
        return LOOK_NOTHING;
    }
    bit = loot_bit();
    if (bit == NO_LOOT) {
        if (scene == SCENE_TOWN || no_encounters || !grave_odds
            || rand() % grave_odds) {
            view_message("You see nothing.");
            return LOOK_NOTHING;
        }
        view_message("The ground stirs. The dead rise!");
        ++game_turn;
        return battle_undead();
    }

    mask = 1 << (bit & 7);
    bit >>= 3;
    if (looted[bit] & mask) {
        view_message("You've already taken everything.");
        return LOOK_NOTHING;
    }
    looted[bit] |= mask;

    /* A dungeon chest is even odds a draught or gold, the gold worth 5 per
       quest stage and 4 per level below the first, plus a d10: a fight or
       two's worth, no more, or a dungeon pays for the next land's shops
       before you get there. Rolled now rather than when the level is
       built, which would spend the level's own seeded draws. */
    if (scene == SCENE_DUNGEON) {
        if (rand() & 1) {
            kind   = LOOT_DRAUGHT;
            amount = 1;
        } else {
            kind   = LOOT_GOLD;
            amount = (quest_stage << 2) + quest_stage
                   + ((depth - 1) << 2)
                   + 1 + (unsigned char)(rand() % 10);
        }
    } else {
        kind   = found->kind;
        amount = found->amount;
    }
    ++game_turn;

    if (kind == LOOT_DRAUGHT) {
        potions += (unsigned char)amount;
        if (amount == 1) {
            view_message("You find a draught.");
            return LOOK_NOTHING;
        }
    } else {
        gold += amount;
    }

    strcpy(msg, "You find ");
    utoa(amount, msg + strlen(msg), 10);
    strcat(msg, kind == LOOT_DRAUGHT ? " draughts." : " gold.");
    view_message(msg);
    return LOOK_NOTHING;
}

#pragma rodata-name (pop)

/* Something buried here, by a map? Take it and tear the map up. */
static unsigned char dig(void)
{
    unsigned char i;
    unsigned int amount;

    for (i = 0; i < MAPS; ++i) {
        if (tmap[TM_LAND + i] == land + 1 && tmap[TM_X + i] == hero.x
            && tmap[TM_Y + i] == hero.y) {
            tmap[TM_LAND + i] = 0;
            ++game_turn;
            /* Worth more as the story goes on, 16 a stage -- some five
               fights' gold, for the walk -- and the last bit from where it
               lay, so that no two are the same and a test can know. */
            amount = ((unsigned int)(quest_stage + 2) << 4)
                   + ((hero.x ^ hero.y) & 15);
            gold += amount;
            potions += 2;
            strcpy(msg, "You dig up ");
            utoa(amount, msg + strlen(msg), 10);
            strcat(msg, " gold and 2 draughts!");
            view_message(msg);
            return 1;
        }
    }
    return 0;
}

/* --- treasure maps ---------------------------------------------------------- */

static const char *const compass[4] = { "north", "east", "south", "west" };

/* Home's places, in the order the generator places them (place_at()), and
   a continent's story place. Millford and the castle are the first two
   towns; the rest are towns, named by the land. */
static const char *anchor_name(unsigned char l, unsigned char pl)
{
    const char *name;
    unsigned char was = land, slot;

    if (l == ISLE_HOME) {
        if (pl < 2) {
            return town_names[pl];
        }
        if (pl == 2) {
            return dungeon_names[0];
        }
        if (pl == 3) {
            return "Maldrath's lair";
        }
    } else if (pl == 0) {
        slot = slot_of(l);
        return slot & 1 ? dungeon_names[1 + (slot >> 1)]
                        : town_names[TOWN_WARDLOW + (slot >> 1)];
    }
    land = l;                   /* town_gen_name() reads it */
    name = town_gen_name();
    land = was;
    return name;
}

/* " 18 leagues west" and the like, onto the end of msg. */
static void leg(unsigned char from, unsigned char to, unsigned char neg,
                unsigned char pos, unsigned char more)
{
    unsigned char n = to < from ? from - to : to - from;

    if (!n) {
        return;
    }
    if (msg[0]) {
        strcat(msg, ", ");
    }
    utoa(n, msg + strlen(msg), 10);
    if (!more) {
        strcat(msg, n == 1 ? " league" : " leagues");
    }
    strcat(msg, " ");
    strcat(msg, compass[to < from ? neg : pos]);
}

/* Slot `i`'s map measures from the place of this land nearest the buried
   square (place_near.s), by steps: far from Millford is still near
   something. */
static void nearest(unsigned char i)
{
    near_x = tmap[TM_X + i];
    near_y = tmap[TM_Y + i];
    tmap[TM_PL + i] = place_near();
    tmap[TM_RX + i] = near_fx;
    tmap[TM_RY + i] = near_fy;
}

void treasure_drop(void)
{
    unsigned char i, n, x, y;

    if ((scene != SCENE_WORLD && scene != SCENE_ISLE)
        || rand() % treasure_odds) {
        return;
    }
    for (i = 0; i < MAPS && tmap[TM_LAND + i]; ++i) {
    }
    if (i == MAPS) {
        return;
    }
    /* Anywhere you could walk, and not on the hero: every walkable square of
       a land can be reached, and no special square is plain ground. */
    for (n = 0; n < 40; ++n) {
        x = (unsigned char)(rand() % local_w);
        y = (unsigned char)(rand() % local_h);
        if (map_flag(x, y) == TF_PASS && (x != hero.x || y != hero.y)) {
            tmap[TM_LAND + i] = land + 1;
            tmap[TM_X + i]    = x;
            tmap[TM_Y + i]    = y;
            nearest(i);
            view_message("It had a treasure map! (I)");
            return;
        }
    }
}

void treasure_maps(void)
{
    unsigned char i, row = 3, l;

    sprite_hide();
    clrscr();
    textcolor(COLOR_YELLOW);
    cputsxy(1, 0, "Treasure maps");
    textcolor(COLOR_GRAY1);
    chlinexy(0, 1, SCR_W);

    for (i = 0; i < MAPS; ++i) {
        l = tmap[TM_LAND + i];
        if (!l) {
            continue;
        }
        --l;
        textcolor(COLOR_LIGHTBLUE);
        if (l == ISLE_HOME) {
            strcpy(msg, "Keldmarch");
        } else {
            strcpy(msg, "Over the ");
            strcat(msg, compass[l]);
            strcat(msg, " sea");
        }
        cputsxy(1, row, msg);

        msg[0] = '\0';
        leg(tmap[TM_RX + i], tmap[TM_X + i], 3, 1, 0);
        leg(tmap[TM_RY + i], tmap[TM_Y + i], 0, 2, msg[0] != '\0');
        strcat(msg, " of ");
        strcat(msg, anchor_name(l, tmap[TM_PL + i]));
        textcolor(COLOR_WHITE);
        cputsxy(3, row + 1, msg);
        row += 3;
    }
    if (row == 3) {
        textcolor(COLOR_WHITE);
        cputsxy(1, 3, "You carry no treasure maps.");
    }
    textcolor(COLOR_GRAY2);
    cputsxy(1, 24, "Stand on the spot and press L");
    cgetc();
    view_restore();
}
