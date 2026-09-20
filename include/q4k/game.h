/*
 * Quest for the King -- game state and the scene the player is standing in.
 *
 * Three scenes share one set of movement and drawing code. The overworld is
 * a wrapping 64x64 map in ROM; towns and dungeon levels are 21x19 and live in
 * one RAM buffer, rebuilt whenever the player goes somewhere.
 */

#ifndef Q4K_GAME_H
#define Q4K_GAME_H

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
extern unsigned char quest_stage;
extern unsigned char items;       /* IT_* bitmask */

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
extern const unsigned char dungeon_deep[NUM_DUNGEONS];  /* levels       */

/* -1 if (x,y) is not a dungeon mouth. */
signed char   dungeon_at(unsigned char x, unsigned char y);
signed char   town_at(unsigned char x, unsigned char y);

void          enter_town(unsigned char index);
void          leave_local(void);

unsigned char has(unsigned char item);
void          give(unsigned char item);

#endif /* Q4K_GAME_H */
