#include <cbm.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/dungeon.h>
#include <q4k/fov.h>
#include <q4k/game.h>
#include <q4k/rle.h>
#include <q4k/savegame.h>

/* Top of BASIC memory. The KERNAL treats anything above it as out of
   bounds, and our BSS -- where the save block lives -- starts at $A000,
   which is exactly where MEMSIZ points on a stock machine. Without this,
   LOAD walks into the weeds. modem.c raises it for the same reason. */
#define MEMSIZ (*(unsigned int *)0x0283)

#define SAVE_DEVICE 8
#define SAVE_NAME   "Q4K.SAV"
#define SCRATCH_CMD "S0:Q4K.SAV"
#define MAGIC       "Q4K3"   /* 3: the bow */

typedef struct {
    char          magic[4];
    /* Not called "hero": that is a macro for party[0], and it would expand
       here. cc65 also declines to assign structs this large, so the copies
       below go through memcpy. */
    Actor         who;
    unsigned int  turn;
    unsigned int  gold;
    unsigned char potions;
    unsigned char weapon_lv;
    unsigned char armour_lv;
    unsigned char bow_lv;
    unsigned char quest_stage;
    unsigned char items;
    unsigned char lore_seed;
    unsigned char scene;
    unsigned char place;
    unsigned char depth;
    unsigned char lit[LOCAL_W * LOCAL_H];
} SaveGame;

static SaveGame blob;

static void gather(void)
{
    memcpy(blob.magic, MAGIC, 4);
    memcpy(&blob.who, &hero, sizeof(blob.who));
    blob.turn        = game_turn;
    blob.gold        = gold;
    blob.potions     = potions;
    blob.weapon_lv   = weapon_lv;
    blob.armour_lv   = armour_lv;
    blob.bow_lv      = bow_lv;
    blob.quest_stage = quest_stage;
    blob.items       = items;
    blob.lore_seed   = lore_seed;
    blob.scene       = scene;
    blob.place       = place;
    blob.depth       = depth;
    memcpy(blob.lit, lit, sizeof(blob.lit));
}

unsigned char save_game(void)
{
    MEMSIZ = 0xC700;
    gather();

    /* CBM DOS will not overwrite, so scratch any previous save first. The
       command channel is unit 8, secondary address 15. */
    cbm_open(15, SAVE_DEVICE, 15, SCRATCH_CMD);
    cbm_close(15);

    /* cbm_save takes a byte count, not an end address. */
    if (cbm_save(SAVE_NAME, SAVE_DEVICE, &blob, sizeof(blob)) != 0) {
        return SAVE_EDISK;
    }
    return SAVE_OK;
}

unsigned char load_game(void)
{
    MEMSIZ = 0xC700;

    if (cbm_load(SAVE_NAME, SAVE_DEVICE, &blob) == 0) {
        return SAVE_EDISK;
    }
    if (memcmp(blob.magic, MAGIC, 4) != 0) {
        return SAVE_EFORMAT;
    }

    memcpy(&hero, &blob.who, sizeof(blob.who));
    game_turn   = blob.turn;
    gold        = blob.gold;
    potions     = blob.potions;
    weapon_lv   = blob.weapon_lv;
    armour_lv   = blob.armour_lv;
    bow_lv      = blob.bow_lv;
    quest_stage = blob.quest_stage;
    items       = blob.items;
    lore_seed   = blob.lore_seed;
    scene       = blob.scene;
    place       = blob.place;
    depth       = blob.depth;

    /* Rebuild the map rather than store it. items has to be restored first:
       whether the keeper is still down there depends on it. */
    if (scene == SCENE_TOWN) {
        rle_unpack(local, town_map[place], sizeof(local));
    } else if (scene == SCENE_DUNGEON) {
        dungeon_build(place, depth);
        reseed();
    }
    memcpy(lit, blob.lit, sizeof(lit));

    return SAVE_OK;
}
