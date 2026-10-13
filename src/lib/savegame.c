#include <cbm.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/dungeon.h>
#include <q4k/fov.h>
#include <q4k/game.h>
#include <q4k/isle.h>
#include <q4k/music.h>
#include <q4k/rle.h>
#include <q4k/savegame.h>
#include <q4k/vault.h>
#include <q4k/world.h>

/* Top of BASIC memory. The KERNAL treats anything above it as out of
   bounds, and our BSS -- where the save block lives -- starts at $B000,
   above where MEMSIZ points on a stock machine ($A000). Without this,
   LOAD walks into the weeds. modem.c raises it for the same reason. */
#define MEMSIZ (*(unsigned int *)0x0283)

#define SAVE_DEVICE 8
#define CMD_CHANNEL 15
#define MAGIC       "Q4K8"   /* 8: arrows; 7: maps name a place; 6: treasure maps; 5: bigger dungeons, seen as bits */

/* Everything the drive reads is spelled in numeric bytes: cc65 would
   re-encode a string literal, and "S0:..." came out with a shifted $D3 for
   the S. The drive rejected that as a syntax error, so the old save was
   never scratched, every save after the first failed with FILE EXISTS, and
   KERNAL SAVE never hears about errors on the command channel -- the game
   said "Saved." while the drive light flashed. The name itself went to disk
   shifted too, as graphics characters in the directory. */
static const char scratch_cmd[] = {
    0x53, 0x30, 0x3A,                           /* S0:     */
    0x51, 0x34, 0x4B, 0x2E, 0x53, 0x41, 0x56,   /* Q4K.SAV */
    0
};
#define SAVE_NAME   (scratch_cmd + 3)

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
    unsigned char arrows;
    unsigned char quest_stage;
    unsigned char items;
    unsigned char lore_seed;
    unsigned char scene;
    unsigned char place;
    unsigned char depth;
    unsigned char land;
    unsigned char ret_x, ret_y;
    unsigned char looted[LOOTED_BYTES];
    unsigned char tmap[TM_SIZE];
    unsigned char seen[FOV_BYTES];
} SaveGame;

static SaveGame blob;

/* Reads the drive's status line and reports whether it began "00" (OK).
   Reading it also clears an error, and stops the drive light flashing. */
static unsigned char drive_ok(void)
{
    static char status[2];

    status[0] = 0;
    cbm_read(CMD_CHANNEL, status, 2);
    return status[0] == 0x30 && status[1] == 0x30;
}

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
    blob.arrows      = arrows;
    blob.quest_stage = quest_stage;
    blob.items       = items;
    blob.lore_seed   = lore_seed;
    blob.scene       = scene;
    blob.place       = place;
    blob.depth       = depth;
    blob.land        = land;
    blob.ret_x       = ret_x;
    blob.ret_y       = ret_y;
    memcpy(blob.looted, looted, sizeof(blob.looted));
    memcpy(blob.tmap, tmap, sizeof(tmap));
    memcpy(blob.seen, fov_seen, sizeof(blob.seen));
}

static unsigned char do_save(void)
{
    MEMSIZ = 0xCF00;
    gather();

    /* CBM DOS will not overwrite, so scratch any previous save first. The
       command channel is unit 8, secondary address 15. Its status is not
       checked: with no previous save it is FILE NOT FOUND, which is fine. */
    if (cbm_open(CMD_CHANNEL, SAVE_DEVICE, 15, scratch_cmd) != 0) {
        return SAVE_EDISK;
    }
    drive_ok();

    /* cbm_save takes a byte count, not an end address. Its return value
       only covers the bus; whether the drive actually wrote the file is on
       the command channel, which stays open to ask. */
    if (cbm_save(SAVE_NAME, SAVE_DEVICE, &blob, sizeof(blob)) != 0
        || !drive_ok()) {
        cbm_close(CMD_CHANNEL);
        return SAVE_EDISK;
    }
    cbm_close(CMD_CHANNEL);
    return SAVE_OK;
}

static unsigned char do_load(void)
{
    MEMSIZ = 0xCF00;

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
    arrows      = blob.arrows;
    quest_stage = blob.quest_stage;
    items       = blob.items;
    lore_seed   = blob.lore_seed;
    scene       = blob.scene;
    place       = blob.place;
    depth       = blob.depth;
    land        = blob.land;
    ret_x       = blob.ret_x;
    ret_y       = blob.ret_y;
    memcpy(looted, blob.looted, sizeof(looted));
    memcpy(tmap, blob.tmap, sizeof(tmap));

    /* Rebuild the maps rather than store them, all from lore_seed. items has
       to be restored first: whether the keeper is still down there depends
       on it. Home is only needed once you have left Millford; a continent is
       put away if you are in a town or a dungeon on it. */
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
        hero.x = blob.who.x;            /* town_build puts you at the door */
        hero.y = blob.who.y;
    }
    /* Only the level you saved on comes back with its map; the rest of
       that dungeon starts unexplored again. What you can see now is worked
       out afresh before the next draw. */
    fov_forget();
    if (scene == SCENE_DUNGEON) {
        dungeon_build(place, depth);
        reseed();
        fov_enter(place, depth);
        memcpy(fov_seen, blob.seen, sizeof(fov_seen));
    }

    return SAVE_OK;
}

/* The music steps out of the way of the serial bus, which the interrupt would
   otherwise be stealing cycles from. */
unsigned char save_game(void)
{
    unsigned char r;
    music_pause();
    r = do_save();
    music_resume();
    return r;
}

unsigned char load_game(void)
{
    unsigned char r;
    music_pause();
    r = do_load();
    music_resume();
    return r;
}
