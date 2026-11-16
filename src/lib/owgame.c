/*
 * The open world's game block (q4k/owgame.h), into the game's own variables
 * and back: for a program of the open world that links actor.c and game.c
 * (the party's making, the talk, the shops ...).
 */

#include <string.h>

#include <q4k/owgame.h>

void owg_in(void)
{
    memcpy(party, G->party, sizeof party);
    party_count = G->party_count;
    party_level = G->party_level;
    party_xp    = G->party_xp;
    gold        = G->gold;
    game_turn   = G->game_turn;
    potions     = G->potions;
    arrows      = G->arrows;
    quest_stage = G->quest_stage;
    items       = G->items;
    lore_seed   = G->seed;
    memcpy(looted, G->looted, sizeof looted);
}

void owg_out(void)
{
    memcpy(G->party, party, sizeof party);
    G->party_count = party_count;
    G->party_level = party_level;
    G->party_xp    = party_xp;
    G->gold        = gold;
    G->game_turn   = game_turn;
    G->potions     = potions;
    G->arrows      = arrows;
    G->quest_stage = quest_stage;
    G->items       = items;
    G->seed        = lore_seed;
    memcpy(G->looted, looted, sizeof looted);
}
