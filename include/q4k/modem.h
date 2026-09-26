/*
 * q4k -- the modem port.
 *
 * Plain KERNAL RS-232 on the user port: device 2, the same path a 300-baud
 * modem would have used in 1983. Under VICE the far end is a TCP socket on
 * the host, so the C64 dials into a daemon running on the same machine.
 *
 * The KERNAL's RS-232 is NMI-driven bit-banging, so it is slow and it steals
 * cycles while a byte is in flight -- fine for line-at-a-time text, which is
 * all the dialogue protocol needs.
 */

#ifndef Q4K_MODEM_H
#define Q4K_MODEM_H

/* Baud codes for the KERNAL control register (low nybble). */
#define MODEM_300   0x06
#define MODEM_1200  0x08
#define MODEM_2400  0x0A

#define MODEM_OK        0
#define MODEM_ETIMEOUT  1
#define MODEM_EOVERRUN  2
#define MODEM_ENOPORT   3

/* Open the port. Returns MODEM_OK or MODEM_ENOPORT. */
unsigned char modem_open(unsigned char baud_code);
void modem_close(void);

/* Send one raw byte, or a buffer that already holds ASCII. */
void modem_put(unsigned char c);
void modem_puts(const char *s);

/* Send a C string literal as ASCII.
 *
 * cc65 re-encodes literals to PETSCII at compile time -- "SAY" is assembled
 * as $D3 $C1 $D9 -- so a literal cannot go on the wire untouched. This
 * converts on the way out; modem_puts() is for buffers that are already
 * ASCII, such as anything the player typed. */
void modem_put_literal(const char *s);

/* Terminate the request. */
void modem_put_cr(void);

/* Next byte, or -1 if nothing is waiting. A genuine NUL reads as -1 too,
   which the line protocol never sends. */
int modem_get(void);

/* Read up to `max`-1 bytes into `buf` until CR or LF.
   Returns MODEM_OK, or MODEM_ETIMEOUT after roughly `timeout_ticks`/60
   seconds with no traffic at all. */
unsigned char modem_getline(char *buf, unsigned char max,
                            unsigned int timeout_ticks);

/* If set, called once a jiffy while modem_getline() waits on silence, so
   the screen can show that something is still coming. */
extern void (*modem_idle)(void);

#endif /* Q4K_MODEM_H */
