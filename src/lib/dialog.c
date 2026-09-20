#include <conio.h>
#include <c64.h>
#include <string.h>

#include <q4k/dialog.h>
#include <q4k/modem.h>
#include <q4k/text.h>
#include <q4k/view.h>

#define TR_Y       3    /* transcript area */
#define TR_H      16
#define IN_Y      21
#define HINT_Y    23

#define INPUT_MAX 56
#define REPLY_MAX 180

/* Replies can take a few seconds: the host is talking to a model, and the
   line runs at 1200 baud. Wait 40 seconds of silence before giving up. */
#define REPLY_TICKS 2400

#define KEY_RETURN 0x0D
#define KEY_DEL    0x14
#define KEY_STOP   0x03   /* RUN/STOP */
#define KEY_F1     0x85

#define LEFT_INPUT 0xFF   /* read_input's "the player walked away" */

static char input[INPUT_MAX];
static char reply[REPLY_MAX];
static char seg[SCR_W + 1];

static unsigned char tr_next;
static unsigned char line_open;

static void scroll_transcript(void)
{
    unsigned char y;
    unsigned int  dst;

    for (y = 0; y < TR_H - 1; ++y) {
        dst = (unsigned int)(TR_Y + y) * SCR_W;
        memcpy(SCREEN_RAM + dst, SCREEN_RAM + dst + SCR_W, SCR_W);
        memcpy(COLOR_RAM + dst, COLOR_RAM + dst + SCR_W, SCR_W);
    }
    text_clear(0, TR_Y + TR_H - 1, SCR_W);
}

static void say_line(const char *s, unsigned char color)
{
    if (tr_next >= TR_H) {
        scroll_transcript();
        tr_next = TR_H - 1;
    }
    text_clear(0, TR_Y + tr_next, SCR_W);
    text_put(0, TR_Y + tr_next, s, color);
    ++tr_next;
}

/* Append ASCII text, wrapped at word boundaries. */
static void say_wrapped(const char *s, unsigned char color)
{
    unsigned char i, brk;

    while (*s) {
        brk = 0;
        for (i = 0; i < SCR_W && s[i]; ++i) {
            if (s[i] == 0x20) {
                brk = i;
            }
        }
        if (s[i] == '\0') {
            brk = i;            /* the rest fits on one line */
        } else if (brk == 0) {
            brk = SCR_W;        /* one very long word: hard-split */
        }

        memcpy(seg, s, brk);
        seg[brk] = '\0';
        say_line(seg, color);

        s += brk;
        while (*s == 0x20) {
            ++s;
        }
    }
}

static void frame(const char *place)
{
    unsigned char x;

    clrscr();
    tr_next = 0;

    textcolor(COLOR_YELLOW);
    cputsxy(0, 0, place);

    textcolor(COLOR_GRAY1);
    chlinexy(0, 1, SCR_W);
    chlinexy(0, IN_Y - 1, SCR_W);

    textcolor(COLOR_GRAY2);
    cputsxy(0, HINT_Y, "RETURN SENDS   RUN/STOP LEAVES");
    (void)x;
}

/* Read a line the player types. Returns its length, or LEFT_INPUT if they
   chose to walk away. */
static unsigned char read_input(void)
{
    unsigned char n = 0;
    unsigned char key, ch;

    for (;;) {
        text_clear(0, IN_Y, SCR_W);
        text_put(0, IN_Y, "> ", COLOR_GRAY1);
        input[n] = '\0';
        text_put(2, IN_Y, input, COLOR_WHITE);
        /* Block cursor, drawn as a reversed space. */
        SCREEN_RAM[(unsigned int)IN_Y * SCR_W + 2 + n] = 0xA0;
        COLOR_RAM[(unsigned int)IN_Y * SCR_W + 2 + n]  = COLOR_GRAY2;

        key = cgetc();

        if (key == KEY_RETURN) {
            input[n] = '\0';
            return n;
        }
        if (key == KEY_STOP || key == KEY_F1) {
            return LEFT_INPUT;
        }
        if (key == KEY_DEL) {
            if (n) {
                --n;
            }
            continue;
        }

        ch = petscii_to_ascii(key);
        if (ch && n < INPUT_MAX - 1) {
            input[n++] = ch;
        }
    }
}

/* Send one request and show the answer. */
static unsigned char exchange(const char *verb, const char *npc_id,
                              const char *said)
{
    unsigned char err, i;
    char *start;

    modem_put_literal(verb);
    modem_put_literal(npc_id);
    modem_put(0x20);
    if (said) {
        modem_puts(said);        /* already ASCII: the player typed it */
    }
    modem_put_cr();

    say_line("...", COLOR_GRAY2);
    err = modem_getline(reply, REPLY_MAX, REPLY_TICKS);

    /* The host prefixes every answer with a bare CR to absorb the first-byte
       sync slip on the line, so an empty read here just means "keep going". */
    while (err == MODEM_OK && reply[0] == '\0') {
        err = modem_getline(reply, REPLY_MAX, REPLY_TICKS);
    }

    --tr_next;                   /* drop the "..." placeholder */

    if (err != MODEM_OK) {
        say_line("(no answer)", COLOR_LIGHTRED);
        return DIALOG_ESILENT;
    }

    /* The first byte the KERNAL's receiver sees after the line comes up is
       reliably corrupted by clock slip -- that is what the host's leading CR
       is for, but the slip lands on the CR itself. Step over any leading
       non-letters before parsing the verb. */
    start = reply;
    for (i = 0; i < 4 && *start && (*start < 0x41 || *start > 0x5A); ++i) {
        ++start;
    }

    /* "OK <text>" / "ERR <text>". Numeric: start[] holds ASCII from the wire,
       and cc65 would compile 'O' to PETSCII $CF. */
    if (start[0] == 0x4F && start[1] == 0x4B && start[2] == 0x20) {
        say_wrapped(start + 3, COLOR_LIGHTGREEN);
    } else if (start[0] == 0x45) {
        say_wrapped(start, COLOR_LIGHTRED);
    } else {
        say_wrapped(start, COLOR_GRAY1);
    }
    return DIALOG_OK;
}

unsigned char dialog_talk(const char *npc_id, const char *place)
{
    unsigned char n;

    frame(place);

    if (!line_open) {
        say_line("DIALLING...", COLOR_GRAY1);
        if (modem_open(MODEM_300) != MODEM_OK) {
            say_line("THE LINE IS DEAD.", COLOR_LIGHTRED);
            cgetc();
            return DIALOG_ENOLINE;
        }
        line_open = 1;
        --tr_next;
    }

    if (exchange("HI ", npc_id, 0) != DIALOG_OK) {
        cgetc();
        return DIALOG_ESILENT;
    }

    for (;;) {
        n = read_input();
        if (n == LEFT_INPUT) {
            break;
        }
        if (n == 0) {
            continue;
        }

        text_clear(0, IN_Y, SCR_W);
        say_line("", COLOR_WHITE);
        seg[0] = 0x3E;   /* '>' in ASCII */
        seg[1] = 0x20;
        memcpy(seg + 2, input, n + 1);
        say_wrapped(seg, COLOR_WHITE);

        exchange("SAY ", npc_id, input);
    }

    return DIALOG_OK;
}
