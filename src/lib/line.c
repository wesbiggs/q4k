/*
 * q4k -- the line to the dialogue host (networked build only).
 *
 * The part of the dialogue that is in the core rather than the talk's engine
 * (engine.h): the title screen asks whether a host is there before any engine
 * has been loaded, and the reply lands here, where the title screen's own
 * code, in the slot, is not. dialog.c does the rest.
 */

#include <q4k/dialog.h>
#include <q4k/modem.h>

/* The check at startup. A host answers PING at once, so three seconds of
   silence means nobody is there. */
#define PROBE_TICKS 180

char reply[REPLY_MAX];

unsigned char dialog_line = LINE_SILENT;

/* The host prefixes every answer with a bare CR to absorb the first-byte
 * sync slip on the line, so an empty read just means "keep going". But the
 * slip lands on that CR itself, so step over any leading non-letters too. */
char *line_answer(unsigned int ticks)
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

/* The port is opened once, here, and stays open for the whole game. */
unsigned char dialog_probe(void)
{
    char *start;

    dialog_line = LINE_SILENT;
    if (modem_open(MODEM_300) == MODEM_OK) {
        /* A spare CR first: whatever the line does to the first byte out
           lands on an empty line, which the host skips. */
        modem_put_cr();
        modem_put_literal("PING");
        modem_put_cr();
        start = line_answer(PROBE_TICKS);
        if (start) {
            /* "OK <model>" or "ERR no model". Numeric: ASCII off the wire. */
            dialog_line = start[0] == 0x45 ? LINE_NOMODEL : LINE_UP;
        }
    }
    return dialog_line;
}
