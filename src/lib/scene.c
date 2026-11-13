/*
 * q4k -- building the scene you are in: a town, home, the way back out.
 *
 * Split out of game.c, which keeps the state and the questions about the map
 * that the fight asks too (map_at() and the rest), so that the offline build
 * can keep this with the map's engine, on the disk (ENGINES.md). Both builds
 * link it; nothing here changed in the move but the two functions at the end,
 * which were the start of game_init() and the end of savegame.c's do_load().
 */

#include <string.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/dungeon.h>
#include <q4k/fov.h>
#include <q4k/game.h>
#include <q4k/rle.h>
#include <q4k/savegame.h>
#include <q4k/town_gen.h>
#include <q4k/tiles.h>
#include <q4k/isle.h>
#include <q4k/music.h>
#include <q4k/vault.h>
#include <q4k/view.h>
#include <q4k/world.h>

/* Home is made when the story first needs it, and kept: where isle.h says. */
void home_ensure(void)
{
    if (home_ok) {
        return;
    }
    /* A few seconds in which the machine is mostly in all-RAM mode with
       interrupts off (vault.s): the SID would hold whatever note it was on
       as a drone. Silence it first, and bring it back after if it was on. */
    music_pause();
    isle_build(ISLE_HOME);
    music_resume();
    memcpy(home_fx, isle_fx, NUM_HOME_PLACES);
    memcpy(home_fy, isle_fy, NUM_HOME_PLACES);
    memcpy(home_port_x, isle_port_x, 4);
    memcpy(home_port_y, isle_port_y, 4);
    home_biome = isle_biome;
    home_ok = 1;
    lair_clear();
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

/* A new game starts in Millford, at the smithy's door. game_init() set
   everything else. */
void game_start(void)
{
    town_build(TOWN_MILLFORD);
    hero.x = town_sx[TOWN_MILLFORD];
    hero.y = town_sy[TOWN_MILLFORD];
}

/* In HIGH, where it was when it was part of savegame.c: MAIN has no room
   for it in the networked build. (The offline build has the whole module
   with the map's engine.) */
#ifndef Q4K_OFFLINE
#pragma code-name (push, "HICODE")
#endif

/* After load_game(): rebuild the maps rather than store them, all from
   lore_seed. items has to be restored first: whether the keeper is still
   down there depends on it. Home is only needed once you have left
   Millford; a continent is put away if you are in a town or a dungeon on
   it. */
void scene_restore(void)
{
    unsigned char x = hero.x, y = hero.y;

    home_forget();
    if (quest_stage > Q_START) {
        home_ensure();
    }
    if (land != ISLE_HOME) {
        isle_story = slot_of(land) & 1;
        isle_build(land);
        if (scene == SCENE_TOWN || scene == SCENE_DUNGEON) {
            vault_park(0);
        }
    }
    if (scene == SCENE_WORLD) {
        local_w = local_h = WORLD_SIZE;
    } else if (scene == SCENE_ISLE) {
        local_w = ISLE_W;
        local_h = ISLE_H;
    }
    if (scene == SCENE_TOWN) {
        town_build(place);
        hero.x = x;                     /* town_build puts you at the door */
        hero.y = y;
    }
    /* Only the level you saved on comes back with its map; the rest of
       that dungeon starts unexplored again. What you can see now is worked
       out afresh before the next draw. */
    fov_forget();
    if (scene == SCENE_DUNGEON) {
        dungeon_build(place, depth);
        reseed();
        fov_enter(place, depth);
        memcpy(fov_seen, save_seen, FOV_BYTES);
    }
}

#ifndef Q4K_OFFLINE
#pragma code-name (pop)
#endif
