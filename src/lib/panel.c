#include <c64.h>
#include <string.h>

#include <q4k/panel.h>
#include <q4k/text.h>
#include <q4k/view.h>

static unsigned char top;
static unsigned char rows;
static unsigned char next;
static char seg[SCR_W + 1];

void panel_begin(unsigned char first, unsigned char count)
{
    unsigned char y;

    top = first;
    rows = count;
    next = 0;
    for (y = 0; y < rows; ++y) {
        text_clear(0, top + y, SCR_W);
    }
}

static void scroll_up(void)
{
    unsigned char y;
    unsigned int  dst;

    for (y = 0; y < rows - 1; ++y) {
        dst = (unsigned int)(top + y) * SCR_W;
        memcpy(SCREEN_RAM + dst, SCREEN_RAM + dst + SCR_W, SCR_W);
        memcpy(COLOR_RAM + dst, COLOR_RAM + dst + SCR_W, SCR_W);
    }
    text_clear(0, top + rows - 1, SCR_W);
}

void panel_line(const char *ascii, unsigned char color)
{
    if (next >= rows) {
        scroll_up();
        next = rows - 1;
    }
    text_clear(0, top + next, SCR_W);
    text_put(0, top + next, ascii, color);
    ++next;
}

void panel_lit(const char *literal, unsigned char color)
{
    str_lit(seg, literal);
    panel_line(seg, color);
}

void panel_undo(void)
{
    if (next) {
        --next;
    }
}

void panel_wrap(const char *ascii, unsigned char color)
{
    unsigned char i, brk;

    while (*ascii) {
        brk = 0;
        for (i = 0; i < SCR_W && ascii[i]; ++i) {
            if (ascii[i] == 0x20) {
                brk = i;
            }
        }
        if (ascii[i] == '\0') {
            brk = i;              /* the rest fits on one line */
        } else if (brk == 0) {
            brk = SCR_W;          /* one very long word: hard-split */
        }

        memcpy(seg, ascii, brk);
        seg[brk] = '\0';
        panel_line(seg, color);

        ascii += brk;
        while (*ascii == 0x20) {
            ++ascii;
        }
    }
}
