/*
 * q4k -- screen code chart.
 *
 * Draws all 256 screen codes in a 16x16 grid so you can pick tile glyphs by
 * eye instead of guessing. Run `make shot PROG=charset` and look at the PNG.
 *
 * Screen codes are NOT PETSCII and NOT ASCII: they are direct indices into
 * whichever character set the VIC-II is pointed at, so the same code draws a
 * different glyph in the uppercase/graphics set than in the lowercase/mixed
 * set. This chart shows the lowercase set ($D018 = $17), which is what the
 * rest of q4k uses.
 */

#include <conio.h>
#include <c64.h>

#include <q4k/probe.h>

#define SCREEN_RAM ((unsigned char *)0x0400)
#define SCR_W      40

/* 8 rows of 32 codes, double-spaced -- dense enough to fit, open enough to
   actually read the glyphs in a screenshot. */
#define GRID_X     4
#define GRID_Y     6
#define ROW_CODES  32

static const char hexdigit[] = "0123456789ABCDEF";

static void put_raw(unsigned char x, unsigned char y,
                    unsigned char code, unsigned char color)
{
    unsigned int offset = (unsigned int)y * SCR_W + x;
    SCREEN_RAM[offset] = code;
    COLOR_RAM[offset]  = color;
}

int main(void)
{
    unsigned char hi, lo;

    VIC.bordercolor = COLOR_BLACK;
    VIC.bgcolor0    = COLOR_BLACK;
    VIC.addr        = 0x17;   /* lowercase/mixed charset at $1800 */

    clrscr();
    textcolor(COLOR_LIGHTBLUE);
    cputsxy(0, 1, "Q4K SCREEN CODE CHART -- LOWERCASE SET");
    textcolor(COLOR_GRAY1);
    cputsxy(0, 3, "ROW LABEL + COLUMN OFFSET = SCREEN CODE");

    /* Column headers: low nybble, repeating twice across the 32-wide row. */
    textcolor(COLOR_YELLOW);
    for (lo = 0; lo < ROW_CODES; ++lo) {
        cputcxy(GRID_X + lo, GRID_Y - 2, hexdigit[lo & 0x0F]);
    }

    for (hi = 0; hi < 8; ++hi) {
        textcolor(COLOR_YELLOW);
        cputcxy(0, GRID_Y + hi * 2, hexdigit[(hi * 2) & 0x0F]);
        cputcxy(1, GRID_Y + hi * 2, '0');

        for (lo = 0; lo < ROW_CODES; ++lo) {
            put_raw(GRID_X + lo, GRID_Y + hi * 2,
                    hi * ROW_CODES + lo, COLOR_LIGHTGREEN);
        }
    }

    probe_stage(1);
    cgetc();
    return 0;
}
