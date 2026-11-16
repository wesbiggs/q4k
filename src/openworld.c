/*
 * Open world: the bootstrap of the smooth-scrolling prototype.
 *
 * Run from its disk (dist/openworld.d64, the first file). It shows the
 * splash screen, the picture for the machine's video system, until F1 (a
 * new game) or F3 (a saved one); looks for an REU; loads what the two
 * engines share (OW-DATA) and the music (a SID file, to under the KERNAL);
 * builds Keldmarch with the game's generator into the RAM under the I/O
 * area, and each of the four continents to see where its places are (an
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

/* What the land generator and the vault read and write of game.c's, which
   is not linked: defined here. */
unsigned char lore_seed, land, local_w, local_h, quest_stage;
unsigned char home_fx[NUM_HOME_PLACES], home_fy[NUM_HOME_PLACES];

/* The continents' cells by compass point, and a continent's first row in
   its cell (src/ow/engine.c has the world's map). */
static const unsigned char cell_x[4] = { 2, 4, 2, 0 };
static const unsigned char cell_y[4] = { 0, 1, 2, 1 };
#define ISLE_TOP 16
#define ISLE_ROWS 31

/* Which continent holds which story place: a shuffle of the seed, as
   game.c's slot_of() has it. */
unsigned char slot_of(unsigned char d)
{
    unsigned char p[4], i, j, t, v = lore_seed;

    for (i = 0; i < 4; ++i) {
        p[i] = i;
    }
    for (i = 3; i > 0; --i) {
        v = v * 5 + 3;
        j = (v >> 5) % (i + 1);
        t = p[i]; p[i] = p[j]; p[j] = t;
    }
    return p[d];
}

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
        if (t == T_WATER) {
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
    while (!cbm_load(name, 8, at)) {
        cputs("Put the disk back and press a key.\r\n");
        cgetc();
    }
}

/* The splash screen and what the player chose there: the seed, and whether
   it is a saved game, with `save` loaded if so. The screen is black until
   the picture is ready, and from the key on it shows the disk. */
/* The disk icon (src/lib/ow_disk.s): its code is in the bootstrap's OWDISK
   segment, which runs at $CF14, where no program loaded later reaches. It is
   copied there before the icon is first shown, and not before the splash:
   the picture is unpacked into $C000-$FFFF, which is where the code is. */
extern unsigned char _OWDISK_LOAD__[], _OWDISK_SIZE__[];

static void disk_resident(void)
{
    memcpy((void *)0xCF14, _OWDISK_LOAD__, (unsigned)_OWDISK_SIZE__);
}

/* A machine with no picture, or a disk without one (the NTSC picture is
   left off the disk for room: the Makefile's OW_NTSC_SPLASH), is asked in
   words instead. The tests' hook works here too. */
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

static void splash(void)
{
    unsigned char key = 0;

    OW->video = ow_video();
    if (OW->video <= OW_NTSC &&
        cbm_load(OW->video == OW_NTSC ? "splash-n" : "splash-p", 8,
                 (void *)OW_SPLASH_AT)) {
        key = ow_splash();
    } else {
        key = title_text();
    }
    disk_resident();
    ow_disk_on();
    /* A new world for a new game, from how long the player took, and a
       party to make for it (src/owparty.c), unless it is the tests' quick
       start. */
    lore_seed = 1 + ow_splash_ticks;
    OW->saved = 0;
    G->magic = 0;
    OWB->mode = key == OW_KEY_QUICK ? OWB_QUICK : OWB_NEW;
    if (key == OW_KEY_F3) {
        /* The saved game is the game block, loaded where it lives. */
        if (cbm_load("ow-save", 8, 0) && G->magic == OWG_MAGIC) {
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
    unsigned char d, again = OW->magic == OW_MAGIC;

    bgcolor(COLOR_BLACK);
    bordercolor(COLOR_BLACK);
    textcolor(COLOR_GRAY3);     /* for the one thing that may be said */
    clrscr();
    /* MEMSIZ up to the probe page before the drive is touched, as the
       game has it. */
    POKEW(0x0283, 0xCF00);

    if (again && OWB->mode == OWB_LOAD) {
        /* F3 in an engine: the saved game's world, if there is one. */
        OWB->mode = OWB_NONE;
        cbm_load("ow-save", 8, 0);     /* none: the game goes on as it was */
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
    OW->seed = lore_seed;
    OW->reu = ow_reu_detect();
    if (!again) {               /* else they are still there */
        need("ow-data", 0);     /* to its own address, $7400 */
        /* The music a game starts with, to where the game keeps a tune:
           $E7C0, under the KERNAL. Not worth stopping for if it is not on
           the disk; an engine loads any other itself. */
        d = tune_file[MUSIC_HOME];
        OW->tune = cbm_load(tune_name[d], 8, 0) ? d : OW_NO_TUNE;
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
        moor(4 + d, isle_dock_x, isle_dock_y, cell_x[d] * 64u,
             cell_y[d] * 64 + ISLE_TOP, 0);
    }
    isle_build(ISLE_HOME);      /* and home, in place */
    OW->home_biome = isle_biome;
    memcpy(OW->home_fx, isle_fx, OW_PLACES);
    memcpy(OW->home_fy, isle_fy, OW_PLACES);
    for (d = 0; d < 4; ++d) {
        moor(d, isle_port_x[d], isle_port_y[d], 128, 64, 1);
    }
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
        ow_run("owtown");
    }
    ow_run(OW->reu == OW_REU_OK ? "ow-reu" : "ow-cpu");
    return 0;
}
