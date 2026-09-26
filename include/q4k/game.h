/*
 * Quest for the King -- game state and the scene the player is standing in.
 *
 * Three scenes share one set of movement and drawing code. The overworld is
 * a wrapping 64x64 map in ROM; towns and dungeon levels are 21x19 and live in
 * one RAM buffer, rebuilt whenever the player goes somewhere.
 */

#ifndef Q4K_GAME_H
#define Q4K_GAME_H

#include <q4k/loot_data.h>
#include <q4k/town_data.h>

#define SCENE_WORLD    0
#define SCENE_TOWN     1
#define SCENE_DUNGEON  2

/* Quest progress. Keep in step with STAGES in host/npcd.py -- the number is
   sent to the dialogue daemon so NPCs know where the story has got to. */
#define Q_START     0   /* the letter has come; still in Millford        */
#define Q_SUMMONED  1   /* on the road to the castle                     */
#define Q_QUESTED   2   /* the king has given the quest and the Sigil    */
#define Q_EMBER     3   /* Emberstone taken from the Hollow Crown        */
#define Q_TIDAL     4   /* Vane has handed over the Tidal Key            */
#define Q_TEAR      5   /* Silver Tear taken from the Drowned Stair      */
#define Q_ASHEN     6   /* Mother Ash has handed over the Ashen Key      */
#define Q_HEART     7   /* Dragon's Heart taken from Blackpit            */
#define Q_FORGED    8   /* Garrett has reforged Dragonbane               */
#define Q_VICTORY   9   /* Maldrath banished                             */

/* Things worth carrying. */
#define IT_SIGIL   0x01
#define IT_TIDAL   0x02
#define IT_ASHEN   0x04
#define IT_EMBER   0x08
#define IT_TEAR    0x10
#define IT_HEART   0x20
#define IT_BLADE   0x40

#define NUM_DUNGEONS 3
/* No dungeon is deeper than this: dungeon_deep[] must stay within it, since
   the looted bits below give every dungeon this many levels. */
#define MAX_DEEP     4
/* dungeon_build() hides up to this many chests on a level. */
#define DUN_CHESTS   2

/* One bit per treasure, set once it has been taken. Dungeon chests come
   first, DUN_CHESTS bits per level, so that a new line in assets/loot.txt
   moves nothing that is already saved; loot_table[i] is bit DUN_LOOT + i. */
#define DUN_LOOT      (NUM_DUNGEONS * MAX_DEEP * DUN_CHESTS)
#define LOOTED_BYTES  ((DUN_LOOT + NUM_LOOT + 7) / 8)

extern unsigned char scene;
extern unsigned char place;    /* town index, or dungeon index      */
extern unsigned char depth;    /* dungeon level, 1 at the entrance  */

/* The current town or dungeon level. The overworld is read from ROM. */
extern unsigned char local[LOCAL_W * LOCAL_H];

extern unsigned int  game_turn;
extern unsigned int  gold;
extern unsigned char potions;
extern unsigned char weapon_lv;
extern unsigned char armour_lv;
extern unsigned char bow_lv;      /* 0: none */
extern unsigned char quest_stage;
/* Rolled once per game (never 0, which means "not rolled yet") and saved;
   the dialogue host draws this run's secrets from it. Non-static and
   poke-able, so a test can pin the secrets with --poke lore_seed=N. */
extern unsigned char lore_seed;
extern unsigned char items;       /* IT_* bitmask */
extern unsigned char looted[LOOTED_BYTES];

void          game_init(void);

/* Tile and terrain flag at (x,y) in whatever scene is current. */
unsigned char map_at(unsigned char x, unsigned char y);
unsigned char map_flag(unsigned char x, unsigned char y);
void          map_set(unsigned char x, unsigned char y, unsigned char code);

/* Where the dungeons are, indexed 0..NUM_DUNGEONS-1. */
extern const unsigned char dungeon_wx[NUM_DUNGEONS];
extern const unsigned char dungeon_wy[NUM_DUNGEONS];
extern const char *const   dungeon_names[NUM_DUNGEONS];
extern const unsigned char dungeon_key[NUM_DUNGEONS];   /* IT_* needed  */
extern const unsigned char dungeon_prize[NUM_DUNGEONS]; /* IT_* it drops */
extern const unsigned char dungeon_deep[NUM_DUNGEONS];  /* levels       */

/* -1 if (x,y) is not a dungeon mouth. */
signed char   dungeon_at(unsigned char x, unsigned char y);
signed char   town_at(unsigned char x, unsigned char y);

void          enter_town(unsigned char index);
void          leave_local(void);

unsigned char has(unsigned char item);
void          give(unsigned char item);

#endif /* Q4K_GAME_H */
