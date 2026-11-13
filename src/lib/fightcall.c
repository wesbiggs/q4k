/*
 * q4k -- the fights, as the map's engine calls them.
 *
 * The fight is an engine of its own, so the map cannot call battle.c's entry
 * points directly: these have their names and load it instead, through the
 * core (engine_call()), which loads the map back before returning to the
 * caller. What a won fight leaves -- a treasure map, a dropped blade -- is
 * shared out here, after the map is back, because the code for that is the
 * map's and the shop's; it happens in the same order, with the same dice, as
 * it did when battle.c's won() did it itself.
 */

#include <stdlib.h>

#include <q4k/battle.h>
#include <q4k/engine.h>
#include <q4k/loot.h>
#include <q4k/shop.h>

static unsigned char fight(unsigned char what, unsigned char arg)
{
    unsigned char r;

    eng_a = what;
    eng_b = arg;
    fight_spoils = 0;
    r = engine_call(ENG_FIGHT, fight_entry);
    if (fight_spoils & SPOIL_WON) {
        treasure_drop();
        /* gear_drop()'s own roll, made here so that the shop's engine is
           loaded only for a monster that really did drop something. */
        if ((fight_spoils & SPOIL_ARMED) && !(rand() % gear_odds)) {
            eng_a = SHOP_GEAR;
            engine_call(ENG_SHOP, shop_entry);
        }
    }
    return r;
}

unsigned char battle_wander(void)
{
    return fight(FIGHT_WANDER, 0);
}

unsigned char battle_undead(void)
{
    return fight(FIGHT_UNDEAD, 0);
}

unsigned char battle_boss(unsigned char dungeon)
{
    return fight(FIGHT_BOSS, dungeon);
}

unsigned char battle_dragon(void)
{
    return fight(FIGHT_DRAGON, 0);
}
