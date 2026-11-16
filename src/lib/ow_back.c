/*
 * The way back to the open world's engine, for its other programs (the
 * party's making, the talk, the shops ...: q4k/ow.h). They are linked flat,
 * over the engine and over OW-DATA, so the data is loaded again first; then
 * the machine is left as the engine's sm_quit() leaves it for a program to
 * be loaded over, which is how the engine expects to find it, and the
 * engine is loaded. It carries on from the game block (owgame.h): a program
 * that changed the game has put it back there (owg_out()) first.
 */

#include <cbm.h>
#include <conio.h>
#include <c64.h>
#include <peekpoke.h>

#include <q4k/ow.h>

void ow_back(void)
{
    /* RUN/STOP, which may have left the talk, may still be down, or still
       flagged in $91: the KERNAL's LOAD gives up when it sees it, with no
       drive error, so wait for it to come up (row 7 of the keyboard, STOP's
       bit) and set the flag to released ($FF) before loading. */
    POKE(0xDC00, 0x7F);
    while (!(PEEK(0xDC01) & 0x80)) {
    }
    POKE(0xDC00, 0xFF);
    POKE(0x91, 0xFF);           /* released: $7F is STOP held down */

    OWB->mode = OWB_BACK;
    /* Where OW-DATA was: the tile sets and the towns' maps. */
    while (!cbm_load("ow-data", 8, 0)) {
        cputs("Put the disk back and press a key.\r\n");
        cgetc();
    }
    POKEW(0x0314, 0xEA31);      /* the KERNAL's interrupt again */
    VIC.imr = 0;
    VIC.spr_ena = 0;
    POKE(0xDC0D, 0x81);         /* the CIA's timer interrupt again */
    POKE(0xDD00, PEEK(0xDD00) | 0x03);  /* VIC bank 0 */
    VIC.addr = 0x17;            /* the text screen, the lowercase set */
    clrscr();                   /* and clear before the engine loads */

    /* Black while the engine starts; it sets its own colours over that,
       and turns the screen on only once its area is drawn. */
    VIC.bordercolor = COLOR_BLACK;
    VIC.bgcolor0 = COLOR_BLACK;
    ow_run(OW->reu == OW_REU_OK ? "ow-reu" : "ow-cpu");
}
