/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Quest for the King -- the smooth-scrolling prototype's hand-off.
 *
 * The prototype is a disk (dist/openworld.d81): the bootstrap, OPENWORLD
 * (src/openworld.c), two engines built from one source (src/ow/engine.c and
 * smooth.s), OW-CPU and OW-REU, and what the bootstrap loads for them. It
 * shows the splash screen (SPLASH-P or SPLASH-N, by the machine's video
 * system) until F1, a new game, or F3, a saved one (OW-SAVE, which the disk
 * comes with: nothing saves yet); then it looks for an REU (and lays out
 * its load cache: ow_cload() below), loads what both engines read and
 * neither changes (OW-DATA: the tables the tile sets share and every
 * module's constants, at the top of the engines' memory, where their own
 * files do not reach) and the music a game starts with (a SID file, under the KERNAL as
 * the game has it), builds Keldmarch into the RAM under the I/O area (the
 * vault) and each continent once to see where its places and its landing
 * are, moors the ships, writes what it found here and loads the engine that
 * suits the machine, which takes everything from here. So neither engine
 * carries the tiles, and each carries only its own way of scrolling. (Both
 * carry the land generator, in the world's overlay: there is room under the KERNAL for one
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
    unsigned char slot[4];      /* which story place: slot.c's slot_of() */
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

/* The open world's other programs -- the party's making (src/owparty.c),
   the talk, the shops and the inns, all in src/owtown.c -- take the engine's place
   for a while and load it back when they are done, and the engine carries
   on from the game block (owgame.h), which says where it was. This block
   says what the program is for. Its own, past the bootstrap's (which ends
   at $02EC), so that the bootstrap's layout does not move. */
#define OWB_NONE  0
#define OWB_TALK  1             /* a conversation: npc */
#define OWB_BACK  2             /* (done: the engine carries on) */
#define OWB_NEW   3             /* the party's making, then Millford */
#define OWB_QUICK 4             /* a lone warrior outside Millford: tests */
#define OWB_SHOP  5             /* a shop: arg is its kind (SHOP_*) */
#define OWB_INN   6             /* an inn: arg is which (INN_PROC: ours) */
#define OWB_PARTY 7             /* the party and what they carry */
#define OWB_LOAD  8             /* (to the bootstrap) F3: the saved game */
#define OWB_FIGHT 9             /* a fight: arg OWF_*, npc the ground (a
                                   tile), the dungeon or the monster */
#define OWB_HELP  10            /* ?: the keys, and what to do next */
#define OWB_SPOILS 11           /* (owtown to itself, after a fight won)
                                   what the foe left: arg OWS_*, npc a roll
                                   for which gear (src/lib/ow_tmap.c) */

#define OWF_WANDER 0            /* what wanders out at you, on npc's ground */
#define OWF_KEEPER 1            /* a dungeon's keeper: npc the dungeon */
#define OWF_DRAGON 2            /* Maldrath, in his lair */
#define OWF_UNDEAD 3            /* the dead, woken by L: npc the ground */
#define OWF_FOE    4            /* a dungeon's monster that reached the
                                   party, or that it walked into: npc its
                                   index in the game block's foe_a[] */
#define OWF_SEA    5            /* what comes up out of the deep sea at
                                   the king's ship (the seafight module) */
#define OWF_LOOT   0x80         /* or'd in, the tests': a win leaves gear,
                                   and on the overworld a map, every time */

#define OWS_GEAR   1            /* OWB_SPOILS: a blade, mail or a bow */
#define OWS_MAP    2            /* and/or a treasure map */

typedef struct {
    unsigned char mode;         /* OWB_* */
    unsigned char npc;          /* the person (NPC_* in town_data.h) */
    unsigned char arg;
    unsigned char song;         /* 1 + the song of the tune under the KERNAL
                                   that was playing, paused where it was (so
                                   that the next program carries on with it),
                                   or 0: start the scene's from the top */
} OwBack;

#define OWB ((OwBack *)0x02ED)

/* Load a program from unit 8 over whatever is running and start it at
   $080D, from a loader copied into the tape buffer first. The screen and
   the interrupts must be the KERNAL's again before it is called. Does not
   return; the border flashes if the load fails. */
void __fastcall__ ow_run(const char *name);

/* Load a file from unit 8 as cbm_load(name, 8, at) does (at 0: to its own
   address), through the REU cache when there is one (src/lib/ow_cache.s):
   the first time from the disk and stashed, after that fetched. The end
   address, or 0 and _oserror. Never for OW-SAVE, which changes. */
unsigned int __fastcall__ ow_cload(const char *name, void *at);
extern unsigned int cache_hits, cache_misses;
/* Whether ow_cload(name) would fetch it from the REU, with no drive: the
   music need not be paused for it (VICE's sound and the serial bus). */
unsigned char __fastcall__ ow_cached(const char *name);

/* The bootstrap's: the REU's directory for the cache kept if it is this
   build's, or laid out anew (src/lib/ow_cinit.s). */
void ow_cinit(void);

/* From another program of the open world's back to the engine, which
   carries on from the game block (src/lib/ow_back.c). Does not return. */
void ow_back(void);

/* A floppy disk blinking in the middle of the screen, from now until an
   engine has the screen: the machine is busy. */
void ow_disk_on(void);

/* What the loader's icon (src/lib/ow_disk.s, resident at $CF14) shows for
   the next load, or for ow_disk_on(): set before it, and back to the disk
   after every load. OW_ICON_MODE is the disk (0), OW_ICON_ROLL, the die
   rolling while the world is built (its faces at OW_ICON_D20), or
   OW_ICON_FLASH, someone's sprite (a block in the engine's VIC bank, whose
   address goes in OW_ICON_SRC, and its colour in OW_ICON_COL), doubled and
   blinking. The pictures of the disk and the die are OW-ICON
   (assets/icons.txt), which the bootstrap loads under the KERNAL. */
#define OW_ICON_MODE   (*(unsigned char *)0xCF1A)
#define OW_ICON_COL    (*(unsigned char *)0xCF1B)
#define OW_ICON_SRC    (*(unsigned int *)0xCF27)
#define OW_ICON_DISK   0x00
#define OW_ICON_FLASH  0x01
#define OW_ICON_ROLL   0x80
#define OW_ICON_ART    0xFF00
#define OW_ICON_D20    (OW_ICON_ART + 64)

/* 0 no REU, 1 one whose transfers reach colour RAM, 2 one whose do not:
   OW_NO_REU, OW_REU_OK, OW_REU_BLIND. */
unsigned char ow_reu_detect(void);

/* The bootstrap's: roads on home, just built in the vault, joining Millford
   (isle_fx[0]), the castle, home's town and the four ports, through
   isle_fx/fy and isle_port_x/y as the build left them (src/lib/ow_roads.s,
   which uses $4000-$6FFF). */
void ow_roads(void);

/* --- The splash screen (src/lib/ow_splash.s) --------------------------- */

#define OW_PAL  0
#define OW_NTSC 1               /* anything else has no picture */

/* Where the bootstrap loads SPLASH-P or SPLASH-N: its text, in a page, and
   then the picture (tools/mksplash.py), 21K, clear of the bootstrap below
   and its BSS above (cfg/c64-owboot.cfg). OW-DATA is loaded over it later. */
#define OW_SPLASH_AT 0x4000

#define OW_KEY_F1 0x85          /* a new game */
#define OW_KEY_F3 0x86          /* a saved one */
#define OW_KEY_F2 0x89          /* (the left SHIFT+F1) a new game from a
                                   seed the player types: the only key here
                                   with bit 3, which the bootstrap reads */
#define OW_KEY_QUICK 0x87       /* the tests': a lone warrior outside Millford,
                                   as the prototype began (no matrix key) */

/* While the splash waits for a key OW->hook is OW_HOOK_WAIT, and a key
   written there is taken as pressed: tools/screendump.py --splash. */
#define OW_HOOK_OFF  62
#define OW_HOOK_WAIT 0xA5

/* And the tests' way to have OW-CPU with an REU (screendump.py
   --cpu-engine, written while the splash waits): the REU taken for one
   whose transfers do not reach colour RAM (OW_REU_BLIND), which still
   caches. Cleared by a cold start of the bootstrap. Past OWB. */
#define OW_TESTCPU    (*(unsigned char *)0x02F1)
#define OW_TESTCPU_ON 0xC7

/* A talk or a fight (owtalk, owtown) moved the quest on: the engine says
   so in the text window when it has the screen again, and clears it.
   Zero from a reset, which clears page 2. Past OW_TESTCPU. */
#define OW_NEWTASK    (*(unsigned char *)0x02F2)

/* SHIFT+F1 (F2) on the splash: the bootstrap runs owseed (src/owseed.c)
   before owparty, which asks for the world's seed and clears it. If the seed
   is another, it sends the bootstrap back to build that world, as N in an
   engine does, which then runs owparty. Set (or cleared) by the splash.
   Past OW_NEWTASK. */
#define OW_ASKSEED    (*(unsigned char *)0x02F3)

unsigned char ow_video(void);
/* Show the picture at OW_SPLASH_AT until F1, SHIFT+F1 or F3: OW_KEY_F1,
   OW_KEY_F2, OW_KEY_F3, or 0 if there is no picture there for this
   machine. With `saved` 0 (no OW-SAVE on the disk) the text is only its
   first part, "F1 Start New Game", centred, and F3 does nothing; else it
   is $80 (the bootstrap's load_save()). */
unsigned char __fastcall__ ow_splash(unsigned char saved);
extern unsigned char ow_splash_ticks;   /* frames the player took, mod 256 */

/* A saved game is the game block (owgame.h) in OW-SAVE, which the disk
   comes with one of (tools/mkowsave.py) until the game saves over it. */

#endif /* Q4K_OW_H */
