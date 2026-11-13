/*
 * Quest for the King -- engines loaded from the disk (offline build only).
 *
 * The offline build keeps a core in memory -- the state, the screen, the
 * sprites, the music player, the disk -- and loads everything else into one
 * slot at $4000 when it is wanted: the map's engine (walking, towns,
 * dungeons, sailing, the shops' sums, help), the fight's, the talk's (people
 * and inns), the shop's. The title screen is what the slot holds when the
 * game starts: it comes in the PRG itself. See ENGINES.md.
 *
 * A slot holds one engine at a time, so an engine may call the core and
 * itself, and nothing else; the core may call an engine only through
 * engine_call() or, at the start, by loading it and calling its entry.
 * tools/mkengines.py checks that after every link. Every engine's variables
 * are in the core's BSS (only the code and the constants are in the slot),
 * so a call can go out of the map into the fight and back with the map's
 * statics just as they were.
 */

#ifndef Q4K_ENGINE_H
#define Q4K_ENGINE_H

#define ENG_TITLE  0   /* in the PRG: what the slot holds at the start */
#define ENG_MAP    1
#define ENG_FIGHT  2
#define ENG_TALK   3
#define ENG_SHOP   4
#define ENG_COUNT  5

/* What engine_call() hands an engine, besides the game's own state, and
   what it hands back: a line of text (a name going in, a note coming out). */
extern unsigned char eng_a, eng_b;
extern char          eng_text[41];

/* Which engine the slot holds. */
extern unsigned char eng_held;

/* Make sure the slot holds engine `e`, loading it (and its music: see
   engine.c) if it does not. Waits for the disk if it is not there. */
void          engine_load(unsigned char e);

/* Load `e`, call `entry` in it, load back whatever the slot held before,
   and return what `entry` did. Not reentrant: nothing an engine_call()
   calls may make another. */
unsigned char engine_call(unsigned char e, unsigned char (*entry)(void));

/* The entries. map_entry() never returns. */
void          map_entry(unsigned char restored);

/* The fight: eng_a is what kind (FIGHT_*), eng_b the dungeon for a keeper.
   It leaves fight_spoils for the map, which has the code to share them out. */
#define FIGHT_WANDER  0
#define FIGHT_UNDEAD  1
#define FIGHT_BOSS    2
#define FIGHT_DRAGON  3
#define SPOIL_WON     0x01   /* treasure_drop() */
#define SPOIL_ARMED   0x02   /* ...and gear_drop()'s roll */
extern unsigned char fight_spoils;
unsigned char fight_entry(void);

/* Talk: eng_a is TALK_NPC (eng_b the person, eng_text their name) or
   TALK_INN (eng_b the inn, eng_text its name). For a person with no lines
   on the disk, it returns 1 with what they handed over in eng_text. */
#define TALK_NPC  0
#define TALK_INN  1
unsigned char talk_entry(void);

/* The shop: eng_a is the kind (SHOP_*), or SHOP_GEAR to take up what a
   beaten monster dropped. */
#define SHOP_GEAR 0xFF
unsigned char shop_entry(void);

#endif /* Q4K_ENGINE_H */
