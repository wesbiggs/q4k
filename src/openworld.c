/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Open world: the bootstrap of the smooth-scrolling prototype.
 *
 * Run from its disk (dist/openworld.d81, the first file). It shows the
 * splash screen, the picture for the machine's video system, until F1 (a
 * new game), SHIFT+F1 (one from a seed typed in) or F3 (a saved one, if
 * the disk has one); looks for an REU; loads what the two
 * engines share (OW-DATA) and the music (a SID file, to under the KERNAL);
 * builds Keldmarch with the game's generator into the RAM under the I/O
 * area and lays its roads (src/lib/ow_roads.s), and each of the four continents to see where its places are (an
 * engine builds the one it needs again: there is room to keep one, beside
 * the music); moors a ship beside
 * each port and landing; leaves all of that in the hand-off block
 * (include/q4k/ow.h) and loads the engine that suits the machine: OW-REU if
 * there is an REU whose transfers reach colour RAM, OW-CPU otherwise. The
 * engines are src/ow/engine.c, built twice; see there for the keys.
 *
 * The order is the memory's. The picture is unpacked into $C000-$FFFF and
 * shown with the KERNAL out, so nothing loads while it is up, and the land
 * and the music go into that same RAM after it; the picture's file is where
 * OW-DATA goes, so that is loaded after it too.
 *
 * An engine comes back here for N, with the next seed in the block: no
 * splash then, OW-DATA and the music are still in memory, and this builds
 * that world.
 */

#include <cbm.h>
#include <conio.h>
#include <peekpoke.h>
#include <string.h>

#include <q4k/game.h>
#include <q4k/isle.h>
#include <q4k/music_map.h>
#include <q4k/ow.h>
#include <q4k/owgame.h>
#include <q4k/tiles.h>
#include <q4k/vault.h>
#include <q4k/isles.h>

/* What the land generator and the vault read and write of game.c's, which
   is not linked: defined here. */
unsigned char lore_seed, land, local_w, local_h, quest_stage;
unsigned char home_fx[NUM_HOME_PLACES], home_fy[NUM_HOME_PLACES];

/* A continent's first row in its cell, and its rows (src/ow/engine.c has
   the world's map). */
#define ISLE_TOP 16
#define ISLE_ROWS 31

/* Ship i beside landing (x, y): on the first square of water next to it,
   north, east, south or west. The land is home in the vault, or a
   continent still in local[]; (ox, oy) is where it starts in the world. */
static void moor(unsigned char i, unsigned char x, unsigned char y,
                 unsigned int ox, unsigned char oy, unsigned char home)
{
    static const signed char mx[4] = { 0, 1, 0, -1 };
    static const signed char my[4] = { -1, 0, 1, 0 };
    unsigned char d, nx, ny, t;

    for (d = 0; d < 4; ++d) {
        nx = x + mx[d];
        ny = y + my[d];
        if (nx >= 64 || ny >= (home ? 64 : ISLE_ROWS)) {
            continue;
        }
        t = home ? vault_tile(nx, ny) : local[LOCAL_AT(nx, ny)];
        if (t == T_WATER || t == T_DEEP) {
            break;
        }
    }
    OW->ship_x[i] = ox + nx;
    OW->ship_y[i] = oy + ny;
}

/* ow_splash.s finds OW->hook by its offset: seven bytes from the end
   (cc65's offsetof is not a constant, so this checks the size instead). */
typedef char hook_is_where_ow_splash_has_it
    [2 * (sizeof(OwWorld) == OW_HOOK_OFF + 7) - 1];

/* A file that has to be there. Nothing is said unless it is not. */
static void need(const char *name, void *at)
{
    while (!ow_cload(name, at)) {
        cputs("Put the disk back and press a key.\r\n");
        cgetc();
    }
}

/* The saved game: OW-SAVE, the game block and, after it in a save from
   an engine since the land's fog came, what the party had seen of the land
   (owgame.h's OWG_WFOG). Loaded where nothing is yet (the roads' grid comes
   later), the block put where it lives; the land's bits wait there for
   main() to put back under the KERNAL once it has cleared the old ones
   (wf_back). Nonzero if there was a save. */
#define SAVE_AT ((unsigned char *)0x7000)
static unsigned char wf_back;
static unsigned char isl_back;  /* and the islands' places (q4k/isles.h's
                                   ISL_STATE), after the bits: none in a
                                   save from before them */

static unsigned char load_save(void)
{
    unsigned int n = cbm_load("ow-save", 8, SAVE_AT);

    if (n < OWG_SIZE) {
        return 0;
    }
    memcpy(G, SAVE_AT, OWG_SIZE);
    wf_back = n >= OWG_SIZE + OWG_WFOG_SIZE;
    isl_back = n >= OWG_SIZE + OWG_WFOG_SIZE + ISL_CELLS;
    return 0x80;                /* (the splash's text's offset: ow_splash.s) */
}

/* The splash screen and what the player chose there: the seed, and whether
   it is a saved game, with `save` loaded if so. The screen is black until
   the picture is ready, and from the key on it shows the die, rolling. */
/* The loading icon (src/lib/ow_disk.s): its code is in the bootstrap's
   OWDISK segment, which runs at $CF14, where no program loaded later
   reaches. It is copied there before the icon is first shown, and not
   before the splash: the picture is unpacked into $C000-$FFFF, which is
   where the code is. As copied, it is the die, rolling, which it stays
   through the bootstrap's few loads and its building until the program
   after it has loaded; then the loader puts it back to the disk. */
extern unsigned char _OWDISK_LOAD__[], _OWDISK_SIZE__[];

static void disk_resident(void)
{
    memcpy((void *)0xCF14, _OWDISK_LOAD__, (unsigned)_OWDISK_SIZE__);
}

/* A machine with no picture (neither PAL nor NTSC), or a disk without one,
   is asked in words instead. The tests' hook works here too. */
static unsigned char title_text(void)
{
    unsigned char k = 0;

    textcolor(COLOR_YELLOW);
    cputsxy(11, 10, "Quest for the King");
    textcolor(COLOR_GRAY3);
    cputsxy(6, 13, "F1 a new game   F3 the saved one");
    OW->hook = OW_HOOK_WAIT;
    while (k != OW_KEY_F1 && k != OW_KEY_F3) {
        ++ow_splash_ticks;
        if (OW->hook != OW_HOOK_WAIT) {
            k = OW->hook;           /* (OW_KEY_QUICK too) */
            break;
        }
        if (kbhit()) {
            k = cgetc();
        }
    }
    OW->hook = 0;
    clrscr();
    return k;
}

/* A save of this version, or of the two before: the one before the
   treasure maps (OWG_MAGIC_V2), which is the block without them, and the
   one before the party's pronouns, the same with them all they/them (0,
   owgame.h's OWG_MAGIC_V1): made this version, with no maps. (A save is
   loaded at the block's address, not the file's, which was 8 bytes higher
   before the maps.) */
static unsigned char save_version(void)
{
    if (G->magic == OWG_MAGIC_V1 || G->magic == OWG_MAGIC_V2) {
        G->magic = OWG_MAGIC;
        memset(G->tmap, 0, sizeof G->tmap);
    }
    return G->magic == OWG_MAGIC;
}

/* The splash's key, which main() reads too: F2 runs owseed before
   owparty. 0 when the bootstrap is run again (BSS), as N or owseed do. */
static unsigned char key;
static const char run_names[] = "owparty\0owseed";

static void splash(void)
{
    /* Whether there is a saved game, which the splash offers (F3) only if
       so: read now and again for F3, the picture being loaded over it. */
    ow_saved = load_save();
    OW->video = ow_video();
    if (OW->video <= OW_NTSC &&
        cbm_load(OW->video == OW_NTSC ? "splash-n" : "splash-p", 8,
                 (void *)OW_SPLASH_AT)) {
        key = ow_splash();
    } else {
        key = title_text();
    }
    /* A new world for a new game, from how long the player took, and a
       party to make for it (src/owparty.c), unless it is the tests' quick
       start. */
    lore_seed = 1 + ow_splash_ticks;
    disk_resident();
    /* The loading icon's pictures, the disk's and the die's (q4k/ow.h's
       OW_ICON_ART), under the KERNAL, where they stay for the whole game:
       once, before the icon is first shown, since the splash had its
       picture there. */
    need("ow-icon", 0);
    ow_disk_on();
    OW->saved = 0;
    G->magic = 0;
    wf_back = 0;                /* (as the save at the splash left it) */
    OWB->mode = key == OW_KEY_QUICK ? OWB_QUICK : OWB_NEW;
    if (key == OW_KEY_F3) {
        /* The saved game is the game block, loaded where it lives. */
        if (load_save() && save_version()) {
            lore_seed = G->seed;
            OW->saved = 1;
            OWB->mode = OWB_NONE;
        } else {
            G->magic = 0;
        }
    }
}

int main(void)
{
    unsigned char d, k = 0, best, dist, again = OW->magic == OW_MAGIC;

    bgcolor(COLOR_BLACK);
    bordercolor(COLOR_BLACK);
    textcolor(COLOR_GRAY3);     /* for the one thing that may be said */
    clrscr();
    if (!again) {
        OW_TESTCPU = 0;         /* (the tests' --cpu-engine sets it later) */
    }
    /* MEMSIZ up to the probe page before the drive is touched, as the
       game has it. */
    POKEW(0x0283, 0xCF00);

    if (again && OWB->mode == OWB_LOAD) {
        /* F3 in an engine: the saved game's world, if there is one. */
        OWB->mode = OWB_NONE;
        load_save();            /* none: the game goes on as it was */
        save_version();
        lore_seed = G->seed;
        OW->saved = 1;
        disk_resident();
        ow_disk_on();
    } else if (again) {
        OWB->mode = OWB_NONE;
        lore_seed = OW->seed;   /* N in an engine: the next world */
        OW->saved = 0;
        disk_resident();
        ow_disk_on();
    } else {
        splash();
    }
    OW->magic = 0;              /* until it is all there */
    /* No dungeon seen yet (owgame.h's OWG_FOG): the splash's picture was
       there, or another game's. A write under the KERNAL reaches the RAM. */
    memset((void *)OWG_FOG, 0, OWG_FOG_SIZE);
    memset((void *)OWG_WFOG, 0, OWG_WFOG_SIZE);    /* nor any land ... */
    memset((void *)ISL_STATE, 0, ISL_CELLS);        /* ... nor island */
    if (wf_back) {              /* ... but what a save says was seen */
        memcpy((void *)OWG_WFOG, SAVE_AT + OWG_SIZE, OWG_WFOG_SIZE);
    }
    if (isl_back) {             /* (and where the party has been) */
        memcpy((void *)ISL_STATE, SAVE_AT + OWG_SIZE + OWG_WFOG_SIZE,
               ISL_CELLS);
    }
    OW->seed = lore_seed;
    OW->reu = ow_reu_detect();
    if (OW->reu && OW_TESTCPU == OW_TESTCPU_ON) {
        OW->reu = OW_REU_BLIND; /* (tests: OW-CPU, and the cache) */
    }
    ow_cinit();                 /* its load cache, kept or laid out anew */
    if (!again) {               /* else they are still there */
        need("ow-data", 0);     /* to its own address, $8800 */
        need("ow-isles", 0);     /* under the KERNAL, ISL_CODE (isles.h) */
        /* The music a game starts with, to where the game keeps a tune:
           $E7C0, under the KERNAL. Not worth stopping for if it is not on
           the disk; an engine loads any other itself. */
        d = tune_file[MUSIC_HOME];
        OW->tune = ow_cload(tune_name[d], 0) ? d : OW_NO_TUNE;
    }

    /* Each continent, for where its places and its landing are. It is not
       kept: an engine builds the one it needs (ow_gen.s). */
    for (d = 0; d < 4; ++d) {
        OW->slot[d] = slot_of(d);
        isle_story = OW->slot[d] & 1;
        isle_build(d);
        OW->ipx[d][0] = isle_fx[0];
        OW->ipy[d][0] = isle_fy[0];
        OW->ipx[d][1] = isle_fx[1];
        OW->ipy[d][1] = isle_fy[1];
        OW->ibiome[d] = isle_biome;
    }
    isle_build(ISLE_HOME);      /* and home, in place, with its roads */
    ow_roads();
    /* The open sea's small islands, under the KERNAL (q4k/isles.h), where
       ow-isl, which the engine draws them with, was loaded beside them. */
    isles_make();
    OW->home_biome = isle_biome;
    memcpy(OW->home_fx, isle_fx, OW_PLACES);
    memcpy(OW->home_fy, isle_fy, OW_PLACES);
    /* One ship, the king's, at the port nearest the castle (home's second
       place): the continents' docks have none. */
    best = 0xFF;
    for (d = 0; d < 4; ++d) {
        dist = (isle_port_x[d] > isle_fx[1] ? isle_port_x[d] - isle_fx[1]
                                             : isle_fx[1] - isle_port_x[d])
             + (isle_port_y[d] > isle_fy[1] ? isle_port_y[d] - isle_fy[1]
                                             : isle_fy[1] - isle_port_y[d]);
        if (dist < best) {
            best = dist;
            k = d;
        }
    }
    moor(0, isle_port_x[k], isle_port_y[k], 128, 64, 1);
    /* A new game starts at Millford, as does the next world (N, which
       keeps the party): a save where it was saved. */
    if (!OW->saved) {
        G->seed  = lore_seed;
        G->area  = AREA_HOME;
        G->land  = ISLE_HOME;
        G->wx    = 128 + isle_fx[0];
        G->wy    = 64 + isle_fy[0];
        G->x     = OWG_GATE;
        G->boat  = 0;
        G->ships = 0;           /* where the bootstrap moored them */
    }
    OW->saved = 0;              /* (N comes back here) */
    OW->magic = OW_MAGIC;

    /* The party is made first, unless the game has one. */
    if (G->magic != OWG_MAGIC) {
        if (OWB->mode != OWB_QUICK) {
            OWB->mode = OWB_NEW;
        }
        /* F2: owseed, which asks for a seed first. Of the keys only F2
           ($89) has bit 3, and 8 is how far the second name is. In
           assembly, as cc65's sum is longer and the bootstrap is full. */
        __asm__("lda %v", key);
        __asm__("and #$08");
        __asm__("clc");
        __asm__("adc #<%v", run_names);
        __asm__("ldx #>%v", run_names);
        __asm__("bcc %g", sum);
        __asm__("inx");
sum:
        __asm__("jsr %v", ow_run);
    }
    ow_run(OW->reu == OW_REU_OK ? "ow-reu" : "ow-cpu");
    return 0;
}
