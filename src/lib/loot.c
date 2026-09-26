#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/dungeon.h>
#include <q4k/game.h>
#include <q4k/loot.h>
#include <q4k/view.h>

#define NO_LOOT 0xFF

static char msg[24];

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

    map = (scene == SCENE_WORLD) ? LOOT_WORLD : place;
    found = loot_table;
    for (i = 0; i < NUM_LOOT; ++i, ++found) {
        if (found->map == map && found->x == hero.x && found->y == hero.y) {
            return DUN_LOOT + i;
        }
    }
    return NO_LOOT;
}

void look(void)
{
    unsigned char bit, mask;
    unsigned int amount;

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

    /* Gold is the only treasure there is, until there is an inventory to
       put anything else in. A dungeon chest is worth 10 per quest stage
       plus a d20, rolled now rather than when the level is built, which
       would spend the level's own seeded draws. */
    if (scene == SCENE_DUNGEON) {
        amount = ((unsigned int)quest_stage << 3) + (quest_stage << 1)
               + 1 + (unsigned char)(rand() % 20);
    } else {
        amount = found->amount;
    }
    gold += amount;

    strcpy(msg, "You find ");
    utoa(amount, msg + strlen(msg), 10);
    strcat(msg, " gold.");
    view_message(msg);
    ++game_turn;
}
