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
 *                       which reads each person's lines from the disk)
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
#include <q4k/ow.h>
#include <q4k/owgame.h>
#include <q4k/tiles.h>
#include <q4k/town_data.h>

/* What the offline talk reads of the game, which the prototype does not
   have: no quest, so every line is at the stage it starts at, and no save
   device of its own but the disk. */

/* Nothing is handed over here (see above). */
static const char *no_gift(unsigned char granted)
{
    (void)granted;
    return 0;
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
        dialog_talk(OWB->npc, npc_names[OWB->npc], no_gift);
        owg_out();
    } else {
        ow_new_game(mode == OWB_QUICK);
    }
    ow_back();                  /* the engine, which carries on from the
                                   game block (owgame.h) */
    return 0;
}
