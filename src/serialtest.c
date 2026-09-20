/*
 * q4k -- modem smoke test.
 *
 * Opens the user port at 1200 baud, sends one line, shows whatever comes
 * back -- as text and as raw hex, because the interesting question is what
 * bytes actually reach the wire. PETSCII is not ASCII, and cc65 re-encodes
 * string literals at compile time, so the hex view is the one that tells the
 * truth.
 *
 *   host/echod.py &
 *   tools/screendump.py dist/serialtest.prg --rs232 25232 --stage 3
 */

#include <conio.h>
#include <c64.h>

#include <q4k/modem.h>
#include <q4k/probe.h>

static char line[64];

static const char hexdigit[] = "0123456789ABCDEF";

static void show_hex(unsigned char y, const char *label,
                     const char *data, unsigned char count)
{
    unsigned char i;

    cputsxy(0, y, label);
    for (i = 0; i < count; ++i) {
        cputcxy(8 + i * 3, y, hexdigit[(unsigned char)data[i] >> 4]);
        cputcxy(9 + i * 3, y, hexdigit[(unsigned char)data[i] & 0x0F]);
    }
}

int main(void)
{
    unsigned char err, n;

    VIC.bordercolor = COLOR_BLACK;
    VIC.bgcolor0    = COLOR_BLACK;
    VIC.addr        = 0x17;      /* ROM lowercase set: this tool is all text */

    clrscr();
    textcolor(COLOR_LIGHTBLUE);
    cputsxy(0, 0, "Q4K MODEM SMOKE TEST");

    textcolor(COLOR_GRAY1);
    cputsxy(0, 2, "OPENING USER PORT AT 1200 BAUD");

    err = modem_open(MODEM_1200);
    if (err) {
        textcolor(COLOR_LIGHTRED);
        cputsxy(0, 3, "OPEN FAILED, KERNAL ERROR $");
        cputcxy(27, 3, hexdigit[err >> 4]);
        cputcxy(28, 3, hexdigit[err & 0x0F]);
        PROBE_SCRATCH[0] = err;
        PROBE_SCRATCH[1] = *(unsigned char *)0x0293;   /* RS-232 control  */
        PROBE_SCRATCH[2] = *(unsigned char *)0x0294;   /* RS-232 command  */
        PROBE_SCRATCH[3] = *(unsigned char *)0x00F7;   /* RX buffer ptr lo */
        PROBE_SCRATCH[4] = *(unsigned char *)0x00F8;   /* RX buffer ptr hi */
        probe_stage(9);
        cgetc();
        return 1;
    }

    textcolor(COLOR_LIGHTGREEN);
    cputsxy(0, 3, "PORT OPEN");
    probe_stage(1);

    textcolor(COLOR_GRAY1);
    cputsxy(0, 5, "SENT: PING");
    modem_put_literal("PING");
    modem_put_cr();
    probe_stage(2);

    cputsxy(0, 7, "WAITING...");
    err = modem_getline(line, sizeof(line), 600);   /* ten seconds */

    if (err == MODEM_ETIMEOUT) {
        textcolor(COLOR_LIGHTRED);
        cputsxy(0, 7, "TIMED OUT  ");
        probe_stage(8);
    } else {
        textcolor(COLOR_YELLOW);
        cputsxy(0, 7, "REPLY:     ");
        cputsxy(0, 8, line);

        for (n = 0; n < 8 && line[n]; ++n) {
            PROBE_SCRATCH[n] = (unsigned char)line[n];
        }
        textcolor(COLOR_GRAY2);
        show_hex(10, "HEX:", line, n);
        probe_stage(3);
    }

    modem_close();
    cgetc();
    return 0;
}
