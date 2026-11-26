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
#define ISL_DEEP    2           /* a sea cave's levels: no keeper at the
                                   bottom, and no chests (looted[] has no
                                   bits for them) */

void isles_make(void);          /* the bootstrap's (ow_isles.c) */

#endif
