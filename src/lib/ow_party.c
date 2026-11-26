/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Quest for the King -- the open world's party, made.
 *
 * F1 on the splash screen is a new game: the bootstrap builds the world and
 * then loads OWPARTY (src/owparty.c) over itself with OWB_NEW, which calls
 * this. It tells the story, and then shows who answered the king's letter:
 * a warrior, an archer and a wizard, each with a name picked from the title
 * screen's list. 1-3 opens one's page, with their figure (q4k/owfig.h), to
 * change their class, name, pronouns, skin or clothes, put them in another
 * place, or send them home; A adds
 * another while there are fewer than three; RETURN begins. The new game
 * goes into the game block (q4k/owgame.h), at Millford's gate, and from
 * there into OW-SAVE on the disk, so that F3 at the title starts it again;
 * the engine is loaded over it after, so nothing of this stays in memory.
 *
 * OWB_QUICK, the tests' start (screendump.py --splash quick), makes a lone
 * warrior called Alaric without asking, with the keys to every dungeon and
 * the three stones (so every ship sails), on the overworld outside Millford:
 * where the prototype always began, with nothing shut.
 */

#include <c64.h>
#include <cbm.h>
#include <conio.h>
#include <peekpoke.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/charset.h>
#include <q4k/game.h>
#include <q4k/ow.h>
#include <q4k/owfig.h>
#include <q4k/owgame.h>
#include <q4k/probe.h>
#include <q4k/tiles.h>
#include <q4k/town_data.h>

#define SCR_W     40
#define SCREEN_RAM ((unsigned char *)0x0400)
#define STORY_Y   5
#define ROSTER_Y  5             /* the party's rows */
#define ASK_Y    17
#define ENTRY_Y  18
#define HINT_Y   20
#define FIG_COL  29             /* a member's figure, on their own page */
#define FIG_ROW   5

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

/* The party's names, picked for them: the title screen's. */
static const char *const names[] = {
    "Aldwin",  "Bramwell", "Caelan",  "Doran",   "Elowen",
    "Faelan",  "Gareth",   "Halvard", "Isolde",  "Jorund",
    "Kestrel", "Lirael",   "Maelis",  "Neriah",  "Orrin",
    "Perrin",  "Quenby",   "Rowena",  "Sigrun",  "Talwyn",
    "Ulric",   "Vesper",   "Wystan",  "Zephyr",
};
#define NUM_NAMES (sizeof(names) / sizeof(names[0]))

/* ... and the pronouns each name comes with (PRON_*, actor.h), which the
   page's P changes: the old forms of the names, he or she, and they for
   the ones that are words or are either. */
#define T PRON_THEY
#define M PRON_HE
#define F PRON_SHE
static const unsigned char name_pron[NUM_NAMES] = {
    M, M, M, M, F,              /* Aldwin .. Elowen   */
    M, M, M, F, M,              /* Faelan .. Jorund   */
    T, F, F, F, M,              /* Kestrel .. Orrin   */
    M, F, F, F, T,              /* Perrin .. Talwyn   */
    M, T, M, T,                 /* Ulric .. Zephyr    */
};
#undef T
#undef M
#undef F

/* A member's pronouns in a sentence: object and possessive. */
static const char *const p_obj[4]  = { "them", "him", "her", "it" };
static const char *const p_poss[4] = { "Their", "His", "Her", "Its" };
/* P steps them on: he/him, she/her, they/them, it, and round again (as
   the party screen, owtown's I, does). */
static const unsigned char pron_next[4] = {
    PRON_IT, PRON_SHE, PRON_THEY, PRON_HE       /* after they, he, she, it */
};

static char entry[NAME_LEN];
static unsigned char picked;    /* the pronouns of the name pick_name() took */

static unsigned char tunic(const char *name);

static void hint(unsigned char y, const char *s)
{
    textcolor(COLOR_GRAY2);
    cputsxy(1, y, s);
}

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

/* The title, a rule under it, and nothing else. */
static void heading(void)
{
    clrscr();
    textcolor(COLOR_YELLOW);
    cputsxy(11, 1, "Quest for the King");
    textcolor(COLOR_GRAY1);
    chlinexy(0, 3, SCR_W);
}

/* Member `who` called by a name nobody else in the party has, into
   entry[]. */
static void pick_name(unsigned char who)
{
    unsigned char i = (unsigned char)(rand() % NUM_NAMES), n;

    for (n = 0; n < party_count; ) {
        if (n == who || strcmp(party[n].name, names[i])) {
            ++n;
        } else {
            i = (i + 1) % NUM_NAMES;
            n = 0;
        }
    }
    strcpy(entry, names[i]);
    picked = name_pron[i];
}

/* Member `who` made a level-1 `cls` called entry[], with the pronouns of
   the name picked and the colours it gives them (the page changes both). */
static void make(unsigned char who, unsigned char cls)
{
    party_make(who, entry, cls);
    ACT_SET_PRON(party[who], picked);
    G->tunic[who] = tunic(entry);
}

/* `pre`, member `who`'s pronoun from `table`, and `post`, at x, y. */
static void pron_at(unsigned char x, unsigned char y, const char *pre,
                    const char *const *table, unsigned char who,
                    const char *post)
{
    cputsxy(x, y, pre);
    cputs(table[ACT_PRON(party[who])]);
    cputs(post);
}

/* entry[] typed into from its first n characters, to RETURN, which gives
   how many there are then: DEL takes one back, and no more than `most` go
   in. Digits only for a seed, else what a name may have. */
static unsigned char typed(unsigned char n, unsigned char most,
                           unsigned char digits)
{
    unsigned char key;

    for (;;) {
        textcolor(COLOR_WHITE);
        cclearxy(1, ENTRY_Y, SCR_W - 2);
        entry[n] = '\0';
        cputsxy(1, ENTRY_Y, entry);
        SCREEN_RAM[(unsigned int)ENTRY_Y * SCR_W + 1 + n] = 0xA0;
        COLOR_RAM[(unsigned int)ENTRY_Y * SCR_W + 1 + n]  = COLOR_GRAY2;

        key = timed_key();
        if (key == KEY_RETURN) {
            return n;
        }
        if (key == KEY_DEL) {
            if (n) {
                --n;
            }
            continue;
        }
        if (digits) {
            if (key < 0x30 || key > 0x39) {     /* (PETSCII's are ASCII's) */
                continue;
            }
        /* PETSCII as typed: $41-$5A unshifted, $C1-$DA shifted. */
        } else if (key < 0x20 || (key > 0x3F && key < 0x41)
                   || (key > 0x5A && key < 0xC1) || key > 0xDA) {
            continue;
        }
        if (n < most) {
            entry[n++] = (char)key;
        }
    }
}

/* A name into entry[], starting from member `who`'s: typed over (DEL
   takes a letter back), or RETURN with none for one picked. */
static void read_name(unsigned char who)
{
    strcpy(entry, party[who].name);
    ask("");
    textcolor(COLOR_YELLOW);
    pron_at(1, ASK_Y, "", p_poss, who, " name? (RETURN alone picks one)");
    if (!typed((unsigned char)strlen(entry), NAME_LEN - 1, 0)) {
        pick_name(who);
    }
}

/* A label and a number of 0-99 at x, y. */
static void stat(unsigned char x, unsigned char y, const char *s,
                 unsigned char v)
{
    cputsxy(x, y, s);
    if (v > 9) {
        cputc('0' + v / 10);
    }
    cputc('0' + v % 10);
}

/* Member `who`'s health and a wizard's magic, at x. */
static void numbers(unsigned char who, unsigned char x, unsigned char y)
{
    textcolor(COLOR_GRAY3);
    stat(x, y, "HP ", party[who].hp_max);
    if (party[who].mp_max) {
        stat(x + 7, y, "MP ", party[who].mp_max);
    }
}

/* Member `who`'s own page: who they are, their figure, and one key for
   each thing about them that may be changed; the cursor keys go through
   the others, 1-3 puts them in that place, and RETURN goes back to the
   party. */
static void edit(unsigned char who)
{
    unsigned char key, k, i;

    for (;;) {
        heading();
        if (party_count > 1) {
            textcolor(COLOR_GRAY2);
            gotoxy(34, 1);
            cputc('1' + who);
            cputs(" of ");
            cputc('0' + party_count);
        }
        textcolor(COLOR_WHITE);
        cputsxy(1, 5, party[who].name);
        textcolor(COLOR_LIGHTBLUE);
        cputsxy(1, 6, class_names[party[who].cls]);
        textcolor(COLOR_GRAY3);
        cputsxy(1, 7, pron_names[ACT_PRON(party[who])]);
        numbers(who, 1, 9);
        textcolor(COLOR_WHITE);
        cputsxy(1, 11, "C  Class");
        cputsxy(1, 12, "N  Name");
        cputsxy(1, 13, "P  Pronouns");
        cputsxy(1, 14, "S  Skin colour");
        cputsxy(1, 15, "T  Tunic colour");
        if (party_count > 1) {
            pron_at(1, 16, "DEL  Send ", p_obj, who, " home");
            hint(20, "Cursor left, right  the others");
            hint(21, party_count > 2 ? "1-3  Change places"
                                     : "1 2  Change places");
        }
        hint(23, "RETURN  Back to the party");
        fig_show(who, FIG_COL, FIG_ROW);

        key = fig_key(who);
        k = key & 0x7F;
        if (key == KEY_RETURN) {
            fig_off();
            return;
        } else if (key == K_CRSR_RIGHT) {
            who = who + 1 == party_count ? 0 : who + 1;
        } else if (key == K_CRSR_LEFT) {
            who = who ? who - 1 : party_count - 1;
        } else if ((unsigned char)(key - '1') < party_count) {
            key -= '1';                         /* another place */
            fig_swap(who, key);
            who = key;
        } else if (key == KEY_DEL) {            /* sent home */
            if (party_count > 1) {
                fig_off();
                for (i = who; i + 1 < party_count; ++i) {
                    party[i] = party[i + 1];
                    G->tunic[i] = G->tunic[i + 1];
                }
                memset(&party[i], 0, sizeof party[i]);
                --party_count;
                return;
            }
        } else if (k == 'c') {                  /* the next class */
            strcpy(entry, party[who].name);
            picked = ACT_PRON(party[who]);
            i = G->tunic[who];
            key = party[who].cls + 1;
            make(who, key == NUM_CLASSES ? 0 : key);
            G->tunic[who] = i;                  /* (as they were dressed) */
        } else if (k == 'n') {                  /* a name, and only that */
            read_name(who);
            strcpy(party[who].name, entry);
        } else if (k == 'p') {                  /* the next pronouns */
            ACT_SET_PRON(party[who], pron_next[ACT_PRON(party[who])]);
        } else if (k == 's' || k == 't') {      /* a colour (SHIFT: back) */
            fig_step(who, k == 's', key & 0x80);
        }
    }
}

/* The story, then the party the letter found: a warrior, an archer and a
   wizard, named for them, to change, send home or add to before the road
   to Millford. */
static void make_party(void)
{
    unsigned char i, key;

    heading();
    textcolor(COLOR_LIGHTGREEN);
    for (i = 0; i < STORY_LINES; ++i) {
        cputsxy(1, STORY_Y + i, story[i]);
    }
    hint(HINT_Y + 2, "Press any key");
    VIC.ctrl1 |= 0x10;          /* the screen on: the loader left it off */
    while (kbhit()) {
        cgetc();
    }
    probe_stage(1);
    timed_key();

    party_count = 0;
    for (i = 0; i < MAX_MADE; ++i) {
        pick_name(i);
        make(i, i);             /* CL_WARRIOR, CL_ARCHER, CL_WIZARD */
    }

    for (;;) {
        heading();
        textcolor(COLOR_LIGHTGREEN);
        cputsxy(1, ROSTER_Y - 1, "These answered the king's letter:");
        for (i = 0; i < party_count; ++i) {
            textcolor(COLOR_YELLOW);
            gotoxy(1, ROSTER_Y + 1 + i * 2);
            cputc('1' + i);
            textcolor(COLOR_WHITE);
            cputsxy(4, ROSTER_Y + 1 + i * 2, party[i].name);
            textcolor(COLOR_LIGHTBLUE);
            cputsxy(14, ROSTER_Y + 1 + i * 2, class_names[party[i].cls]);
            numbers(i, 23, ROSTER_Y + 1 + i * 2);
            textcolor(COLOR_GRAY2);
            cputsxy(4, ROSTER_Y + 2 + i * 2, pron_names[ACT_PRON(party[i])]);
        }
        textcolor(COLOR_WHITE);
        if (party_count > 1) {
            stat(1, 14, "1-", party_count);
            cputs("  Change one");
        } else {
            pron_at(1, 14, "1  Change ", p_obj, 0, "");
        }
        if (party_count < MAX_MADE) {
            cputsxy(1, 15, "A  Add another");
        }
        cputsxy(1, 17, "RETURN  Begin the adventure");

        key = timed_key();
        if (key == KEY_RETURN) {
            break;
        }
        if ((key & 0x7F) == 'a' && party_count < MAX_MADE) {
            /* Whoever is missing from the three, or a warrior. */
            for (key = 0; key < NUM_CLASSES; ++key) {
                for (i = 0; i < party_count && party[i].cls != key; ++i) {
                }
                if (i == party_count) {
                    break;
                }
            }
            i = party_count;
            pick_name(i);
            make(i, key < NUM_CLASSES ? key : CL_WARRIOR);
            edit(i);
            continue;
        }
        key -= 0x31;
        if (key < party_count) {
            edit(key);
        }
    }
    heading();
    textcolor(COLOR_YELLOW);
    cputsxy(1, 5, "Then to Millford...");
}

/* A member's colours from their name, as the game's sprite.c's tunic_of()
   picks the clothes: the same name always gets the same ones, clothes never
   black or white, and skin light red or brown, even odds, and never the
   clothes' (tools/mkowsave.py and src/owarena.c have it too). The high four
   bits are the skin, the low the clothes (OWG_SKIN, OWG_DRESS). The party
   screen changes them. */
static unsigned char tunic(const char *name)
{
    unsigned char h = 0, c, k;

    while (*name) {
        h = (unsigned char)((h << 1) | (h >> 7)) ^ *name++;
    }
    c = (unsigned char)(2 + h % 14);
    k = h & 0x10 ? COLOR_BROWN : COLOR_LIGHTRED;
    if (c == k) {
        ++c;                    /* (9 or 10: 10 or 11) */
    }
    return (unsigned char)((k << 4) | c);
}

/* The new game into OW-SAVE, over the one the disk came with, as the
   engine's F1 does (src/ow/engine.c's save_game()): the drive will not
   overwrite, so the old one is scratched first, the command spelled in
   bytes (cc65 would shift a literal's letters). The border is light blue
   while the drive works; if it would not, the screen says so and waits
   for a key, and the game starts all the same. */
static const char scratch_save[] = {
    0x53, 0x30, 0x3A,                           /* S0:      */
    0x4F, 0x57, 0x2D, 0x53, 0x41, 0x56, 0x45,   /* OW-SAVE  */
    0
};

static void save_new(void)
{
    unsigned char ok = 0, st[2];

    POKEW(0x0283, 0xCF00);      /* MEMSIZ past the game block */
    VIC.bordercolor = COLOR_LIGHTBLUE;
    if (cbm_open(15, 8, 15, scratch_save) == 0) {
        cbm_read(15, st, 2);            /* (no save yet is fine) */
        ok = cbm_save(scratch_save + 3, 8, G, OWG_SIZE) == 0;
        st[0] = 0;
        cbm_read(15, st, 2);
        ok &= st[0] == 0x30 && st[1] == 0x30;
        cbm_close(15);
    }
    VIC.bordercolor = COLOR_BLACK;
    if (!ok) {
        textcolor(COLOR_LIGHTRED);
        cputsxy(1, 7, "The game could not be saved.");
        hint(9, "Press any key");
        cgetc();
    }
}

/* SHIFT+F1 on the splash (OW_ASKSEED): the world's seed, typed, until it
   is one of 0-255. */
unsigned char ow_ask_seed(void)
{
    unsigned char n, i;
    unsigned int v;

    heading();
    ask("The world's seed? (0-255)");
    hint(HINT_Y, "RETURN alone keeps this one");
    do {
        n = typed(0, 3, 1);
        v = 0;
        for (i = 0; i < n; ++i) {
            v = v * 10 + (entry[i] - 0x30);
        }
    } while (v > 255);
    if (n && v != OW->seed) {
        OW->seed = (unsigned char)v;
        return 1;
    }
    return 0;
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
        /* Everything open but the last of it: every dungeon's key, the
           king's ship, Dragonbane unmade. */
        quest_stage = Q_HEART;
        items = IT_SIGIL | IT_TIDAL | IT_ASHEN | IT_EMBER | IT_TEAR | IT_HEART
              | IT_SHIP;
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
    for (i = party_count; i < MAX_PARTY; ++i) {
        G->tunic[i] = 0;
    }
    if (quick) {
        G->tunic[0] = tunic(party[0].name);
    }
    G->foe_made = 0;
    G->fled     = 0;
    memset(G->tmap, 0, sizeof G->tmap);     /* no treasure maps */
    G->magic    = OWG_MAGIC;
    if (!quick) {
        save_new();
    }
}
