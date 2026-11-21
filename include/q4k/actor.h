/*
 * q4k -- the party.
 *
 * Up to three who answered the king's letter, made on the title screen, and
 * a fourth place kept for a companion the computer will steer one day. Each
 * has a class, their own health and their own arms; the gold, the draughts,
 * the arrows, the experience and the level are the party's.
 *
 * party[0] leads: on the map the party is one figure, the leader's, at
 * hero.x, hero.y, and only in a fight does everyone stand on a square of
 * their own. The level and the experience are the party's: party_level and
 * party_xp.
 */

#ifndef Q4K_ACTOR_H
#define Q4K_ACTOR_H

#define MAX_PARTY   4
#define MAX_MADE    3   /* the title screen makes up to this many */
#define NAME_LEN    9   /* 8 characters plus a terminator */

/* Classes. Only a warrior wields anything better than a short sword, only an
   archer draws anything better than a shortbow, and only a wizard casts --
   and a wizard wears nothing heavier than robes. */
#define CL_WARRIOR  0
#define CL_ARCHER   1
#define CL_WIZARD   2
#define NUM_CLASSES 3

/* Kept small: four of these are in memory, and four more in the save. A
   member with no health has fallen (only in a fight: they get up after it);
   their glyph is their class's, and their figure is white. */
typedef struct {
    unsigned char x, y;         /* world position: party[0]'s is the party's */
    unsigned char hp, hp_max;
    char          name[NAME_LEN];
    unsigned char cls;          /* CL_*                                      */
    unsigned char weapon;       /* index into weapon_names (battle.h)        */
    unsigned char armour;       /* armour_names                              */
    unsigned char bow;          /* bow_names; 0 none                         */
    unsigned char mp, mp_max;   /* a wizard's; 0 for everyone else          */
    unsigned char known;        /* spells learned, one bit each (SP_*)       */
    unsigned char ready;        /* ...and of those, the ones prepared        */
} Actor;

extern Actor         party[MAX_PARTY];
extern unsigned char party_count;
extern unsigned char party_level;
extern unsigned int  party_xp;

/* The character the player is currently steering. */
#define hero (party[0])

extern const char *const   class_names[NUM_CLASSES];
extern const unsigned char class_glyph[NUM_CLASSES];

/* One member, the leader, called Alaric: what a new game starts with until
   the title screen has made the real party. */
void party_init(void);

/* Make member `i` (party_count grows to cover it) a level-1 `cls` called
   `name`, with nothing in their hands. */
void party_make(unsigned char i, const char *name, unsigned char cls);

/* Health and magic for the party's level, for a member: full health from it
   and the class, and a wizard's magic. party_grow() raises everyone's to the
   party's level and fills them up, as a level gained does. */
unsigned char hp_for(unsigned char cls, unsigned char level);
void          party_grow(void);

/* Everyone mended, up and with full magic: a bed, or waking in Millford. */
void          party_rest(void);

/* Once a turn on the map: a wizard's magic comes back slowly. */
void          party_tick(void);

/* The member whose turn it is in a fight, or 0xFF outside one: the panel
   marks them. */
extern unsigned char party_turn;


#endif /* Q4K_ACTOR_H */
