#include <string.h>

#include <q4k/actor.h>
#include <q4k/game.h>
#include <q4k/rle.h>
#include <q4k/tiles.h>
#include <q4k/world.h>

unsigned char scene;
unsigned char place;
unsigned char depth;
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

/* The three deep places, in the order the quest opens them. */
const unsigned char dungeon_wx[NUM_DUNGEONS]   = { 36, 22, 11 };
const unsigned char dungeon_wy[NUM_DUNGEONS]   = { 45, 16, 35 };
const unsigned char dungeon_key[NUM_DUNGEONS]  = { IT_SIGIL, IT_TIDAL, IT_ASHEN };
const unsigned char dungeon_prize[NUM_DUNGEONS] = { IT_EMBER, IT_TEAR, IT_HEART };
/* None deeper than MAX_DEEP (game.h): that is all the looted bits allow. */
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
    game_turn   = 0;
    gold        = 60;
    potions     = 1;
    weapon_lv   = 0;
    armour_lv   = 0;
    bow_lv      = 0;
    quest_stage = Q_START;
    items       = 0;
    lore_seed   = 0;
    memset(looted, 0, sizeof(looted));

    town_unpack(TOWN_MILLFORD);
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

signed char town_at(unsigned char x, unsigned char y)
{
    unsigned char i;

    for (i = 0; i < NUM_TOWNS; ++i) {
        if (town_wx[i] == x && town_wy[i] == y) {
            return (signed char)i;
        }
    }
    return -1;
}

signed char dungeon_at(unsigned char x, unsigned char y)
{
    unsigned char i;

    for (i = 0; i < NUM_DUNGEONS; ++i) {
        if (dungeon_wx[i] == x && dungeon_wy[i] == y) {
            return (signed char)i;
        }
    }
    return -1;
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

void enter_town(unsigned char index)
{
    scene = SCENE_TOWN;
    place = index;
    depth = 0;
    town_unpack(index);
    hero.x = town_sx[index];
    hero.y = town_sy[index];
}

/* Step back out onto the overworld, at the place's own map square. */
void leave_local(void)
{
    if (scene == SCENE_TOWN) {
        hero.x = town_wx[place];
        hero.y = town_wy[place];
    } else {
        hero.x = dungeon_wx[place];
        hero.y = dungeon_wy[place];
    }
    scene = SCENE_WORLD;
    depth = 0;
}

unsigned char has(unsigned char item)
{
    return (items & item) != 0;
}

void give(unsigned char item)
{
    items |= item;
}
