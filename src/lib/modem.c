#include <cbm.h>

#include <q4k/modem.h>
#include <q4k/text.h>

#define RS232_LFN     2
#define RS232_DEVICE  2

/* Top of BASIC memory. The KERNAL drops its two RS-232 buffers just below
   this when the port opens; cfg/c64-q4k.cfg reserves $C500-$C6FF for them. */
#define MEMSIZ (*(unsigned int *)0x0283)

/* Low byte of the jiffy clock, bumped 60 times a second by the KERNAL IRQ. */
#define JIFFY_LO (*(volatile unsigned char *)0x00A2)

/* Pointer to the KERNAL's RS-232 receive buffer; zero until the port opens. */
#define RIBUF_HI (*(volatile unsigned char *)0x00F8)

static char params[2];

void (*modem_idle)(void);

unsigned char modem_open(unsigned char baud_code)
{
    MEMSIZ = 0xC700;

    /* The "filename" for device 2 is the control byte, and optionally a
       command byte. setnam() measures with strlen, so a command byte of zero
       cannot be passed this way -- which is fine, zero is also the default:
       3-line handshake, no parity, full duplex. */
    params[0] = baud_code;
    params[1] = '\0';

    cbm_k_setlfs(RS232_LFN, RS232_DEVICE, 0);
    cbm_k_setnam(params);
    cbm_k_open();

    /* Don't trust the return value: cc65 hands back whatever the KERNAL left
       in A, and for device 2 that is not an error code -- a perfectly good
       open reports $F0. What does prove the port came up is the KERNAL
       having allocated its receive buffer. */
    return RIBUF_HI ? MODEM_OK : MODEM_ENOPORT;
}

void modem_close(void)
{
    cbm_k_close(RS232_LFN);
    cbm_k_clrch();
}

void modem_put(unsigned char c)
{
    cbm_k_ckout(RS232_LFN);
    cbm_k_bsout(c);
    cbm_k_clrch();
}

void modem_puts(const char *s)
{
    cbm_k_ckout(RS232_LFN);
    while (*s) {
        cbm_k_bsout(*s++);
    }
    cbm_k_clrch();
}

void modem_put_literal(const char *s)
{
    unsigned char a;

    cbm_k_ckout(RS232_LFN);
    while (*s) {
        a = petscii_to_ascii((unsigned char)*s++);
        if (a) {
            cbm_k_bsout(a);
        }
    }
    cbm_k_clrch();
}

void modem_put_cr(void)
{
    modem_put(0x0D);   /* numeric: cc65 maps '\r' to $0A on this target */
}

int modem_get(void)
{
    unsigned char c;

    cbm_k_chkin(RS232_LFN);
    c = cbm_k_getin();
    cbm_k_clrch();

    return c ? c : -1;
}

unsigned char modem_getline(char *buf, unsigned char max,
                            unsigned int timeout_ticks)
{
    unsigned char n = 0;
    unsigned int  idle = 0;
    unsigned char last = JIFFY_LO;
    int           c;

    for (;;) {
        c = modem_get();

        if (c < 0) {
            /* Count elapsed jiffies rather than loop iterations, so the
               timeout means the same thing however fast the host is. */
            if (JIFFY_LO != last) {
                last = JIFFY_LO;
                if (modem_idle) {
                    modem_idle();
                }
                if (++idle >= timeout_ticks) {
                    buf[n] = '\0';
                    return MODEM_ETIMEOUT;
                }
            }
            continue;
        }

        idle = 0;
        if (c == 0x0D || c == 0x0A) {   /* numeric: cc65 swaps CR and LF */
            buf[n] = '\0';
            return MODEM_OK;
        }
        if (n < max - 1) {
            buf[n++] = (char)c;
        }
    }
}
