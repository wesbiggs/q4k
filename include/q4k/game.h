/*
 * Quest for the King -- the game's state, as the generators and the screens
 * of text read and write it. The open world keeps it in its game block
 * (owgame.h); each program defines these variables itself (the engine,
 * src/ow/owstand.c, the bootstrap) and copies them in and out.
 *
 * Home is 64x64, in the vault under the I/O area; towns (21x19), dungeon
 * levels (DUN_W x DUN_H) and the continents over the sea (64x31) are built
 * into one RAM buffer, local[].
 */

#ifndef Q4K_GAME_H
#define Q4K_GAME_H

#include <q4k/loot_data.h>
#include <q4k/town_data.h>

/* Quest progress: what talk.txt's lines and the next step (quest.c) go by. */
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
#define Q_REWARDED 10   /* Got 1000 gold from the king                   */

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
/* dungeon_build() hides at least one chest on a level, and up to this many. */
#define DUN_CHESTS   3

/* One bit per treasure, set once it has been taken. Dungeon chests come
   first, DUN_CHESTS bits per level, so that a new line in assets/loot.txt
   moves nothing that is already saved; loot_table[i] is bit DUN_LOOT + i. */
#define DUN_LOOT      (NUM_DUNGEONS * MAX_DEEP * DUN_CHESTS)
#define LOOTED_BYTES  ((DUN_LOOT + NUM_LOOT + 7) / 8)

extern unsigned char depth;    /* dungeon level, 1 at the entrance  */

/* Which land you are on, or went into a town or a dungeon from: ISLE_HOME (4)
   or a compass point, 0 to 3. */
extern unsigned char land;

/* The current town or dungeon level, or a continent being built.
 *
 * Rows are LOCAL_STRIDE apart whatever the place's width, so finding a
 * square is a shift rather than a multiply. A town fills the top-left
 * LOCAL_W x LOCAL_H of it; a dungeon level is DUN_W x DUN_H and scrolls.
 *
 * There is no storage of its own: local[] is the font image, which is dead
 * once charset_install() has copied it to $3800 (see src/lib/local.s). So
 * the charset must go in before anything here is written, and only once. */
#define LOCAL_SHIFT   6
#define LOCAL_STRIDE  (1 << LOCAL_SHIFT)
#define LOCAL_ROWS    32
#define LOCAL_AT(x, y) (((unsigned int)(y) << LOCAL_SHIFT) + (x))

#define DUN_W  48
#define DUN_H  32

extern unsigned char local[LOCAL_STRIDE * LOCAL_ROWS];
extern unsigned char local_w, local_h;   /* the current place's size */

extern unsigned int  game_turn;
/* The party's, shared: any of them can spend or drink them. Each member's
   arms are their own (actor.h). */
extern unsigned int  gold;
extern unsigned char potions;
extern unsigned char quest_stage;
/* Rolled once per game (never 0, which means "not rolled yet") and saved:
   the lands, the towns and the dungeons are made from it. */
extern unsigned char lore_seed;
extern unsigned char items;       /* IT_* bitmask */
extern unsigned char looted[LOOTED_BYTES];

/* Treasure maps the hero carries, MAPS of them, kept as six rows of MAPS
   bytes so a save is one memcpy: which land, plus one (0 for an empty slot),
   the buried square, the place the map measures from (its square, then
   which of the land's places it is). The open world drops none yet; the
   party screen (shop.c) counts them. */
#define MAPS     4
#define TM_LAND  0
#define TM_X     MAPS
#define TM_Y     (2 * MAPS)
#define TM_RX    (3 * MAPS)
#define TM_RY    (4 * MAPS)
#define TM_PL    (5 * MAPS)
#define TM_SIZE  (6 * MAPS)
extern unsigned char tmap[TM_SIZE];

/* What the bows shoot, from the party's one quiver: one a shot, found after
   fights and sold ten at a time by the bowyer, who throws twelve in with a
   first bow (and the smith with an archer's hunting bow). */
extern unsigned char arrows;
#define ARROW_PACK   10
#define ARROW_PRICE  12
#define ARROW_FREE   12
#define ARROW_MAX    40

extern const unsigned char dungeon_key[NUM_DUNGEONS];   /* IT_* needed  */
extern const unsigned char dungeon_prize[NUM_DUNGEONS]; /* IT_* it drops */
extern const unsigned char dungeon_deep[NUM_DUNGEONS];  /* levels       */

/* The places on the land. Home has five, in the order isle.h gives (Millford,
   the castle, the Hollow Crown, Maldrath's lair, a town of its own), and each
   continent two: its story place and a town of its own. Which is which on a
   continent depends on the game: see slot_of(). */
/* A town the generator makes (src/lib/town_gen.c): one to a land. */
#define TOWN_PROC    NUM_TOWNS

#define PLACE_NONE   0xFF
#define PLACE_LAIR   3          /* home's */
#define NUM_HOME_PLACES 5

extern unsigned char home_fx[NUM_HOME_PLACES], home_fy[NUM_HOME_PLACES];
extern unsigned char home_biome;        /* what home is mostly made of */

/* The inn of a town the generator made: after the hand-drawn ones. */
#define INN_PROC     255

/* The story is told across the four continents in a different order each
   game: slot 0 is where the Tidal Key is (a town, Wardlow), 1 the Drowned
   Stair, 2 Ashcombe, 3 Blackpit. slot_of(d) is the slot of compass point d;
   the continent opens when the story reaches Q_EMBER + its slot. */
unsigned char slot_of(unsigned char d);

/* The people of the town you are standing in, and its way out. */
extern const TownEnt *here_ents;
extern unsigned char  here_n;
extern unsigned char  here_ex, here_ey;

unsigned char has(unsigned char item);
void          give(unsigned char item);

#endif /* Q4K_GAME_H */
