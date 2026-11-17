/*
 * Quest for the King -- the open world's party, made.
 *
 * F1 on the splash screen is a new game: the bootstrap builds the world and
 * then loads OWPARTY (src/owparty.c) over itself with OWB_NEW, which calls
 * this. It tells the story and asks who answered the king's letter, as the
 * game's title screen does -- up to three, each a name and a class -- and
 * puts the new game in the game block (q4k/owgame.h), at Millford's gate;
 * the engine is loaded over it after, so nothing of this stays in memory.
 *
 * OWB_QUICK, the tests' start (screendump.py --splash quick), makes a lone
 * warrior called Alaric without asking, with the keys to every dungeon and
 * the three stones (so every ship sails), on the overworld outside Millford:
 * where the prototype always began, with nothing shut.
 */

#include <c64.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/charset.h>
#include <q4k/game.h>
#include <q4k/ow.h>
#include <q4k/owgame.h>
#include <q4k/probe.h>
#include <q4k/tiles.h>
#include <q4k/town_data.h>

#define SCR_W     40
#define SCREEN_RAM ((unsigned char *)0x0400)
#define STORY_Y   5
#define ROSTER_Y 13
#define ASK_Y    17
#define ENTRY_Y  18
#define HINT_Y   21

#define KEY_RETURN 0x0D
#define KEY_DEL    0x14

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

/* For a player who just presses RETURN: the title screen's. */
static const char *const names[] = {
    "Aldwin",  "Bramwell", "Caelan",  "Doran",   "Elowen",
    "Faelan",  "Gareth",   "Halvard", "Isolde",  "Jorund",
    "Kestrel", "Lirael",   "Maelis",  "Neriah",  "Orrin",
    "Perrin",  "Quenby",   "Rowena",  "Sigrun",  "Talwyn",
    "Ulric",   "Vesper",   "Wystan",  "Zephyr",
};
#define NUM_NAMES (sizeof(names) / sizeof(names[0]))

static const char *const ordinal[MAX_MADE] = {
    "Who answers the letter?", "And who else?", "And the third?",
};

static char entry[NAME_LEN];

static void ask(const char *s)
{
    textcolor(COLOR_YELLOW);
    cclearxy(1, ASK_Y, SCR_W - 2);
    cputsxy(1, ASK_Y, s);
}

/* A key, and the time the player took, stirred into rand(). */
static unsigned int spins;

static unsigned char timed_key(void)
{
    while (!kbhit()) {
        ++spins;
    }
    srand(spins ^ (CIA1.ta_lo | (CIA1.ta_hi << 8)));
    return cgetc();
}

/* A name into entry[]: typed, or one nobody in the party has. */
static void read_name(unsigned char made)
{
    unsigned char i, n = 0, key;

    ask(ordinal[made]);
    for (;;) {
        textcolor(COLOR_WHITE);
        cclearxy(1, ENTRY_Y, SCR_W - 2);
        entry[n] = '\0';
        cputsxy(1, ENTRY_Y, entry);
        SCREEN_RAM[(unsigned int)ENTRY_Y * SCR_W + 1 + n] = 0xA0;
        COLOR_RAM[(unsigned int)ENTRY_Y * SCR_W + 1 + n]  = COLOR_GRAY2;

        key = timed_key();
        if (key == KEY_RETURN) {
            if (!n) {
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
            return;
        }
        if (key == KEY_DEL) {
            if (n) {
                --n;
            }
            continue;
        }
        /* PETSCII as typed: $41-$5A unshifted, $C1-$DA shifted. */
        if (key < 0x20 || (key > 0x3F && key < 0x41)
            || (key > 0x5A && key < 0xC1) || key > 0xDA) {
            continue;
        }
        if (n < NAME_LEN - 1) {
            entry[n++] = (char)key;
        }
    }
}

static void make_party(void)
{
    unsigned char i, made, key;

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
    VIC.ctrl1 |= 0x10;          /* the screen on: the loader left it off */
    while (kbhit()) {
        cgetc();
    }
    probe_stage(1);

    party_count = 0;
    for (made = 0; made < MAX_MADE; ) {
        read_name(made);
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
    ask("Then to Millford...");
}

/* A member's colour from their name, as the game's sprite.c's tunic_of():
   the same name always gets the same one, never black or white
   (tools/mkowsave.py has it too). The party screen changes it. */
static unsigned char tunic(const char *name)
{
    unsigned char h = 0;

    while (*name) {
        h = (unsigned char)((h << 1) | (h >> 7)) ^ *name++;
    }
    return (unsigned char)(2 + h % 14);
}

void ow_new_game(unsigned char quick)
{
    unsigned char i;

    /* The game as game.c's game_init() starts it. */
    lore_seed   = G->seed;
    gold        = 9;
    arrows      = 0;
    items       = 0;
    quest_stage = Q_START;
    game_turn   = 0;
    memset(looted, 0, sizeof looted);
    party_init();               /* Alaric, a warrior, alone */
    if (quick) {
        /* Everything open but the last of it: every dungeon's key, every
           port's ship (the stones are what open them), Dragonbane unmade. */
        quest_stage = Q_HEART;
        items = IT_SIGIL | IT_TIDAL | IT_ASHEN | IT_EMBER | IT_TEAR | IT_HEART;
        G->quiet = OWG_CALM;    /* and nothing wanders out at a test */
    } else {
        make_party();
        G->area  = AREA_TOWN;   /* at Millford's gate (the bootstrap */
        G->place = TOWN_MILLFORD;   /* left wx, wy on Millford) */
        G->x     = OWG_GATE;
        G->quiet = 0;
    }
    potions = party_count;      /* a draught for each who answered */
    owg_out();
    for (i = 0; i < MAX_PARTY; ++i) {
        G->tunic[i] = tunic(party[i].name);
    }
    G->magic = OWG_MAGIC;
}
