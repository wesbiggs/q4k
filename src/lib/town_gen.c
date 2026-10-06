#include <string.h>

#include <q4k/actor.h>
#include <q4k/game.h>
#include <q4k/isle.h>
#include <q4k/names_data.h>
#include <q4k/tiles.h>
#include <q4k/town_gen.h>

/* The map and the people are made in tgen.s; this is what is left. */
void           town_tiles(unsigned char biome);
extern TownEnt proc_ents[5];
extern unsigned char proc_n;

static const char *const names[8] = TOWN_NAMES;
static const char *const inns[4] = INN_NAMES;

void town_gen(void)
{
    town_tiles(land == ISLE_HOME ? home_biome : isle_biome);

    here_ex   = LOCAL_W / 2;
    here_ey   = LOCAL_H - 1;
    local_w   = LOCAL_W;
    local_h   = LOCAL_H;
    here_ents = proc_ents;
    here_n    = proc_n;
    hero.x    = here_ex;
    hero.y    = here_ey - 1;
}

const char *town_gen_name(void)
{
    return names[(lore_seed + TOWN_STRIDE * land) & (TOWN_COUNT - 1)];
}

const char *town_gen_inn(void)
{
    return inns[(lore_seed + INN_STRIDE * land) & (INN_COUNT - 1)];
}
