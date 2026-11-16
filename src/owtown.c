/*
 * Quest for the King -- the open world's town: its screens of text.
 *
 * The smooth-scrolling prototype's engine (src/ow/engine.c) has no room for
 * anything but the map, so the screens of text are this program, loaded
 * over the engine when they are wanted (ow_run), with the hand-off block
 * (q4k/ow.h's OWB) saying what for:
 *
 *   OWB_NEW, OWB_QUICK  the party's making, after the splash screen's F1
 *                       (src/lib/ow_party.c), which ends in Millford
 *   OWB_TALK            a conversation, when the hero walks into a
 *                       townsperson: the offline talk (src/lib/talk.c,
 *                       which reads each person's lines from the disk),
 *                       and what it hands over (src/lib/gift.c), which
 *                       moves the quest on
 *
 * It reads and writes the game through the game block (q4k/owgame.h), and
 * when it is done the engine is loaded back (ow_back()) and carries on from
 * there: in the same town, where the hero stood. Everything the engine
 * keeps is rebuilt from the disk on the way (OW-DATA, which this program
 * writes over, is loaded again), or is still in memory: the vault under the
 * I/O area and the music under the KERNAL are not reached. Linked like the
 * test programs, with cfg/c64-flat.cfg (one load).
 */

#include <cbm.h>
#include <conio.h>
#include <c64.h>
#include <peekpoke.h>

#include <q4k/charset.h>
#include <q4k/dialog.h>
#include <q4k/gift.h>
#include <q4k/ow.h>
#include <q4k/owgame.h>
#include <q4k/tiles.h>
#include <q4k/town_data.h>

/* A line said on a screen of its own: what a person handed over when
   there was nothing to read of theirs. Any key goes on. */
static void note_screen(const char *s)
{
    clrscr();
    textcolor(COLOR_YELLOW);
    cputsxy(1, 10, s);
    textcolor(COLOR_GRAY2);
    cputsxy(1, 12, "Press a key.");
    cgetc();
}

/* A conversation, and whatever it hands over (the game's quest_after_talk(),
   gift.c): the Sigil, the keys, Garrett's arms and Dragonbane, the king's
   reward. With nothing on the disk for the person, what they would have
   given is given all the same, as the game does. */
static void talk(unsigned char npc)
{
    const char *note;

    talking_to = npc;
    if (dialog_talk(npc, npc_names[npc], quest_after_talk) != DIALOG_OK) {
        note = quest_after_talk(GRANT_OPEN);
        if (note) {
            note_screen(note);
        }
    }
}

int main(void)
{
    unsigned char mode = OWB->mode;

    if (mode != OWB_TALK && mode != OWB_NEW && mode != OWB_QUICK) {
        cputs("Load OPENWORLD: it builds the world.");
        for (;;) {
        }
    }

    tiles_install();            /* the character set, before any text */
    VIC.bordercolor = COLOR_BLACK;
    VIC.bgcolor0    = COLOR_BLACK;
    VIC.addr        = CHARSET_D018;
    *(unsigned char *)0x0291 = 0x80;    /* C=+Shift keeps this set */
    clrscr();
    while (kbhit()) {
        cgetc();
    }
    VIC.ctrl1 |= 0x10;          /* set up: the screen on */

    if (mode == OWB_TALK) {
        owg_in();
        talk(OWB->npc);
        owg_out();
    } else {
        ow_new_game(mode == OWB_QUICK);
    }
    ow_back();                  /* the engine, which carries on from the
                                   game block (owgame.h) */
    return 0;
}
