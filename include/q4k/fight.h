/*
 * Quest for the King -- the multicolour fight, a module loaded for a fight.
 *
 * The fight of src/lib/battle.c again, on a field of the smooth-scrolling
 * prototype's multicolour metatiles with everything that moves a sprite:
 * src/fight/, linked on its own (cfg/c64-fight.cfg) into one file, FIGHT,
 * which whoever wants a fight loads at FIGHT_LOAD and calls. The multicolour
 * arena (src/owarena.c) is the first to; the module knows nothing of it.
 *
 *   $4000-$4BFF  the field: screen, facts and characters, one file (f.*)
 *   $4C00-$4FFF  the letters, copied from the ROM
 *   $5000-$53FF  the monster's sprites, one file (s.*)
 *   $5400-$57FF  FIGHT: the party's sprites, the shots ...
 *   $5800-       ... a jump to the fight, this block, and the fight
 *
 * The caller fills in the block at FIGHT (the party, the monster, which
 * field and whose sprites) and calls fight_call(); the module loads the two
 * files it names from the disk (unless the sprites' name is empty: then the
 * caller has, as the open world's ow_fight.c does through its REU cache), takes the screen (VIC bank 1, its own raster
 * interrupt), fights, puts the machine back as it found it and returns
 * BATTLE_*, or FIGHT_NODISK if a file would not load, with the party as the
 * fight left it. It shares the caller's zero page and C stack, clears its
 * own BSS on the way in, and keeps nothing between calls.
 *
 * tools/mkfields.py makes the files; the layout of a field is below.
 */

#ifndef Q4K_FIGHT_H
#define Q4K_FIGHT_H

#include <q4k/actor.h>

#define FIGHT_LOAD   0x5400     /* where the file loads */
#define FIGHT_BASE   0x5800     /* the jump; the block is 3 bytes on */
#define FIGHT_FILE   "fight"

#define FIGHT_NAME   16         /* a monster's name, with its terminator */
#define FIGHT_FNAME  12         /* a file's */

typedef struct {
    /* The party, as the game keeps it; health, magic, draughts and arrows
       come back as the fight left them. */
    Actor         party[MAX_PARTY];
    unsigned char party_count, party_level;
    unsigned char potions, arrows;
    unsigned char tunic[MAX_PARTY];     /* each member's colour */
    /* What they fight: a Monster of battle.h, with its name in place. */
    char          name[FIGHT_NAME];
    unsigned char hp, atk, def, colour, speed, size, shot, reach, kind;
    unsigned char count;                /* how many of it, 1 to MAX_FOES */
    unsigned char flee;                 /* whether the sides are a way out */
    unsigned int  seed;                 /* for rand(): the same fight again */
    char          field[FIGHT_FNAME];   /* "f.grass" */
    char          sprite[FIGHT_FNAME];  /* "s.rat" */
    /* What came of it. */
    unsigned char foes;                 /* how many there were, for the pay */
    /* The music: a song of the tune under the KERNAL ($E7C0, as music.s and
       ow_tune.s have it), played from the fight's own interrupt with the
       effects (sfx.s, voice 3). */
    unsigned char song;                 /* 1 + the song, or 0: none */
    unsigned char carry;                /* in: 1 to carry on where it was
                                           paused; out: 1 if it is paused
                                           where it was, 0 if stopped */
    unsigned char quiet;                /* bit 0 music off, bit 1 effects
                                           off (OwGame's); M and X change
                                           it */
    unsigned int  rounds;               /* out: how many rounds it took */
    /* A level, if winning brings one: the fight shows it (a flash and a
       fanfare), the caller pays it (fq_foes times xp_each). */
    unsigned int  xp_each;              /* experience a foe is worth */
    unsigned int  to_level;             /* what the next level still wants,
                                           or 0: none to be had */
} Fight;

/* 155 bytes; src/fight/block.c lays the same things down in the same order
   for the module, and head.s refuses to link if that is not as long. */

#define FIGHT        ((Fight *)(FIGHT_BASE + 3))
#define fight_call() (((unsigned char (*)(void))FIGHT_BASE)())

#define FIGHT_NODISK 3          /* after BATTLE_WON, _FLED and _DEAD */

/* --- inside the module ---------------------------------------------------- */

/* A field is 20 squares by 9, a square 2x2 characters. */
#define FW           20
#define FH           9

#define F_SCREEN     ((unsigned char *)0x4000)
#define F_INFO       ((unsigned char *)0x4400)
#define F_COLOR      ((unsigned char *)0xD800)

/* The field's facts, from F_INFO (tools/mkfields.py writes them). */
#define FI_PAL       0          /* background, and the two shared colours */
#define FI_HX        3          /* where the leader starts */
#define FI_HY        4
#define FI_MX        5          /* and the monster */
#define FI_MY        6
#define FI_ROCK      7          /* a boulder's four characters */
#define FI_NCHARS    11
#define FI_NSCAT     12         /* how many kinds of scatter, 0-3 ... */
#define FI_SCAT      13         /* ... five bytes each: four characters and
                                   the flag of the square it makes */
#define FI_COLOUR    32         /* each character's colour RAM value, 128 */
#define FI_FLAGS     160        /* each square: FF_* */

#define FF_OPEN      0
#define FF_WATER     1          /* nothing walks on it; a shot crosses it */
#define FF_BLOCK     2
#define FF_KIND      3          /* which of those three */
#define FF_SLOW      0x0C       /* and what a step onto it costs, in tenths */
#define FF_COST(f)   (10 + 5 * (((f) >> 2) & 3))     /* 10, 15 or 20 */

/* Sprite blocks, as the VIC counts them in bank 1. */
#define SPR_MONSTER  0x40       /* $5000: the monster's file */
#define SPR_PARTY    0x50       /* $5400: two frames a class */
#define SPR_ARROW    0x56       /* one block, filled for the way it flies */
#define SPR_FIRE     0x57
#define SPR_WAVE     0x58
#define SPR_BOLT     0x59
#define SPR_CURSOR   0x5A
#define SPR_HIT      0x5B
#define SPR_CORNER   0x5C       /* $5700-$57FF: the rounded corners (corners.s) */

/* The multiplexer (src/fight/mux.s): sixteen sprites to ask for, eight to
   show them with. One is on while its vs_y is not 0. */
#define MX_SPRITES   16
extern unsigned char vs_x[MX_SPRITES], vs_xh[MX_SPRITES], vs_y[MX_SPRITES];
extern unsigned char vs_ptr[MX_SPRITES], vs_col[MX_SPRITES];
extern unsigned char mx_frame;          /* counts the frames */
extern unsigned char mx_drops;          /* sprites the last plan had no room
                                           for */
extern unsigned char mx_most;           /* the most any plan has shown */

void mx_font(void);     /* the ROM's letters to $4C00 */
void mx_colour(void);   /* colour RAM for the field, from its facts */
void mx_start(void);    /* take the screen and the interrupt */
void mx_stop(void);     /* and give them back */
void mx_plan(void);     /* show the sprites as they are now, from the next
                           frame */
void mx_wait(void);     /* until the next frame */

#endif /* Q4K_FIGHT_H */
