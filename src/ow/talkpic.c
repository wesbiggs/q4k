/*
 * Quest for the King -- the open world's talk with a portrait (owtalk).
 *
 * owtalk's ts_* (q4k/talkscr.h): talk.c's dialog_talk() draws and reads
 * keys through these. A person with a portrait on the disk -- `v.` and
 * their number, then `n` or `p` for the machine's video system, made from
 * assets/portraits by tools/mkportrait.py -- is shown in it, at the top
 * left of a hires bitmap in VIC bank 1, with the talk's words beside it
 * and the topics below, drawn by vic2orious's v2t.inc (src/ow/picshow.s).
 * Anyone else, or anyone whose portrait will not load or show, talks on the
 * text screen as before (talkscr.c's tsc_*).
 *
 * The portrait is loaded at PIC_ADDR and stays there while it is shown:
 * its display code runs in it. Once it is up the KERNAL is out, so the
 * person's lines are loaded first (ts_ready() is after talk.c's load()),
 * nothing here may call the KERNAL until ts_close(), and keys come from
 * picshow.s's pt_key().
 *
 *   rows 0-15   the portrait (16 or 20 cells wide: PAL's or NTSC's), and
 *               beside it the name and what is said, a page at a time
 *   rows 17-21  the topics, two columns
 *   row 24      the hint
 */

#include <cbm.h>
#include <conio.h>
#include <c64.h>

#include <q4k/savegame.h>
#include <q4k/music.h>
#include <q4k/talkscr.h>
#include <q4k/text.h>

/* picshow.s */
unsigned char __fastcall__ pic_show(const void *v2i);
void pic_end(void);
void __fastcall__ pt_line(const char *ascii);
void __fastcall__ pt_clear(unsigned char colour);
unsigned char pt_key(void);
extern unsigned char pic_cols;
extern const unsigned char pt_fontw[];
extern unsigned int  pt_x;
extern unsigned char pt_y, pt_cols, pt_color, pt_col, pt_row, pt_rows;

#define PIC_ADDR   ((void *)0x8000)     /* tools/mkportrait.py's */
#ifndef PICS                            /* a bit a person with a portrait */
#define PICS       0xFFFF               /* (the Makefile's PORTRAITS) */
#endif
#define PALFLAG    (*(unsigned char *)0x02A6)
#define MEMSIZ     (*(unsigned int *)0x0283)

#define SAY_TOP    2                    /* character rows */
#define SAY_END    16
#define MENU_ROW   17
#define HINT_ROW   24

static unsigned char pic;               /* the portrait is up */
static const char *who;
static char name[41];                   /* who, in ASCII, kept: npc_names[]'
                                           letters are town_data.c's MAPDATA,
                                           in the bank the portrait takes */
static unsigned char x0, cols;          /* the words' column and width, cells */
static unsigned char row;               /* the words' next row */
static char line[41];

static void at(unsigned char x, unsigned char y, unsigned char w,
               unsigned char colour)
{
    pt_x = (unsigned int)x << 3;
    pt_y = y << 3;
    pt_cols = w;
    pt_color = colour << 4;             /* on black */
}

static void clear(unsigned char x, unsigned char y, unsigned char w,
                  unsigned char h)
{
    pt_col = x;
    pt_row = y;
    pt_cols = w;
    pt_rows = h;
    pt_clear(0);
}

/* The words beside the portrait, a line at a time: a full page starts
   again at the top. */
static void say_line(const char *ascii, unsigned char colour)
{
    if (row >= SAY_END) {
        clear(x0, SAY_TOP, cols, SAY_END - SAY_TOP);
        row = SAY_TOP;
    }
    at(x0, row, cols, colour);
    pt_line(ascii);
    ++row;
}

/* Wrapped by the letters' widths in pixels, at a space where there is one. */
static void say_wrap(const char *ascii, unsigned char colour)
{
    unsigned char i, brk;
    unsigned int w, room;
    char c;

    room = (unsigned int)cols << 3;
    while (*ascii) {
        w = 0;
        brk = 0;
        for (i = 0; (c = ascii[i]) != 0; ++i) {
            if (c == 0x20) {
                brk = i;
            }
            if (c >= 0x20 && c < 0x80) {
                w += pt_fontw[c - 0x20];
            }
            if (w > room || i == sizeof line - 1) {
                break;
            }
        }
        if (!c) {
            brk = i;                    /* the rest fits */
        } else if (!brk) {
            brk = i;                    /* one long word: split */
        }
        for (i = 0; i < brk; ++i) {
            line[i] = ascii[i];
        }
        line[brk] = 0;
        say_line(line, colour);
        ascii += brk;
        while (*ascii == 0x20) {
            ++ascii;
        }
    }
}

void ts_open(const char *n)
{
    who = n;
    str_lit(name, n);
    pic = 0;
}

void ts_ready(unsigned char npc)
{
    static char file[5];

    if (npc > 15 || !(PICS & (1u << npc))) {
        tsc_open(who);                  /* no file to look for */
        return;
    }
    file[0] = 0x56;                    /* "v.", the number, n or p: */
    file[1] = 0x2E;                     /* (numerically: see "Screen */
    file[2] = 0x30 + npc;               /* codes" in CLAUDE.md) */
    file[3] = PALFLAG ? 0x50 : 0x4E;
    file[4] = 0;
    MEMSIZ = 0xCF00;
    music_pause();
    if (cbm_load(file, save_device, PIC_ADDR)) {
        music_resume();
        if (pic_show(PIC_ADDR) == 0) {
            pic = 1;
            x0 = pic_cols + 1;
            cols = 40 - x0;
            at(x0, 0, cols, COLOR_YELLOW);
            pt_line(name);
            at(0, HINT_ROW, 40, COLOR_GRAY2);
            str_lit(line, "Number to ask, RUN/STOP leaves");
            pt_line(line);
            row = SAY_TOP;
            return;
        }
    } else {
        music_resume();
    }
    tsc_open(who);
}

void ts_ask(const char *ascii)
{
    if (!pic) {
        tsc_ask(ascii);
        return;
    }
    clear(x0, SAY_TOP, cols, SAY_END - SAY_TOP);
    row = SAY_TOP;
    say_wrap(ascii, COLOR_WHITE);
}

void ts_say(const char *ascii, unsigned char color, unsigned char wrap)
{
    if (!pic) {
        tsc_say(ascii, color, wrap);
        return;
    }
    say_wrap(ascii, color);             /* (a narrower column: always) */
}

void ts_topic(unsigned char i, const char *ascii)
{
    if (!pic) {
        tsc_topic(i, ascii);
    } else if (i == TS_ALL) {
        clear(0, MENU_ROW, 40, 5);
    } else {
        at(i & 1 ? 20 : 0, MENU_ROW + (i >> 1), 20, COLOR_WHITE);
        pt_line(ascii);
    }
}

unsigned char ts_key(void)
{
    return pic ? pt_key() : tsc_key();
}

void ts_close(void)
{
    if (pic) {
        pic_end();
        pic = 0;
    }
}
