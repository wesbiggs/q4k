/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Quest for the King -- the small islands of the open sea.

   Every cell of the world with no land of its own (the open sea between
   Keldmarch and the four continents: src/ow/world.c's grid) has three to
   six small islands, each of 1 to 15 squares, and on one of them a town
   or a dungeon. The bootstrap makes them from lore_seed
   (src/lib/ow_isles.c, isles_make()) into a table under the KERNAL, with
   the code that draws them (src/lib/isl_draw.s), and the engine's world
   streams them in with the rest of the sea (load_run()). Nothing of them is
   saved: the same seed makes the same islands.

   A cell is 8 by 8 blocks of 8 by 8 squares, and an island stands in one
   block of the 6 by 6 within its edge: its squares in the 4 by 4 at
   (2, 2) of the block, shallow water round them, the rest of the block
   deep sea, so that nothing of one island reaches another's. */

#ifndef Q4K_ISLES_H
#define Q4K_ISLES_H

#define ISL_CELLS   24          /* 6 by 4, row by row (world.c's grid) */
#define ISL_MAX     6
#define ISL_ENTRY   (4 + 7 * ISL_MAX)   /* a cell's: count, the place's
                                   tile and square (k: 0-3, the top row of
                                   island 0), then 7 bytes an island: its
                                   block (by * 8 + bx), the 16 squares of
                                   its 4 by 4 (bit k = y * 4 + x), its
                                   ground, a second ground and which squares
                                   have that */
#define ISL_AREA    0xF000      /* under the KERNAL, past the music's */
#define ISL_TABLE   ISL_AREA
#define ISL_PARAMS  (ISL_TABLE + ISL_CELLS * ISL_ENTRY)    /* $F450 */
#define ISL_CODE    0xF460      /* isl_draw */
#define ISL_END     0xF600      /* the land's fog (owgame.h's OWG_WFOG) */

/* isl_draw's arguments, at ISL_PARAMS: a run of the sea in cell `cell`,
   from its square (lx, ly), n long, across or down (down 1), drawn over
   what is at dst (WORLD: the deep sea there already). Called with the
   machine in all-RAM (vault_enter()), interrupts off. */
#define ISL_P_CELL  (ISL_PARAMS + 0)
#define ISL_P_LX    (ISL_PARAMS + 1)
#define ISL_P_LY    (ISL_PARAMS + 2)
#define ISL_P_N     (ISL_PARAMS + 3)
#define ISL_P_DOWN  (ISL_PARAMS + 4)
#define ISL_P_DST   (ISL_PARAMS + 5)

/* A town of an island is TOWN_PROC + 1 + its cell, a dungeon NUM_DUNGEONS
   + its cell: generated as a continent's are, as if a land of their own,
   ISL_LAND + the cell (town.c, ow_isles.c). */
#define ISL_LAND    5
#define ISL_END_BIT 9           /* the looted[] bit of the chest at the end
                                   (src/ow/islmark.s's isl_base) */
#define ISL_DEEP    3           /* a sea cave's levels: no keeper at the
                                   bottom, one or two chests on each, and
                                   the one at the end of the last; gone
                                   once the party climbs out (ISL_GONE) */

/* What the party has made of each cell's place: a byte a cell (ISL_CELLS),
   under the KERNAL past the land's fog (owgame.h's OWG_WFOG), written
   straight through and read with the vault banked in (vault_peek(), or
   the engine's sm_vcopy()); in the save after the fog's bits, and cleared
   by the bootstrap with them. ISL_LEVEL: the party's level the first time
   they went in (0: not yet), which the place is made for -- an island
   town's shops, a sea cave's monsters and its chest -- rather than for the
   land the party last left. ISL_GONE: the party has climbed out of the
   sea cave and its mouth fell in behind them: it cannot be gone into
   again, and is drawn as the island's ground. */
#define ISL_STATE   0xFBE0      /* to $FBF7: free to $FBFF, OWG_FOG is next */
#define ISL_LEVEL   0x0F
#define ISL_GONE    0x80

/* A place made for level `lv` (1 on) sells and drops what this tier of
   lands does: shop.c's tiers, 0 home's, 1-4 the continents' in the order
   the story opens them, which the keepers are fought at about level 4, 6,
   8 and 10 for (bestiary.c). */
#define ISL_TIER(lv) ((lv) > 8 ? 4 : ((lv) - 1) >> 1)

/* The level the place the party is in was made for (ISL_LEVEL), if it is
   an island's, its first visit marked now if it was not before; or 0
   anywhere else (src/lib/ow_islv.c, for owtown: the shops, the fights). */
unsigned char isl_level(void);

void isles_make(void);          /* the bootstrap's (ow_isles.c) */

#endif
