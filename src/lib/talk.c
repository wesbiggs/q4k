/*
 * q4k -- talking to people, offline.
 *
 * The offline build's dialog_talk() and dialog_hint() (see dialog.h): what
 * everyone can say is on the disk, one file per person and one per inn, made
 * by tools/mktalk.py, and loaded into talk[] when it is wanted. The
 * player picks a topic from a menu instead of typing. Only the offline build
 * archives this module, and only the networked one dialog.c, line.c and
 * modem.c.
 */

#include <cbm.h>
#include <conio.h>
#include <c64.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/dialog.h>
#include <q4k/engine.h>
#include <q4k/gift.h>
#include <q4k/inn.h>
#include <q4k/game.h>
#include <q4k/music.h>
#include <q4k/panel.h>
#include <q4k/savegame.h>
#include <q4k/sprite.h>
#include <q4k/text.h>
#include <q4k/town_data.h>
#include <q4k/view.h>

/* Read-only data goes in the charset's spare glyphs: cfg/c64-q4k.cfg. The
   networked build's dialog.c has these bytes. */
#pragma rodata-name ("CHDATA2")

#define TR_Y      3    /* transcript */
#define TR_H     12
#define MENU_Y   17    /* topics, two columns of five rows: 1-9, then 0 */
#define COL_W    20
#define HINT_Y   23

/* An entry in a file: flags, the stages as lo << 4 | hi, a topic's label,
   the text. tools/mktalk.py says more. */
#define T_TOPIC  0x01
#define T_GIVE   0x80
#define T_END    0xFF

/* The inns' files come after the people's; every generated inn shares the
   last. */
#define FIRST_INN (NPC_ASH + 1)

#define KEY_STOP  0x03
#define KEY_1     0x31

#define NONE      0xFF

/* Top of BASIC memory: see savegame.c, which raises it for the same reason. */
#define MEMSIZ (*(unsigned int *)0x0283)

extern unsigned char talk[];        /* talkbuf.s: the end of this engine */

static unsigned char held = NONE;   /* the file talk[] holds */
static unsigned char found;         /* how many find() passed */
static unsigned char told = NONE;   /* the last pick, not to be told twice running */
static char seg[SCR_W + 1];
static const char *(*gift)(unsigned char granted);

static unsigned char load(unsigned char file)
{
    static char name[3];

    if (held == file) {
        return 1;
    }
    /* Digits are the same in ASCII and PETSCII, but spelled numerically
       anyway: see "Screen codes" in CLAUDE.md. */
    if (file < 10) {
        name[0] = 0x30 + file;
        name[1] = 0;
    } else {
        name[0] = 0x31;
        name[1] = 0x30 + file - 10;
        name[2] = 0;
    }
    MEMSIZ = 0xCF00;
    music_pause();
    held = cbm_load(name, save_device, talk) ? file : NONE;
    music_resume();
    if (held == NONE) {
        talk[0] = T_END;
        return 0;
    }
    return 1;
}

/* The entry's text, past its label if it has one. */
static char *text_of(unsigned char *e)
{
    char *s = (char *)e + 2;

    if (*e & T_TOPIC) {
        s += strlen(s) + 1;
    }
    return s;
}

/* The `n`th entry of `kind` that fits the quest stage, counting from 0, or 0
   if there are not that many; either way `found` says how many it passed. */
static unsigned char *find(unsigned char kind, unsigned char n)
{
    unsigned char *e = talk;
    char *s;
    unsigned char r;

    found = 0;
    while (*e != T_END) {
        r = e[1];
        if ((*e & T_TOPIC) == kind
            && quest_stage >= (r >> 4) && quest_stage <= (r & 0x0F)) {
            if (found == n) {
                return e;
            }
            ++found;
        }
        s = text_of(e);
        e = (unsigned char *)s + strlen(s) + 1;
    }
    return 0;
}

/* A greeting, or a rumour, that fits the stage, at random. */
static unsigned char *pick(void)
{
    unsigned char n;

    find(0, NONE);
    if (!found) {
        return 0;
    }
    n = (unsigned char)(rand() % found);
    if (n == told && found > 1 && ++n == found) {
        n = 0;
    }
    told = n;
    return find(0, n);
}

/* Say an entry, and hand over what it gives. */
static void say(unsigned char *e)
{
    const char *note;

    if (!e) {
        return;
    }
    panel_wrap(text_of(e), COLOR_LIGHTGREEN);
    if (*e & T_GIVE) {
        note = gift(GRANT_OPEN);
        if (note) {
            panel_lit(note, COLOR_YELLOW);
        }
    }
}

/* The topics for the stage as it is now, which a gift may have moved on.
   Returns how many. */
static unsigned char menu(void)
{
    unsigned char i;
    unsigned char *e;

    for (i = 0; i < 5; ++i) {
        text_clear(0, MENU_Y + i, SCR_W);
    }
    for (i = 0; (e = find(T_TOPIC, i)) != 0; ++i) {
        seg[0] = KEY_1 + i;
        seg[1] = 0x20;
        strcpy(seg + 2, (char *)e + 2);
        text_put(i & 1 ? COL_W : 0, MENU_Y + (i >> 1), seg, COLOR_WHITE);
    }
    return i;
}

unsigned char dialog_talk(unsigned char npc, const char *name,
                          const char *(*given)(unsigned char granted))
{
    unsigned char n, key;
    unsigned char *e;

    gift = given;
    sprite_hide();
    clrscr();
    textcolor(COLOR_YELLOW);
    cputsxy(0, 0, name);
    textcolor(COLOR_GRAY1);
    chlinexy(0, 1, SCR_W);
    chlinexy(0, MENU_Y - 1, SCR_W);
    textcolor(COLOR_GRAY2);
    cputsxy(0, HINT_Y, "Number to ask, RUN/STOP leaves");
    panel_begin(TR_Y, TR_H);

    /* Always from the disk: another engine may have had the slot since. */
    held = NONE;
    if (!load(npc)) {
        return DIALOG_ENOLINE;
    }

    say(pick());
    for (;;) {
        n = menu();
        key = cgetc();
        if (key == KEY_STOP) {
            return DIALOG_OK;
        }
        key -= KEY_1;
        if (key < n) {
            e = find(T_TOPIC, key);
            str_asc(str_lit(seg, "> "), (char *)e + 2);
            panel_line(seg, COLOR_WHITE);
            say(e);
        }
    }
}

unsigned char dialog_hint(unsigned char inn)
{
    unsigned char *e;

    if (!load(FIRST_INN + (inn < NUM_INNS ? inn : NUM_INNS))) {
        return DIALOG_ENOLINE;
    }
    e = pick();
    if (!e) {
        return DIALOG_ENOLINE;
    }
    panel_wrap(text_of(e), COLOR_LIGHTGREEN);
    return DIALOG_OK;
}

void dialog_forget(void)
{
    held = NONE;
}
