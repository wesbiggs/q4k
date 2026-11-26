/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Quest for the King -- the level an island's place was made for
 * (q4k/isles.h's ISL_STATE), for owtown: the island towns' shops, and the
 * fights and the spoils in a sea cave. The engine marks a place's first
 * visit as the party goes in (src/ow/engine.c's isl_mark()); this marks it
 * too, for a game saved inside one before there were marks.
 */

#include <q4k/game.h>
#include <q4k/isles.h>
#include <q4k/owgame.h>
#include <q4k/vault.h>

unsigned char isl_level(void)
{
    unsigned char c, b;

    if (G->area == AREA_DUNGEON && G->place >= NUM_DUNGEONS) {
        c = G->place - NUM_DUNGEONS;
    } else if (G->area == AREA_TOWN && G->place > TOWN_PROC) {
        c = G->place - (TOWN_PROC + 1);
    } else {
        return 0;
    }
    b = vault_peek(ISL_STATE + c);
    if (!(b & ISL_LEVEL)) {
        /* (a write under the KERNAL reaches the RAM) */
        *(unsigned char *)(ISL_STATE + c) = b |= G->party_level;
    }
    return b & ISL_LEVEL;
}
