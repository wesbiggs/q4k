/*
 * Quest for the King -- the smooth-scrolling prototype's hand-off.
 *
 * The prototype is a disk (dist/openworld.d64): the bootstrap, OPENWORLD
 * (src/openworld.c), two engines built from one source (src/ow/engine.c and
 * smooth.s), OW-CPU and OW-REU, and what the bootstrap loads for them. It
 * shows the splash screen (SPLASH-P or SPLASH-N, by the machine's video
 * system) until F1, a new game, or F3, a saved one (OW-SAVE, which the disk
 * comes with: nothing saves yet); then it looks for an REU, loads what both
 * engines read and neither changes (OW-DATA: the tile sets and the towns'
 * maps, at the top of the engines' memory, where their own files do not
 * reach) and the music a game starts with (a SID file, under the KERNAL as
 * the game has it), builds Keldmarch into the RAM under the I/O area (the
 * vault) and each continent once to see where its places and its landing
 * are, moors the ships, writes what it found here and loads the engine that
 * suits the machine, which takes everything from here. So neither engine
 * carries the tiles, and each carries only its own way of scrolling. (Both
 * carry the land generator: there is room under the KERNAL for one
 * continent beside the music, so an engine builds the one the hero is
 * nearest as they go: src/lib/ow_gen.s.)
 *
 * The block is in $02A7-$02FF, which neither the KERNAL nor cc65 uses, so
 * it survives the loads both ways (N in an engine goes back through the
 * bootstrap, with the next seed).
 */

#ifndef Q4K_OW_H
#define Q4K_OW_H

#define OW_MAGIC 0x5A           /* the block is filled in */
#define OW_SHIPS 8
#define OW_PLACES 5             /* home's: NUM_HOME_PLACES */

/* What the bootstrap found of the REU. */
#define OW_NO_REU   0
#define OW_REU_OK   1           /* and its transfers reach colour RAM */
#define OW_REU_BLIND 2          /* found, but they do not: not used */

typedef struct {
    unsigned char magic;
    unsigned char reu;          /* OW_NO_REU, ... */
    unsigned char seed;         /* the lands' lore_seed */
    unsigned char home_biome;
    unsigned char home_fx[OW_PLACES], home_fy[OW_PLACES];
    unsigned char ipx[4][2], ipy[4][2];  /* each continent's places */
    unsigned char ibiome[4];
    unsigned char slot[4];      /* which story place: game.c's slot_of() */
    unsigned int  ship_x[OW_SHIPS];      /* in the world, 384 by 256 */
    unsigned char ship_y[OW_SHIPS];
    unsigned char hook;         /* the tests' way to press F1 or F3: below */
    unsigned char video;        /* OW_PAL, OW_NTSC */
    unsigned char saved;        /* the game came from a save, not F1 */
    unsigned int  start_x;      /* where the hero starts, in the world */
    unsigned char start_y;
    unsigned char tune;         /* which SID file is under the KERNAL
                                   (tune_name[]: music_map.h), or OW_NO_TUNE */
} OwWorld;

#define OW_NO_TUNE 0xFF

#define OW ((OwWorld *)0x02A7)

/* Load a program from unit 8 over whatever is running and start it at
   $080D, from a loader copied into the tape buffer first. The screen and
   the interrupts must be the KERNAL's again before it is called. Does not
   return; the border flashes if the load fails. */
void __fastcall__ ow_run(const char *name);

/* A floppy disk blinking in the middle of the screen, from now until an
   engine has the screen: the machine is busy. */
void ow_disk_on(void);

/* 0 no REU, 1 one whose transfers reach colour RAM, 2 one whose do not:
   OW_NO_REU, OW_REU_OK, OW_REU_BLIND. */
unsigned char ow_reu_detect(void);

/* --- The splash screen (src/lib/ow_splash.s) --------------------------- */

#define OW_PAL  0
#define OW_NTSC 1               /* anything else has no picture */

/* Where the bootstrap loads SPLASH-P or SPLASH-N: its text, in a page, and
   then the picture (tools/mksplash.py), 21K, clear of the bootstrap below
   and its BSS above (cfg/c64-owboot.cfg). OW-DATA is loaded over it later. */
#define OW_SPLASH_AT 0x4000

#define OW_KEY_F1 0x85          /* a new game */
#define OW_KEY_F3 0x86          /* a saved one */

/* While the splash waits for a key OW->hook is OW_HOOK_WAIT, and a key
   written there is taken as pressed: tools/screendump.py --splash. */
#define OW_HOOK_OFF  62
#define OW_HOOK_WAIT 0xA5

unsigned char ow_video(void);
/* Show the picture at OW_SPLASH_AT until F1 or F3: OW_KEY_F1, OW_KEY_F3, or
   0 if there is no picture there for this machine. */
unsigned char ow_splash(void);
extern unsigned char ow_splash_ticks;   /* frames the player took, mod 256 */

/* A saved game, as far as the open world goes: OW-SAVE on the disk
   (tools/mkowsave.py writes the one it comes with). */
#define OW_SAVE_MAGIC 0x53
typedef struct {
    unsigned char magic;
    unsigned char seed;
    unsigned int  x;            /* the hero's square in the world */
    unsigned char y;
} OwSave;

#endif /* Q4K_OW_H */
