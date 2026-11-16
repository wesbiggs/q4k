#include <conio.h>
#include <c64.h>

#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/charset.h>
#include <q4k/dialog.h>
#include <q4k/game.h>
#include <q4k/probe.h>
#include <q4k/savegame.h>
#include <q4k/tiles.h>
#include <q4k/title.h>
#include <q4k/version.h>
#include <q4k/view.h>

#define STORY_Y   5
#define ROSTER_Y 13   /* who has answered so far */
#define ASK_Y    17
#define ENTRY_Y  18
#define HINT_Y   21
#define LOAD_Y   22
#define LINE_Y   24   /* what the modem check found, left of the version */
#define LINE_W   30
#define ERROR_Y  24

#define KEY_RETURN 0x0D
#define KEY_DEL    0x14
#define KEY_F3     0x86
#define KEY_F5     0x87

static const char *const story[] = {
    "The dragon Maldrath came over the",
    "mountains three winters ago, and has",
    "not left. Two armies went against him.",
    "Neither came back.",
    "",
    "Letters go out under the king's seal",
    "to strangers who have never met: meet",
    "in Millford, then come to the castle.",
};
#define STORY_LINES (sizeof(story) / sizeof(story[0]))

/* For a player who just presses RETURN. At most 8 letters (NAME_LEN), and
   none that echo a townsperson's name. */
static const char *const names[] = {
    "Aldwin",  "Bramwell", "Caelan",  "Doran",   "Elowen",
    "Faelan",  "Gareth",   "Halvard", "Isolde",  "Jorund",
    "Kestrel", "Lirael",   "Maelis",  "Neriah",  "Orrin",
    "Perrin",  "Quenby",   "Rowena",  "Sigrun",  "Talwyn",
    "Ulric",   "Vesper",   "Wystan",  "Zephyr",
};
#define NUM_NAMES (sizeof(names) / sizeof(names[0]))

/* A line of grey or red text at the foot of the screen, replacing the last. */
static void say(unsigned char y, unsigned char color, const char *s)
{
    textcolor(color);
    cclearxy(1, y, LINE_W);
    cputsxy(1, y, s);
}

static void ask(const char *s)
{
    textcolor(COLOR_YELLOW);
    /* The whole row: "1 Warrior, 2 Archer or 3 Wizard?" is longer than
       LINE_W, and what it leaves past that edge stays on screen. */
    cclearxy(1, ASK_Y, SCR_W - 2);
    cputsxy(1, ASK_Y, s);
}

static const char *const ordinal[MAX_MADE] = {
    "Who answers the letter?", "And who else?", "And the third?",
};

/* Typed into a buffer of its own. */
static char entry[NAME_LEN];
static unsigned char seeding;

/* A key, timed: how long the player takes is what makes the random name
   random, as it is in the game's own key loop. */
static unsigned char timed_key(void)
{
    unsigned int spins = 0;

    while (!kbhit()) {
        ++spins;
    }
    rng_stir(spins);
    return cgetc();
}

/* Read a name into entry[]. Returns 0 when one is typed or picked, 1 when F3
   restored the saved game. F5 asks for the world's seed in between. */
static unsigned char read_name(unsigned char made)
{
    unsigned char i, n = 0, key;
    unsigned int v;

    ask(ordinal[made]);
    for (;;) {
        textcolor(COLOR_WHITE);
        cclearxy(1, ENTRY_Y, SCR_W - 2);
        entry[n] = '\0';
        cputsxy(1, ENTRY_Y, entry);
        /* Block cursor, written as a raw screen code: cputc would treat
           $A0 as PETSCII and convert it to something else. */
        SCREEN_RAM[(unsigned int)ENTRY_Y * SCR_W + 1 + n] = 0xA0;
        COLOR_RAM[(unsigned int)ENTRY_Y * SCR_W + 1 + n]  = COLOR_GRAY2;

        key = timed_key();

        if (key == KEY_RETURN && seeding) {
            /* The world's seed, 1 to 255: what was typed, as a number. */
            for (v = i = 0; i < n; ++i) {
                v = (v + v) + (v << 3) + entry[i] - 0x30;
            }
            lore_seed = v > 255 ? 255 : (unsigned char)v;
            seeding = n = 0;
            ask(ordinal[made]);
            continue;
        }
        if (key == KEY_F5 && !seeding) {
            seeding = 1;
            n = 0;
            ask("World seed, 1-255?");
            continue;
        }
        if (key == KEY_RETURN) {
            if (!n) {
                /* Somebody nobody else in the party is called. */
                reseed();
                i = (unsigned char)(rand() % NUM_NAMES);
                for (n = 0; n < made; ) {
                    if (strcmp(party[n].name, names[i])) {
                        ++n;
                    } else {
                        i = (i + 1) % NUM_NAMES;
                        n = 0;
                    }
                }
                strcpy(entry, names[i]);
            }
            return 0;
        }
        if (key == KEY_F3) {
            say(ERROR_Y, COLOR_GRAY2, "Loading...");
            switch (load_game()) {
                case SAVE_OK:
                    return 1;
                case SAVE_EFORMAT:
                    say(ERROR_Y, COLOR_LIGHTRED, "That is not a saved game.");
                    break;
                default:
                    say(ERROR_Y, COLOR_LIGHTRED, "No saved game found.");
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
        if (seeding && (key < 0x30 || key > 0x39)) {
            continue;
        }
        if (key < 0x20 || (key > 0x3F && key < 0x41)
            || (key > 0x5A && key < 0xC1) || key > 0xDA) {
            continue;               /* not something a name is made of */
        }

        if (n < NAME_LEN - 1) {
            entry[n++] = (char)key;
        }
    }
}

unsigned char title_screen(void)
{
    unsigned char i, made, key;

    /* The game's own character set, even though this screen is all text:
       it is the ROM font with tiles patched over codes the text never uses,
       so it looks identical, and it keeps the whole game on one charset --
       which among other things lets the harness screenshot this screen. */
    tiles_install();
    VIC.bordercolor = COLOR_BLACK;
    VIC.bgcolor0    = COLOR_BLACK;
    VIC.addr        = CHARSET_D018;
    /* The KERNAL's keyboard scan flips bit 1 of $D018 on C=+Shift, which
       here points the VIC at $3000, not another font. $80 in MODE locks it
       (what CHR$(8) does), and the reset on quit clears it again. */
    *(unsigned char *)0x0291 = 0x80;

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
    textcolor(COLOR_GRAY2);
    cputsxy(1, HINT_Y, "RETURN alone for a random name");
    cputsxy(1, LOAD_Y, "F3 to load a game, F5 to set the seed");
    textcolor(COLOR_GRAY1);
    /* Right-aligned, a column in from the edge: sizeof counts the NUL. */
    cputsxy(SCR_W - sizeof("v" Q4K_VERSION), SCR_H - 1, "v" Q4K_VERSION);

#ifdef Q4K_ONLINE
    /* Whether anyone will talk, asked once for the whole game. Anything
       short of a host with a model and the game plays without
       conversations, so say which it is here rather than at every door.
       Kept short of the version in the other corner. */
    cputsxy(1, LINE_Y, "Calling the host...");
    switch (dialog_probe()) {
        case LINE_UP:
            cclearxy(1, LINE_Y, LINE_W);
            break;
        case LINE_NOMODEL:
            say(LINE_Y, COLOR_LIGHTRED, "The host has no model.");
            break;
        default:
            say(LINE_Y, COLOR_LIGHTRED, "No host on the modem.");
            break;
    }
#endif

    /* The launcher is still typing RUN when we get here. */
    while (kbhit()) {
        cgetc();
    }

    /* Stage 1 means "drawn, and waiting for a key" -- the same thing the
       main loop publishes on its first turn, so a headless run can wait for
       it here instead of hanging on a screen that never reports in. */
    probe_stage(1);

    /* Up to three, each a name and a class. */
    for (made = 0; made < MAX_MADE; ) {
        if (read_name(made)) {
            return 1;
        }
        cclearxy(1, ENTRY_Y, SCR_W - 2);
        ask("1 Warrior, 2 Archer or 3 Wizard?");
        do {
            key = timed_key() - 0x31;
        } while (key >= NUM_CLASSES);
        party_make(made, entry, key);

        textcolor(COLOR_WHITE);
        cputsxy(1, ROSTER_Y + made, entry);
        textcolor(COLOR_GRAY3);
        cputsxy(11, ROSTER_Y + made, class_names[key]);
        ++made;

        if (made < MAX_MADE) {
            ask("Will another answer? (Y/N)");
            key = timed_key();
            if (key != 0x59 && key != 0xD9) {      /* Y, either case */
                break;
            }
        }
    }
    return 0;
}
