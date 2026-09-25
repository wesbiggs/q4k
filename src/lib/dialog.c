#include <conio.h>
#include <c64.h>
#include <string.h>

#include <q4k/dialog.h>
#include <q4k/modem.h>
#include <q4k/panel.h>
#include <q4k/sprite.h>
#include <q4k/text.h>
#include <q4k/view.h>

#define TR_Y       3    /* transcript area */
#define TR_H      16
#define IN_Y      21
#define HINT_Y    23

/* One screen row: "> ", the text, and the cursor after it. Anything longer
   ran onto the hint line, where the cursor was never cleared again. */
#define INPUT_MAX (SCR_W - 2)
#define REPLY_MAX 180

/* Replies can take a few seconds: the host is talking to a model, and the
   line runs at 300 baud. Wait 40 seconds of silence before giving up. */
#define REPLY_TICKS 2400

/* A hint is a table lookup on the host, so it answers at once or not at
   all. Five seconds of silence is plenty, and nobody should stand at the
   bar for forty when no daemon is attached. */
#define HINT_TICKS  300

#define KEY_RETURN 0x0D
#define KEY_DEL    0x14
#define KEY_STOP   0x03   /* RUN/STOP */
#define KEY_F1     0x85

#define LEFT_INPUT 0xFF   /* read_input's "the player walked away" */

static char input[INPUT_MAX];
static char reply[REPLY_MAX];
static char seg[SCR_W + 1];

static unsigned char line_open;

/* Who is being asked, where the story stands, and how long to wait for
   silence to end, in jiffies. Statics rather than exchange()'s parameters:
   they hold for a whole conversation, and with cc65 every argument costs
   code at every call. */
static const char   *who;
static unsigned char at_stage;
static unsigned char lore;
static unsigned int  patience;

unsigned char dialog_granted;

static void frame(const char *place)
{
    unsigned char x;

    sprite_hide();
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

/* One hex digit, in ASCII: '0'-'9' then 'a'-'f'. */
static void put_nybble(unsigned char n)
{
    modem_put(n < 10 ? 0x30 + n : 0x57 + n);
}

/* Send one request and show the answer. */
static unsigned char exchange(const char *verb, const char *said)
{
    unsigned char err, i;
    char *start;

    modem_put_literal(verb);
    modem_put_literal(who);
    modem_put(0x20);
    modem_put(0x30 + at_stage);   /* Q_* never reaches ten, so one digit does */
    modem_put(0x20);
    put_nybble(lore >> 4);
    put_nybble(lore & 0x0F);
    modem_put(0x20);
    if (said) {
        modem_puts(said);        /* already ASCII: the player typed it */
    }
    modem_put_cr();

    panel_lit("...", COLOR_GRAY2);
    err = modem_getline(reply, REPLY_MAX, patience);

    /* The host prefixes every answer with a bare CR to absorb the first-byte
       sync slip on the line, so an empty read here just means "keep going". */
    while (err == MODEM_OK && reply[0] == '\0') {
        err = modem_getline(reply, REPLY_MAX, patience);
    }

    panel_undo();                /* drop the "..." placeholder */

    if (err != MODEM_OK) {
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

    /* "OK <text>" / "GIVE <g> <text>" / "ERR <text>". Numeric: start[]
       holds ASCII from the wire, and cc65 would compile 'O' to PETSCII $CF. */
    if (start[0] == 0x4F && start[1] == 0x4B && start[2] == 0x20) {
        panel_wrap(start + 3, COLOR_LIGHTGREEN);
    } else if (start[0] == 0x47 && start[1] == 0x49 && start[4] == 0x20
               && start[5] >= 0x31 && start[5] <= 0x39 && start[6] == 0x20) {
        dialog_granted = start[5] - 0x30;
        panel_wrap(start + 7, COLOR_YELLOW);
    } else if (start[0] == 0x45) {
        panel_wrap(start, COLOR_LIGHTRED);
    } else {
        panel_wrap(start, COLOR_GRAY1);
    }
    return DIALOG_OK;
}

/* Open the port the first time anyone needs it; it stays open after. */
static unsigned char line_up(void)
{
    if (!line_open) {
        if (modem_open(MODEM_300) != MODEM_OK) {
            return 0;
        }
        line_open = 1;
    }
    return 1;
}

unsigned char dialog_talk(const char *npc_id, const char *place,
                          unsigned char stage, unsigned char seed)
{
    unsigned char n;

    dialog_granted = GRANT_NONE;
    who = npc_id;
    at_stage = stage;
    lore = seed;
    patience = REPLY_TICKS;
    frame(place);

    if (!line_open) {
        panel_lit("Dialling...", COLOR_GRAY1);
        if (!line_up()) {
            panel_lit("The line is dead.", COLOR_LIGHTRED);
            cgetc();
            return DIALOG_ENOLINE;
        }
        panel_undo();
    }

    if (exchange("HI ", 0) != DIALOG_OK) {
        panel_lit("(no answer)", COLOR_LIGHTRED);
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

        if (exchange("SAY ", input) != DIALOG_OK) {
            panel_lit("(no answer)", COLOR_LIGHTRED);
        }
    }

    return DIALOG_OK;
}

unsigned char dialog_hint(const char *inn_id, unsigned char stage,
                          unsigned char seed)
{
    if (!line_up()) {
        return DIALOG_ENOLINE;
    }
    who = inn_id;
    at_stage = stage;
    lore = seed;
    patience = HINT_TICKS;
    return exchange("ALE ", 0);
}
