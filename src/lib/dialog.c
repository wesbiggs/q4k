#include <conio.h>
#include <c64.h>
#include <string.h>

#include <q4k/dialog.h>
#include <q4k/modem.h>
#include <q4k/panel.h>
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

static unsigned char line_open;

static void frame(const char *place)
{
    unsigned char x;

    clrscr();
    panel_begin(TR_Y, TR_H);

    textcolor(COLOR_YELLOW);
    cputsxy(0, 0, place);

    textcolor(COLOR_GRAY1);
    chlinexy(0, 1, SCR_W);
    chlinexy(0, IN_Y - 1, SCR_W);

    textcolor(COLOR_GRAY2);
    cputsxy(0, HINT_Y, "RETURN sends   RUN/STOP leaves");
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
        str_lit(seg, "> ");
        text_put(0, IN_Y, seg, COLOR_GRAY1);
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
                              unsigned char stage, const char *said)
{
    unsigned char err, i;
    char *start;

    modem_put_literal(verb);
    modem_put_literal(npc_id);
    modem_put(0x20);
    modem_put(0x30 + stage);   /* Q_* never reaches ten, so one digit does */
    modem_put(0x20);
    if (said) {
        modem_puts(said);        /* already ASCII: the player typed it */
    }
    modem_put_cr();

    panel_lit("...", COLOR_GRAY2);
    err = modem_getline(reply, REPLY_MAX, REPLY_TICKS);

    /* The host prefixes every answer with a bare CR to absorb the first-byte
       sync slip on the line, so an empty read here just means "keep going". */
    while (err == MODEM_OK && reply[0] == '\0') {
        err = modem_getline(reply, REPLY_MAX, REPLY_TICKS);
    }

    panel_undo();                /* drop the "..." placeholder */

    if (err != MODEM_OK) {
        panel_lit("(no answer)", COLOR_LIGHTRED);
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
        panel_wrap(start + 3, COLOR_LIGHTGREEN);
    } else if (start[0] == 0x45) {
        panel_wrap(start, COLOR_LIGHTRED);
    } else {
        panel_wrap(start, COLOR_GRAY1);
    }
    return DIALOG_OK;
}

unsigned char dialog_talk(const char *npc_id, const char *place,
                          unsigned char stage)
{
    unsigned char n;

    frame(place);

    if (!line_open) {
        panel_lit("Dialling...", COLOR_GRAY1);
        if (modem_open(MODEM_300) != MODEM_OK) {
            panel_lit("The line is dead.", COLOR_LIGHTRED);
            cgetc();
            return DIALOG_ENOLINE;
        }
        line_open = 1;
        panel_undo();
    }

    if (exchange("HI ", npc_id, stage, 0) != DIALOG_OK) {
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
        str_asc(str_lit(seg, "> "), input);
        panel_wrap(seg, COLOR_WHITE);

        exchange("SAY ", npc_id, stage, input);
    }

    return DIALOG_OK;
}
