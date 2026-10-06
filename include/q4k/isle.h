/*
 * q4k -- the continents across the sea.
 *
 * Four ports on the overworld, one to each compass point, each send a ship
 * to a continent of its own. A continent is not stored anywhere: it is
 * built into the shared local[] map when the ship arrives, from the game's
 * lore_seed and the compass point, so it is the same land every time you
 * come back to it -- and different in the next game. Each has a dominant
 * terrain (plains, forest, swamp, desert, or mountains with the grass
 * between), a little of the others, and ranges of mountain and lakes.
 *
 * Every walkable square can be walked to from the landing. The land is
 * grown as one connected blob, and a mountain or a lake is only ever put on
 * a square whose removal leaves its neighbours joined up around it (see
 * removable() in isle_gen.s), so no obstacle can cut a square off.
 */

#ifndef Q4K_ISLE_H
#define Q4K_ISLE_H

#include <q4k/game.h>

#define ISLE_W  64              /* all of local[] but its last row: 64 by 31, */
#define ISLE_H  31              /* which the overworld's row buffer is */

/* Compass points are numbered as the ports are indexed: 0 north, 1 east, 2
   south, 3 west. Going the other way is d ^ 2. The home continent is built by
   the same code, as ISLE_HOME. */
#define ISLE_HOME 4

/* The five kinds a continent can be mostly made of (isle_gen.s knows the
   numbers too). */
#define BIOME_PLAINS    0
#define BIOME_FOREST    1
#define BIOME_SWAMP     2
#define BIOME_DESERT    3
#define BIOME_MOUNTAINS 4
#define BIOMES          5

/* Build the continent that lies off the port at compass point `dir` into
   local[] (and set local_w and local_h), and set the values below. It has
   its own random numbers, so the game's are untouched. */
void isle_build(unsigned char dir);

/* The same in pieces, so something can be drawn while it goes: begin, then
   work until it returns 0. Each piece takes a tenth of a second or so. The
   map is not fit to read until it has. */
void          isle_begin(unsigned char dir);
unsigned char isle_work(void);

/* Where a ship sailing in heading `dir` comes in at the map's edge, and the
   landing it makes: the first land along that row or column, a port. */
extern unsigned char isle_sx, isle_sy;
extern unsigned char isle_dock_x, isle_dock_y;

extern unsigned char isle_biome;        /* BIOME_*: what it is mostly made of */

/* Home's four ports, by the compass point each sends a ship to; a continent's
   landing is port 0 (the same bytes as isle_dock_x and _y). */
extern unsigned char isle_port_x[4], isle_port_y[4];

/* The places on it, in the order they were put down: home's are the first
   town (where the story starts), the castle, the first dungeon, Maldrath's
   lair and one more town; a continent has its story place and one town. The
   story place is a town if isle_story is 0 and a dungeon if it is 1, which
   the caller sets before building. */
extern unsigned char isle_story;
extern unsigned char isle_fx[5], isle_fy[5];
const char *isle_name(void);

#endif /* Q4K_ISLE_H */
