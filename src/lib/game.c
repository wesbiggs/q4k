#include <string.h>

#include <q4k/actor.h>
#include <q4k/game.h>
#include <q4k/rle.h>
#include <q4k/town_gen.h>
#include <q4k/tiles.h>
#include <q4k/isle.h>
#include <q4k/vault.h>
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
unsigned char weapon_lv;
unsigned char armour_lv;
unsigned char bow_lv;
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
static unsigned char home_ok;

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
    potions     = 1;
    weapon_lv   = 0;
    armour_lv   = 0;
    bow_lv      = 0;
    quest_stage = Q_START;
    items       = 0;
    lore_seed   = 0;
    memset(looted, 0, sizeof(looted));
    home_forget();

    town_build(TOWN_MILLFORD);
    hero.x = town_sx[TOWN_MILLFORD];
    hero.y = town_sy[TOWN_MILLFORD];
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
    }
}

unsigned char map_flag(unsigned char x, unsigned char y)
{
    return tile_flags[map_at(x, y) - TILE_BASE];
}

/* Home is made when the story first needs it, and kept: where isle.h says. */
void home_ensure(void)
{
    if (home_ok) {
        return;
    }
    isle_build(ISLE_HOME);
    memcpy(home_fx, isle_fx, NUM_HOME_PLACES);
    memcpy(home_fy, isle_fy, NUM_HOME_PLACES);
    memcpy(home_port_x, isle_port_x, 4);
    memcpy(home_port_y, isle_port_y, 4);
    home_biome = isle_biome;
    home_ok = 1;
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

/* The packed towns are LOCAL_W to a row. Unpack into the tail of the buffer
   and spread the rows out to LOCAL_STRIDE: every row moves down in memory,
   and never past a row still waiting to be moved. */
void town_unpack(unsigned char index)
{
    unsigned char *src = local + sizeof(local) - LOCAL_W * LOCAL_H;
    unsigned char y;

    rle_unpack(src, town_map[index], LOCAL_W * LOCAL_H);
    for (y = 0; y < LOCAL_H; ++y, src += LOCAL_W) {
        memmove(local + LOCAL_AT(0, y), src, LOCAL_W);
    }
    local_w = LOCAL_W;
    local_h = LOCAL_H;
}

const char *town_name(void)
{
    return place < NUM_TOWNS ? town_names[place] : town_gen_name();
}

void town_build(unsigned char index)
{
    scene = SCENE_TOWN;
    place = index;
    depth = 0;
    if (index < NUM_TOWNS) {
        town_unpack(index);
        here_ents = town_ents + town_ent_first[index];
        here_n    = town_ent_first[index + 1] - town_ent_first[index];
        here_ex   = town_ex[index];
        here_ey   = town_ey[index];
        hero.x    = town_sx[index];
        hero.y    = town_sy[index];
    } else {
        town_gen();             /* sets hero, ents and the way out, too */
    }
}

/* Where you went in from: the overworld's square, kept to come out at. The
   continent you are on is put away while the town or dungeon is in local[]. */
void enter_local(void)
{
    ret_x = hero.x;
    ret_y = hero.y;
    if (scene == SCENE_ISLE) {
        vault_park(0);
    }
}

void enter_town(unsigned char index)
{
    enter_local();
    town_build(index);
}

/* Step back out onto the overworld, where you went in. */
void leave_local(void)
{
    if (land == ISLE_HOME) {
        if (!home_ok) {
            /* The first time out of Millford. */
            home_ensure();
            ret_x = home_fx[0];
            ret_y = home_fy[0];
        }
        scene   = SCENE_WORLD;
        local_w = local_h = WORLD_SIZE;
    } else {
        vault_unpark(0);
        scene   = SCENE_ISLE;
        local_w = ISLE_W;
        local_h = ISLE_H;
    }
    hero.x = ret_x;
    hero.y = ret_y;
    depth  = 0;
}

void respawn(void)
{
    land = ISLE_HOME;
    home_ensure();
    ret_x = home_fx[0];
    ret_y = home_fy[0];
    town_build(TOWN_MILLFORD);
}

unsigned char has(unsigned char item)
{
    return (items & item) != 0;
}

void give(unsigned char item)
{
    items |= item;
}
