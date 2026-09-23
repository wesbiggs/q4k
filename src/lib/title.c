#include <conio.h>
#include <c64.h>

#include <stdlib.h>
#include <string.h>

#include <q4k/battle.h>
#include <q4k/charset.h>
#include <q4k/probe.h>
#include <q4k/savegame.h>
#include <q4k/tiles.h>
#include <q4k/title.h>
#include <q4k/view.h>

#define STORY_Y   5
#define ASK_Y    17
#define ENTRY_Y  18
#define HINT_Y   21
#define LOAD_Y   22
#define ERROR_Y  24

#define KEY_RETURN 0x0D
#define KEY_DEL    0x14
#define KEY_F3     0x86

static const char *const story[] = {
    "The dragon Maldrath came over the",
    "Sundering three winters ago, and has",
    "not left. Two armies went against him.",
    "Neither came back.",
    "",
    "A letter comes, under the king's seal.",
    "Matheson summons you to the castle.",
    "",
    "The blade that can banish him was",
    "broken, and its three binding stones",
    "lie sealed in three deep places.",
};
#define STORY_LINES (sizeof(story) / sizeof(story[0]))

/* For a player who just presses RETURN. At most 8 letters (NAME_LEN), and
   none that echo a townsperson's name. */
static const char *const names[] = {
    "Aldwin",  "Bramwell", "Caelan",  "Doran",   "Elowen",
    "Faelan",  "Gareth",   "Halvard", "Isolde",  "Jorund",
    "Kestrel", "Lirael",   "Maelis",  "Neriah",  "Orrin",
    "Perrin",  "Quenby",   "Rowena",  "Sigrun",  "Talwyn",
    "Ulric",   "Vesper",   "Wystan",  "Ysolde",  "Zephyr",
    "Aveline", "Briony",   "Cedric",  "Dagny",   "Edric",
    "Fenwick", "Gwyneth",  "Hollis",  "Ingrid",  "Leofric",
    "Merrow",  "Niamh",    "Osric",   "Rhoswen", "Seren",
    "Thorne",  "Tristan",  "Wendel",  "Alaric",  "Beren",
    "Corwin",  "Eira",     "Hadric",  "Idris",   "Morgana",
};
#define NUM_NAMES (sizeof(names) / sizeof(names[0]))

unsigned char title_screen(char *name, unsigned char max)
{
    /* Typed into a buffer of its own. Terminating `name` as it is drawn
       would erase the caller's default before the player touched a key. */
    static char entry[17];
    unsigned char i, n = 0, key;
    unsigned int spins;

    if (max > sizeof(entry)) {
        max = sizeof(entry);
    }

    /* The game's own character set, even though this screen is all text:
       it is the ROM font with tiles patched over codes the text never uses,
       so it looks identical, and it keeps the whole game on one charset --
       which among other things lets the harness screenshot this screen. */
    tiles_install();
    VIC.bordercolor = COLOR_BLACK;
    VIC.bgcolor0    = COLOR_BLACK;
    VIC.addr        = CHARSET_D018;

    clrscr();

    textcolor(COLOR_YELLOW);
    cputsxy(11, 1, "Quest for the King");
    textcolor(COLOR_GRAY1);
    chlinexy(0, 3, SCR_W);

    textcolor(COLOR_LIGHTGREEN);
    for (i = 0; i < STORY_LINES; ++i) {
        cputsxy(1, STORY_Y + i, story[i]);
    }

    textcolor(COLOR_GRAY1);
    chlinexy(0, ASK_Y - 1, SCR_W);
    textcolor(COLOR_YELLOW);
    cputsxy(1, ASK_Y, "And who are you, traveller?");
    textcolor(COLOR_GRAY2);
    cputsxy(1, HINT_Y, "RETURN alone for a random name");
    cputsxy(1, LOAD_Y, "F3 to load a saved game");

    /* The launcher is still typing RUN when we get here. */
    while (kbhit()) {
        cgetc();
    }

    /* Stage 1 means "drawn, and waiting for a key" -- the same thing the
       main loop publishes on its first turn, so a headless run can wait for
       it here instead of hanging on a screen that never reports in. */
    probe_stage(1);

    for (;;) {
        textcolor(COLOR_WHITE);
        cclearxy(1, ENTRY_Y, SCR_W - 2);
        entry[n] = '\0';
        cputsxy(1, ENTRY_Y, entry);
        /* Block cursor, written as a raw screen code: cputc would treat
           $A0 as PETSCII and convert it to something else. */
        SCREEN_RAM[(unsigned int)ENTRY_Y * SCR_W + 1 + n] = 0xA0;
        COLOR_RAM[(unsigned int)ENTRY_Y * SCR_W + 1 + n]  = COLOR_GRAY2;

        /* Time the wait, as the game's own key loop does: how long the
           player takes is what makes the random name random. */
        spins = 0;
        while (!kbhit()) {
            ++spins;
        }
        rng_stir(spins);
        key = cgetc();

        if (key == KEY_RETURN) {
            if (n) {
                entry[n] = '\0';
                strcpy(name, entry);
            } else {
                reseed();
                strcpy(name, names[rand() % NUM_NAMES]);
            }
            return 0;
        }
        if (key == KEY_F3) {
            textcolor(COLOR_GRAY2);
            cclearxy(1, ERROR_Y, SCR_W - 2);
            cputsxy(1, ERROR_Y, "Loading...");
            switch (load_game()) {
                case SAVE_OK:
                    return 1;
                case SAVE_EFORMAT:
                    textcolor(COLOR_LIGHTRED);
                    cclearxy(1, ERROR_Y, SCR_W - 2);
                    cputsxy(1, ERROR_Y, "That is not a saved game.");
                    break;
                default:
                    textcolor(COLOR_LIGHTRED);
                    cclearxy(1, ERROR_Y, SCR_W - 2);
                    cputsxy(1, ERROR_Y, "No saved game found.");
                    break;
            }
            continue;
        }
        if (key == KEY_DEL) {
            if (n) {
                --n;
            }
            continue;
        }

        /* PETSCII as typed: unshifted letters arrive as $41-$5A (lower
           case in this character set), shifted ones as $C1-$DA. */
        if (key < 0x20 || (key > 0x3F && key < 0x41)
            || (key > 0x5A && key < 0xC1) || key > 0xDA) {
            continue;               /* not something a name is made of */
        }

        if (n < max - 1) {
            entry[n++] = (char)key;
        }
    }
}
