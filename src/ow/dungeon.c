/*
 * Open world: the dungeon's overlay.
 *
 * The Hollow Crown at home, the Drowned Stair and Blackpit across the sea:
 * built a level at a time by the game's generator (dungeon_build(), whose
 * own copy is linked into this overlay: see the Makefile), its stairs going
 * down and up, and up from the first level out. Loaded into the slot over
 * the world's when the hero steps onto a dungeon (world.c's to_dungeon()),
 * and the way out asks for the world's back (engine.h). Built with
 * --code-name OVDCODE.
 */

#include <q4k/dungeon.h>
#include <q4k/music_map.h>
#include <q4k/ow.h>
#include <q4k/tiles.h>

#include "engine.h"

/* Level `depth` of dungeon dun_index, standing on the stairs you came by:
   up if you came down, down if you came up. */
static void dungeon_level(unsigned char came_down)
{
    dungeon_build(dun_index, depth);
    build_area(ts_dungeon, DUN_W, DUN_H);
    if (came_down) {
        show_area(dun_up_x + BIAS, dun_up_y + BIAS);
    } else {
        show_area(dun_dn_x + BIAS, dun_dn_y + BIAS);
    }
}

/* Just loaded into the slot: the first level of dun_index, on its way up,
   or (resuming) level depth where the hero stood. The screen is off. */
void d_enter(void)
{
    area = AREA_DUNGEON;
    play_for(MUSIC_DUNGEON);
    if (resuming) {             /* back from another program: as left */
        resuming = 0;
        dungeon_build(dun_index, depth);
        build_area(ts_dungeon, DUN_W, DUN_H);
        show_area(resume_x + BIAS, resume_y + BIAS);
        return;
    }
    depth = 1;
    dungeon_level(1);
}

/* At the end of a step: the stairs, down, up, or out to the world's
   overlay from the first level. */
void d_arrived(void)
{
    unsigned char t = tile_at(hx, hy);

    if (t == T_MONSTER) {       /* the keeper, who does not step aside */
        ow_fight_with(OWF_KEEPER, dun_index);
    }
    if (t == T_STAIRDN && depth < dungeon_deep[dun_index]) {
        leave_area();
        ++depth;
        dungeon_level(1);
    } else if (t == T_STAIRUP) {
        leave_area();
        if (depth == 1) {
            ov_want = OV_WORLD;
        } else {
            --depth;
            dungeon_level(0);
        }
    } else {
        wander(t);
    }
}
