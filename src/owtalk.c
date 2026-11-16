/*
 * Quest for the King -- a conversation in the open world.
 *
 * The smooth-scrolling prototype's engine (src/ow/engine.c) has no room for
 * the talk, so when the hero walks into a townsperson it goes (the town's
 * overlay, src/ow/town.c, sees to it): its hand-off
 * block (ow.h's OWB) says who, and in which town, and this program is loaded
 * over it (ow_run). It is the offline talk (src/lib/talk.c, which reads each
 * person's lines from the disk, one file per person), with the character
 * set the game draws its text in, and nothing of the game's quest: the
 * prototype has none, so nobody hands anything over.
 *
 * When the talk ends the engine is loaded back, with OWB_BACK in the block:
 * it starts in the same town, the hero where they stood. Everything the
 * engine keeps is rebuilt from the disk on the way (OW-DATA, which this
 * program writes over, is loaded again here), or is still in memory: the
 * vault under the I/O area and the music under the KERNAL are not reached.
 * Linked like the test programs, with cfg/c64-flat.cfg (one load).
 */

#include <cbm.h>
#include <conio.h>
#include <c64.h>
#include <peekpoke.h>

#include <q4k/charset.h>
#include <q4k/dialog.h>
#include <q4k/ow.h>
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

/* The file the engine needs back: where OW-DATA was, the tile sets and the
   towns' maps. The talk wrote over it, so it is loaded again. */
static void need_data(void)
{
    /* RUN/STOP, which left the talk, may still be down, or still flagged in
       $91: the KERNAL's LOAD gives up when it sees it, with no drive error,
       so wait for it to come up (row 7 of the keyboard, STOP's bit) and set
       the flag to released ($FF) before loading. */
    POKE(0xDC00, 0x7F);
    while (!(PEEK(0xDC01) & 0x80)) {
    }
    POKE(0xDC00, 0xFF);
    POKE(0x91, 0xFF);           /* released: $7F is STOP held down */

    while (!cbm_load("ow-data", 8, 0)) {
        cputs("Put the disk back and press a key.\r\n");
        cgetc();
    }
}

int main(void)
{
    if (OWB->mode != OWB_TALK) {
        cputs("Load OPENWORLD: it builds the world.");
        for (;;) {
        }
    }

    tiles_install();            /* the character set, before any text */
    VIC.bordercolor = COLOR_BLACK;
    VIC.bgcolor0    = COLOR_BLACK;
    VIC.addr        = CHARSET_D018;
    clrscr();
    while (kbhit()) {
        cgetc();
    }
    VIC.ctrl1 |= 0x10;          /* set up: the screen on for the talk */

    dialog_talk(OWB->npc, npc_names[OWB->npc], no_gift);

    /* The engine comes back for the same town (src/ow/engine.c's
       back_to_town): the data first, then the machine as sm_quit() leaves
       it for a program to be loaded over, which the engine reads. */
    OWB->mode = OWB_BACK;
    need_data();
    POKEW(0x0314, 0xEA31);      /* the KERNAL's interrupt again */
    VIC.imr = 0;
    VIC.spr_ena = 0;
    POKE(0xDC0D, 0x81);         /* the CIA's timer interrupt again */
    POKE(0xDD00, PEEK(0xDD00) | 0x03);  /* VIC bank 0 */
    VIC.addr = 0x17;            /* the text screen, the lowercase set */
    clrscr();                   /* and clear before the engine loads */

    /* Black while the engine starts; it sets its own green over that, and
       turns the screen on only once the town is drawn. */
    VIC.bgcolor0 = COLOR_BLACK;
        ow_run(OW->reu == OW_REU_OK ? "ow-reu" : "ow-cpu");
    return 0;
}
