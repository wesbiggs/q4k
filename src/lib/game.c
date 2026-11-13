#include <string.h>

#include <q4k/actor.h>
#include <q4k/game.h>
#include <q4k/rle.h>
#include <q4k/town_gen.h>
#include <q4k/tiles.h>
#include <q4k/isle.h>
#include <q4k/music.h>
#include <q4k/vault.h>
#include <q4k/view.h>
#include <q4k/world.h>

unsigned char scene;
unsigned char place;
unsigned char depth;
unsigned char land;
unsigned char ret_x, ret_y;
unsigned char local_w, local_h;     /* local[] itself is in local.s */

unsigned int  game_turn;
unsigned int  gold;
unsigned char potions;
unsigned char arrows;
unsigned char quest_stage;
unsigned char items;
unsigned char lore_seed;
unsigned char looted[LOOTED_BYTES];
unsigned char tmap[TM_SIZE];

/* Nonzero and nothing wanders out at you: --poke no_encounters=1 lets a
   screen test walk the overworld or a dungeon without a fight turning up
   on a different square every run. Nothing in the game sets it. */
unsigned char no_encounters;

unsigned char home_fx[NUM_HOME_PLACES], home_fy[NUM_HOME_PLACES];
unsigned char home_biome;
unsigned char home_port_x[4], home_port_y[4];
unsigned char home_ok;    /* scene.c builds it */

const TownEnt *here_ents;
unsigned char  here_n;
unsigned char  here_ex, here_ey;

/* None deeper than MAX_DEEP (game.h): that is all the looted bits allow. */
const unsigned char dungeon_key[NUM_DUNGEONS]  = { IT_SIGIL, IT_TIDAL, IT_ASHEN };
const unsigned char dungeon_prize[NUM_DUNGEONS] = { IT_EMBER, IT_TEAR, IT_HEART };
const unsigned char dungeon_deep[NUM_DUNGEONS] = { 3, 3, 4 };

const char *const dungeon_names[NUM_DUNGEONS] = {
    "The Hollow Crown",
    "The Drowned Stair",
    "Blackpit",
};

void game_init(void)
{
    party_init();

    scene       = SCENE_TOWN;
    place       = TOWN_MILLFORD;
    depth       = 0;
    land        = ISLE_HOME;
    game_turn   = 0;
    gold        = 9;
    potions     = 1;            /* one a member: boot.c, once they are made */
    arrows      = 0;
    quest_stage = Q_START;
    items       = 0;
    lore_seed   = 0;
    memset(looted, 0, sizeof(looted));
    home_forget();
}

unsigned char map_at(unsigned char x, unsigned char y)
{
    if (scene == SCENE_WORLD) {
        return world_tile(x, y);
    }
    if (x >= local_w || y >= local_h) {
        return T_WALL;
    }
    return local[LOCAL_AT(x, y)];
}

void map_set(unsigned char x, unsigned char y, unsigned char code)
{
    if (scene != SCENE_WORLD && x < local_w && y < local_h) {
        local[LOCAL_AT(x, y)] = code;
        town_drawn = 0;
    }
}

unsigned char map_flag(unsigned char x, unsigned char y)
{
    return tile_flags[map_at(x, y) - TILE_BASE];
}

unsigned char home_built(void)
{
    return home_ok;
}

void home_forget(void)
{
    home_ok = 0;
}

unsigned char place_at(unsigned char x, unsigned char y)
{
    unsigned char i, n = 2;
    const unsigned char *fx = isle_fx, *fy = isle_fy;

    if (land == ISLE_HOME) {
        n  = NUM_HOME_PLACES;
        fx = home_fx;
        fy = home_fy;
    }
    for (i = 0; i < n; ++i) {
        if (fx[i] == x && fy[i] == y) {
            return i;
        }
    }
    return PLACE_NONE;
}

/* A shuffle of 0-3 from the game's seed, which is a byte: not every order
   comes up, but the common ones do and no seed gives a sequence a player
   could tell was not drawn by chance. */
unsigned char slot_of(unsigned char d)
{
    unsigned char p[4], i, j, t, v = lore_seed;

    for (i = 0; i < 4; ++i) {
        p[i] = i;
    }
    for (i = 3; i > 0; --i) {
        v = v * 5 + 3;
        j = (v >> 5) % (i + 1);
        t = p[i]; p[i] = p[j]; p[j] = t;
    }
    return p[d];
}

unsigned char has(unsigned char item)
{
    return (items & item) != 0;
}

void give(unsigned char item)
{
    items |= item;
}
