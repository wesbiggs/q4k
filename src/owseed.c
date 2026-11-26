/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Quest for the King -- the open world's seed, asked for: a program of its
 * own.
 *
 * SHIFT+F1 (F2) on the splash screen is a new game whose seed the player
 * types. The splash sets OW_ASKSEED (q4k/ow.h), and the bootstrap
 * (src/openworld.c) builds the lands as for F1, from the seed the splash
 * made, then loads this instead of owparty. It asks for the seed (0-255).
 * Another one has the bootstrap loaded again to build that world, as N in
 * an engine does, and it runs owparty once it has; RETURN alone, RUN/STOP or
 * the same seed runs owparty now. It is a program of its own because
 * neither the bootstrap nor owparty has the room.
 *
 * Linked with cfg/c64-tool.cfg, as owparty is. On the text screen, in the
 * KERNAL's lowercase set, which conio writes for.
 */

#include <conio.h>
#include <c64.h>

#include <q4k/ow.h>

int main(void)
{
    unsigned char k, n = 0;
    unsigned int v = 0;

    OW_ASKSEED = 0;
    VIC.bordercolor = COLOR_BLACK;
    VIC.bgcolor0    = COLOR_BLACK;
    VIC.addr        = 0x17;
    *(unsigned char *)0x0291 = 0x80;    /* C=+Shift keeps this set */
    clrscr();
    textcolor(COLOR_YELLOW);
    cputsxy(11, 10, "Quest for the King");
    textcolor(COLOR_GRAY3);
    cputsxy(8, 13, "The world's seed (0-255):");
    cputsxy(8, 15, "RETURN alone for this one");
    gotoxy(34, 13);
    cursor(1);
    for (;;) {
        k = cgetc();
        if (k == 0x0D || k == 0x03) {           /* RETURN, RUN/STOP */
            break;
        }
        if (k == 0x14 && n) {                   /* DEL */
            --n;
            v /= 10;
            gotoxy(34 + n, 13);
            cputc(' ');
            gotoxy(34 + n, 13);
        } else if (k >= 0x30 && k <= 0x39 && n < 3) {
            k -= 0x30;                  /* (PETSCII digits are ASCII's) */
            if (v * 10 + k < 256) {
                v = v * 10 + k;
                cputc(k + 0x30);
                ++n;
            }
        }
    }
    cursor(0);
    clrscr();
    if (n && k != 0x03 && v != OW->seed) {
        OW->seed  = (unsigned char)v;
        OWB->mode = OWB_NONE;           /* (that world, then owparty) */
        ow_run("openworld");
    }
    ow_run("owparty");                  /* OWB_NEW, as the bootstrap left */
    return 0;
}
