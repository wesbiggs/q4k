#include <conio.h>
#include <c64.h>

#include <string.h>

#include <q4k/probe.h>
#include <q4k/title.h>
#include <q4k/view.h>

#define STORY_Y   5
#define ASK_Y    17
#define ENTRY_Y  18
#define HINT_Y   21

#define KEY_RETURN 0x0D
#define KEY_DEL    0x14

static const char *const story[] = {
    "THE DRAGON MALDRATH CAME OVER THE",
    "SUNDERING THREE WINTERS AGO, AND HAS",
    "NOT LEFT. TWO ARMIES WENT AGAINST HIM.",
    "NEITHER CAME BACK.",
    "",
    "A LETTER COMES, UNDER THE KING'S SEAL.",
    "MATHESON SUMMONS YOU TO THE CASTLE.",
    "",
    "THE BLADE THAT CAN BANISH HIM WAS",
    "BROKEN, AND ITS THREE BINDING STONES",
    "LIE SEALED IN THREE DEEP PLACES.",
};
#define STORY_LINES (sizeof(story) / sizeof(story[0]))

void title_screen(char *name, unsigned char max)
{
    /* Typed into a buffer of its own. Terminating `name` as it is drawn
       would erase the caller's default before the player touched a key. */
    static char entry[17];
    unsigned char i, n = 0, key;

    if (max > sizeof(entry)) {
        max = sizeof(entry);
    }

    VIC.bordercolor = COLOR_BLACK;
    VIC.bgcolor0    = COLOR_BLACK;
    VIC.addr        = 0x17;   /* ROM mixed-case set: this screen is all text */

    clrscr();

    textcolor(COLOR_YELLOW);
    cputsxy(11, 1, "QUEST FOR THE KING");
    textcolor(COLOR_GRAY1);
    chlinexy(0, 3, SCR_W);

    textcolor(COLOR_LIGHTGREEN);
    for (i = 0; i < STORY_LINES; ++i) {
        cputsxy(1, STORY_Y + i, story[i]);
    }

    textcolor(COLOR_GRAY1);
    chlinexy(0, ASK_Y - 1, SCR_W);
    cputsxy(1, ASK_Y, "AND WHO ARE YOU?");
    textcolor(COLOR_GRAY2);
    cputsxy(1, HINT_Y, "RETURN TO BEGIN AS ");
    cputsxy(20, HINT_Y, name);

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

        key = cgetc();

        if (key == KEY_RETURN) {
            if (n) {
                entry[n] = '\0';
                strcpy(name, entry);
            }
            return;                 /* nothing typed: the default stands */
        }
        if (key == KEY_DEL) {
            if (n) {
                --n;
            }
            continue;
        }

        /* Unshifted letters arrive as $41-$5A, which this character set
           draws in lower case. Names look better in capitals, and capitals
           are the shifted range. */
        if (key >= 0x41 && key <= 0x5A) {
            key += 0x80;
        } else if (key < 0x20 || (key > 0x3F && key < 0xC1) || key > 0xDA) {
            continue;               /* not something a name is made of */
        }

        if (n < max - 1) {
            entry[n++] = (char)key;
        }
    }
}
