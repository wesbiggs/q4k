#include <conio.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/dungeon.h>
#include <q4k/game.h>
#include <q4k/isle.h>
#include <q4k/loot.h>
#include <q4k/sprite.h>
#include <q4k/tiles.h>
#include <q4k/town_gen.h>
#include <q4k/view.h>

/* Read-only data goes in the charset's spare glyphs: cfg/c64-q4k.cfg. */
#pragma rodata-name ("CHDATA2")

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

void look(void)
{
    unsigned char bit, mask, kind;
    unsigned int amount;

    if ((scene == SCENE_WORLD || scene == SCENE_ISLE) && dig()) {
        return;
    }
    bit = loot_bit();
    if (bit == NO_LOOT) {
        view_message("You see nothing.");
        return;
    }

    mask = 1 << (bit & 7);
    bit >>= 3;
    if (looted[bit] & mask) {
        view_message("You've already taken everything.");
        return;
    }
    looted[bit] |= mask;

    /* A dungeon chest is even odds a draught or gold, the gold worth 10 per
       quest stage plus a d20. Rolled now rather than when the level is
       built, which would spend the level's own seeded draws. */
    if (scene == SCENE_DUNGEON) {
        if (rand() & 1) {
            kind   = LOOT_DRAUGHT;
            amount = 1;
        } else {
            kind   = LOOT_GOLD;
            amount = ((unsigned int)quest_stage << 3) + (quest_stage << 1)
                   + 1 + (unsigned char)(rand() % 20);
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
            return;
        }
    } else {
        gold += amount;
    }

    strcpy(msg, "You find ");
    utoa(amount, msg + strlen(msg), 10);
    strcat(msg, kind == LOOT_DRAUGHT ? " draughts." : " gold.");
    view_message(msg);
}

#pragma rodata-name ("HIDATA")

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
            /* Worth more as the story goes on; the last bit from where it
               lay, so that no two are the same and a test can know. */
            amount = ((unsigned int)(quest_stage + 2) << 5)
                   + ((hero.x ^ hero.y) & 31);
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

/* The town a map measures from, in the land `l` that it is on. */
static const char *anchor_name(unsigned char l)
{
    const char *name;
    unsigned char was = land;

    if (l == ISLE_HOME) {
        return town_names[TOWN_MILLFORD];
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
            tmap[TM_RX + i]   = land == ISLE_HOME ? home_fx[0] : isle_fx[1];
            tmap[TM_RY + i]   = land == ISLE_HOME ? home_fy[0] : isle_fy[1];
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
        strcat(msg, anchor_name(l));
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
