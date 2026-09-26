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

/* The check at startup. A host answers PING at once, so three seconds of
   silence means nobody is there. */
#define PROBE_TICKS 180

/* A hint is a table lookup on the host, so it answers at once or not at
   all. Five seconds of silence is plenty, and nobody should stand at the
   bar for forty when no daemon is attached. */
#define HINT_TICKS  300

#define KEY_RETURN 0x0D
#define KEY_DEL    0x14
#define KEY_STOP   0x03   /* RUN/STOP */
#define KEY_F1     0x85

#define LEFT_INPUT 0xFF   /* read_input's "the player walked away" */

/* exchange()'s "the NPC has said their last": the host answered BYE. */
#define ENDED      3

/* The "..." placeholder pulses through the greys while a reply is on its
   way, one step every PULSE_JIFFIES, each dot a step behind the one before,
   so a highlight runs along them. A static "..." looked like a hang. */
#define PULSE_JIFFIES 5
#define PULSE_STEPS   6

static char input[INPUT_MAX];
static char reply[REPLY_MAX];
static char seg[SCR_W + 1];

static unsigned char line_open;

unsigned char dialog_line = LINE_SILENT;

/* Who is being asked, where the story stands, and how long to wait for
   silence to end, in jiffies. Statics rather than exchange()'s parameters:
   they hold for a whole conversation, and with cc65 every argument costs
   code at every call. */
static const char   *who;
static unsigned char at_stage;
static unsigned char lore;
static unsigned int  patience;
static const char *(*gift)(unsigned char granted);

static const unsigned char pulse_ramp[PULSE_STEPS] = {
    COLOR_GRAY1, COLOR_GRAY2, COLOR_GRAY3, COLOR_WHITE, COLOR_GRAY3, COLOR_GRAY2
};
static unsigned int  dots;        /* where the "..." sits in screen RAM */
static unsigned char pulse_wait;
static unsigned char pulse_step;

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

/* modem_idle while a reply is awaited: move the highlight one step on. */
static void pulse_dots(void)
{
    unsigned char i, k;

    if (++pulse_wait < PULSE_JIFFIES) {
        return;
    }
    pulse_wait = 0;
    if (++pulse_step == PULSE_STEPS) {
        pulse_step = 0;
    }
    k = pulse_step;
    for (i = 0; i < 3; ++i) {
        COLOR_RAM[dots + i] = pulse_ramp[k];
        k = k ? k - 1 : PULSE_STEPS - 1;
    }
}

/* The conversation is over, by the NPC's choice or the line's: hold the
   last words on screen until the player has read them. */
static void farewell(void)
{
    text_clear(0, IN_Y, SCR_W);
    text_clear(0, HINT_Y, SCR_W);
    textcolor(COLOR_GRAY2);
    cputsxy(0, HINT_Y, "Press any key to continue");
    cgetc();
}

/* One hex digit, in ASCII: '0'-'9' then 'a'-'f'. */
static void put_nybble(unsigned char n)
{
    modem_put(n < 10 ? 0x30 + n : 0x57 + n);
}

/* The host's next line, from its verb on; 0 if `ticks` jiffies pass in
   silence.
 *
 * The host prefixes every answer with a bare CR to absorb the first-byte
 * sync slip on the line, so an empty read just means "keep going". But the
 * slip lands on that CR itself, so step over any leading non-letters too. */
static char *answer(unsigned int ticks)
{
    unsigned char err, i;
    char *start;

    err = modem_getline(reply, REPLY_MAX, ticks);
    while (err == MODEM_OK && reply[0] == '\0') {
        err = modem_getline(reply, REPLY_MAX, ticks);
    }
    if (err != MODEM_OK) {
        return 0;
    }
    start = reply;
    for (i = 0; i < 4 && *start && (*start < 0x41 || *start > 0x5A); ++i) {
        ++start;
    }
    return start;
}

/* Send one request and show the answer. DIALOG_OK, DIALOG_ESILENT, or
   ENDED if the NPC has closed the conversation. */
static unsigned char exchange(const char *verb, const char *said)
{
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

    panel_lit("...", COLOR_GRAY1);
    dots = (unsigned int)panel_row() * SCR_W;
    pulse_wait = 0;
    pulse_step = 0;
    modem_idle = pulse_dots;
    start = answer(patience);
    modem_idle = 0;
    panel_undo();                /* drop the "..." placeholder */

    if (!start) {
        return DIALOG_ESILENT;
    }

    /* "OK <text>" / "GIVE <g> <text>" / "BYE <text>" / "ERR <text>".
       Numeric: start[] holds ASCII from the wire, and cc65 would compile
       'O' to PETSCII $CF. */
    if (start[0] == 0x4F && start[1] == 0x4B && start[2] == 0x20) {
        panel_wrap(start + 3, COLOR_LIGHTGREEN);
    } else if (start[0] == 0x47 && start[1] == 0x49 && start[4] == 0x20
               && start[5] >= 0x31 && start[5] <= 0x39 && start[6] == 0x20) {
        dialog_granted = start[5] - 0x30;
        panel_wrap(start + 7, COLOR_LIGHTGREEN);
    } else if (start[0] == 0x42 && start[1] == 0x59 && start[2] == 0x45
               && start[3] == 0x20) {
        panel_wrap(start + 4, COLOR_LIGHTGREEN);
        return ENDED;
    } else if (start[0] == 0x45) {
        panel_wrap(start, COLOR_LIGHTRED);
    } else {
        panel_wrap(start, COLOR_GRAY1);
    }
    return DIALOG_OK;
}

/* One turn of a conversation: the NPC's answer, then anything it has earned
   the player, announced in the transcript while the player is still there. */
static unsigned char turn(const char *verb, const char *said)
{
    unsigned char r;
    const char *note;

    r = exchange(verb, said);
    if (r != DIALOG_ESILENT) {
        note = gift(dialog_granted);
        if (note) {
            panel_lit(note, COLOR_YELLOW);
        }
    }
    return r;
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

unsigned char dialog_probe(void)
{
    char *start;

    dialog_line = LINE_SILENT;
    if (line_up()) {
        /* A spare CR first: whatever the line does to the first byte out
           lands on an empty line, which the host skips. */
        modem_put_cr();
        modem_put_literal("PING");
        modem_put_cr();
        start = answer(PROBE_TICKS);
        if (start) {
            /* "OK <model>" or "ERR no model". Numeric: ASCII off the wire. */
            dialog_line = start[0] == 0x45 ? LINE_NOMODEL : LINE_UP;
        }
    }
    return dialog_line;
}

unsigned char dialog_talk(const char *npc_id, const char *place,
                          unsigned char stage, unsigned char seed,
                          const char *(*given)(unsigned char granted))
{
    unsigned char n, r;
    const char *note;

    /* Nobody to talk through -- the title screen said so -- so only what
       the quest needs changes hands, and it is told on the map. */
    if (dialog_line != LINE_UP) {
        note = given(GRANT_OPEN);
        if (!note) {
            strcpy(seg, place);
            strcat(seg, " has nothing for you.");
            note = seg;
        }
        view_message(note);
        return DIALOG_OK;
    }

    dialog_granted = GRANT_NONE;
    who = npc_id;
    at_stage = stage;
    lore = seed;
    patience = REPLY_TICKS;
    gift = given;
    frame(place);

    r = turn("HI ", 0);
    if (r == DIALOG_ESILENT) {
        /* The host was there at startup and has gone. Say so this once;
           from here on the game plays without conversations. */
        dialog_line = LINE_SILENT;
        panel_lit("(no answer)", COLOR_LIGHTRED);
        farewell();
        return DIALOG_ESILENT;
    }

    while (r != ENDED) {
        n = read_input();
        if (n == LEFT_INPUT) {
            return DIALOG_OK;
        }
        if (n == 0) {
            continue;
        }

        text_clear(0, IN_Y, SCR_W);
        str_asc(str_lit(seg, "> "), input);
        panel_wrap(seg, COLOR_WHITE);

        r = turn("SAY ", input);
        if (r == DIALOG_ESILENT) {
            panel_lit("(no answer)", COLOR_LIGHTRED);
        }
    }

    farewell();
    return DIALOG_OK;
}

unsigned char dialog_hint(const char *inn_id, unsigned char stage,
                          unsigned char seed)
{
    if (dialog_line != LINE_UP) {
        return DIALOG_ENOLINE;
    }
    who = inn_id;
    at_stage = stage;
    lore = seed;
    patience = HINT_TICKS;
    return exchange("ALE ", 0);
}
