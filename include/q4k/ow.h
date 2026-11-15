/*
 * Quest for the King -- the smooth-scrolling prototype's hand-off.
 *
 * The prototype is three files on its disk (dist/openworld.d64): the
 * bootstrap, OPENWORLD (src/openworld.c), and two engines built from one
 * source (src/ow/engine.c and smooth.s), OW-CPU and OW-REU. The bootstrap
 * looks for an REU, builds every land into the RAM under the ROMs (the
 * vault), moors the ships, writes what it found here and loads the engine
 * that suits the machine, which takes everything from here. So neither
 * engine carries the land generator, and each carries only its own way of
 * scrolling.
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
} OwWorld;

#define OW ((OwWorld *)0x02A7)

/* Load a program from unit 8 over whatever is running and start it at
   $080D, from a loader copied into the tape buffer first. The screen and
   the interrupts must be the KERNAL's again before it is called. Does not
   return; the border flashes if the load fails. */
void __fastcall__ ow_run(const char *name);

/* 0 no REU, 1 one whose transfers reach colour RAM, 2 one whose do not:
   OW_NO_REU, OW_REU_OK, OW_REU_BLIND. */
unsigned char ow_reu_detect(void);

#endif /* Q4K_OW_H */
