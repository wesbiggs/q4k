/*
 * q4k -- a conversation on the text screen (q4k/talkscr.h).
 *
 * What dialog_talk() (talk.c) drew itself before the open world's talk had
 * a portrait to show: the name, a transcript that scrolls, two columns of
 * topics and a hint, written with conio and the panel.
 */

#include <conio.h>
#include <c64.h>

#include <q4k/panel.h>
#include <q4k/sprite.h>
#include <q4k/talkscr.h>
#include <q4k/text.h>
#include <q4k/view.h>

/* As talk.c's: the charset's spare glyphs (cfg/c64-q4k.cfg). */
#pragma rodata-name ("CHDATA2")

#define TR_Y      3    /* transcript */
#define TR_H     12
#define MENU_Y   17    /* topics, two columns of five rows */
#define COL_W    20
#define HINT_Y   23

void tsc_open(const char *name)
{
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
}

void tsc_ask(const char *ascii)
{
    panel_line(ascii, COLOR_WHITE);
}

void tsc_say(const char *ascii, unsigned char color, unsigned char wrap)
{
    if (wrap) {
        panel_wrap(ascii, color);
    } else {
        panel_line(ascii, color);
    }
}

void tsc_topic(unsigned char i, const char *ascii)
{
    if (i == TS_ALL) {
        for (i = 0; i < 5; ++i) {
            text_clear(0, MENU_Y + i, SCR_W);
        }
    } else {
        text_put(i & 1 ? COL_W : 0, MENU_Y + (i >> 1), ascii, COLOR_WHITE);
    }
}

unsigned char tsc_key(void)
{
    return cgetc();
}
