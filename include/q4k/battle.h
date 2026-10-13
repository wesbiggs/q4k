/*
 * Quest for the King -- fighting.
 *
 * Every fight happens on a field: a map of the ground you were crossing
 * (assets/arenas.txt), a dungeon room below ground, or Maldrath's lair. It
 * walks at you and you walk into it to strike. The caller redraws the map
 * afterwards.
 */

#ifndef Q4K_BATTLE_H
#define Q4K_BATTLE_H

#include <q4k/game.h>   /* NUM_DUNGEONS */

#define NUM_WEAPONS 5
#define NUM_ARMOUR  5

extern const char *const   weapon_names[NUM_WEAPONS];
extern const unsigned char weapon_atk[NUM_WEAPONS];
extern const unsigned int  weapon_cost[NUM_WEAPONS];

extern const char *const   armour_names[NUM_ARMOUR];
extern const unsigned char armour_def[NUM_ARMOUR];
extern const unsigned int  armour_cost[NUM_ARMOUR];

/* Bows are a second slot: you carry one as well as your weapon, and F in
   a fight shoots it. Index 0 is having none. Range is in squares, and a
   shot flies straight up, down, left or right. */
#define NUM_BOWS 4

extern const char *const   bow_names[NUM_BOWS];
extern const unsigned char bow_atk[NUM_BOWS];
extern const unsigned char bow_range[NUM_BOWS];
extern const unsigned int  bow_cost[NUM_BOWS];

#define BATTLE_WON   0
#define BATTLE_FLED  1
#define BATTLE_DEAD  2

/* The bestiary. Out here rather than private to battle.c so that
   src/arena.c can put any of it on any field; nothing in the game itself
   reaches in. */
typedef struct {
    const char   *name;
    unsigned char hp, atk, def, xp;
    unsigned int  gold;
    unsigned char glyph, color;   /* how it looks on a field */
    unsigned char speed;          /* moves a turn, in tenths: 15 = 1.5 */
    unsigned char size;           /* squares a side; the glyphs run on
                                     from `glyph` in reading order */
    unsigned char shot, reach;    /* its ranged attack, and range in
                                     squares; left out, and so 0, for the
                                     many things that only close in */
    unsigned char kind;           /* what it shoots, SHOT_*: left out, an
                                     arrow */
} Monster;

/* Wandering monsters, and the level each starts turning up at. */
typedef struct {
    unsigned char from;
    Monster       m;
} Wanderer;

extern const Wanderer      wanderers[];
extern const unsigned char num_wanderers;
extern const Monster       keepers[NUM_DUNGEONS];
extern const Monster       maldrath;

/* The most of one kind that come at once. The panel lists their health
   three to a row in two rows, so six is all it has room for. */
#define MAX_FOES 6

/* Fight `count` of `m` (1 to MAX_FOES; fewer if the field has no room for
   them all) on the field for `ground` (an overworld tile; T_FLOOR is a
   dungeon room, T_LAIR Maldrath's), with R to run if `can_flee`. Only
   one-square monsters may come more than one at a time. Returns BATTLE_*.
   The caller redraws whatever it had on screen. */
unsigned char battle_fight(const Monster *m, unsigned char ground,
                           unsigned char can_flee, unsigned char count);

/* True if crossing `tile` should bring something out of the scenery. */
unsigned char encounter_roll(unsigned char tile);

/* The dead, for whoever digs where nothing is buried. Returns BATTLE_*. */
unsigned char battle_undead(void);

unsigned char battle_wander(void);
unsigned char battle_boss(unsigned char dungeon);
unsigned char battle_dragon(void);

/* What one draught restores.
 *
 * A share of your full health, not a fixed number. Twelve points was most of
 * a health bar at level one and nothing at level ten -- Maldrath hits for
 * about that much through plate, so drinking cost you a turn to stand still.
 * Damage scales with the game; the cure has to as well. */
unsigned char potion_heal(void);

/* Drink one if there is one and it would do any good. Returns how much it
   healed, or zero if nothing happened. Shared so the in-fight and
   on-the-map versions cannot drift apart. */
unsigned char drink_potion(void);

void          heal_full(void);

/* Where the randomness comes from.
 *
 * cc65's rand() starts from a fixed seed, so without help every playthrough
 * meets the same monsters on the same squares. Sampling a timer at startup
 * is not enough either: the machine is doing the same thing at the same
 * cycle on every boot, and measured across runs the same seed comes up again
 * and again.
 *
 * The one thing here that genuinely is not repeatable is how long the player
 * takes to press a key. rng_stir() folds that in -- how long the wait was,
 * and where the CIA's free-running timer had got to -- and reseed() uses the
 * accumulated result. dungeon_build() seeds deliberately so a level is
 * reproducible, so anything calling it reseeds afterwards. */
void          rng_stir(unsigned int spins);
void          reseed(void);
extern unsigned int last_seed;
extern unsigned char last_chance, last_roll;   /* what reseed() last used */

#endif /* Q4K_BATTLE_H */
