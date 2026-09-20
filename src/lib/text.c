#include <c64.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/text.h>
#include <q4k/view.h>

/* Every bound here is numeric on purpose.
 *
 * cc65 re-encodes character literals to PETSCII at compile time, so 'a'
 * assembles as $41 and 'A' as $C1. Written with literals, the range tests
 * below would compare ASCII input against PETSCII bounds: uppercase letters
 * fall into the lowercase branch and come out as $E1-$FA, and real lowercase
 * matches nothing and turns into spaces. Numeric constants mean what they
 * say on both sides. */
unsigned char ascii_to_screen(unsigned char c)
{
    if (c >= 0x61 && c <= 0x7A) {
        return c - 0x60;          /* a-z -> $01-$1A */
    }
    if (c >= 0x41 && c <= 0x5A) {
        return c;                 /* A-Z are already $41-$5A */
    }
    if (c >= 0x20 && c <= 0x3F) {
        return c;                 /* space, punctuation, digits: 1:1 */
    }
    if (c == 0x40) {
        return 0x00;              /* @ */
    }
    if (c == 0x5B) {
        return 0x1B;              /* [ */
    }
    if (c == 0x5D) {
        return 0x1D;              /* ] */
    }
    return 0x20;                  /* anything else we cannot draw */
}

unsigned char petscii_to_ascii(unsigned char c)
{
    if (c >= 0x41 && c <= 0x5A) {
        return c + 0x20;          /* unshifted keys -> lower case */
    }
    if (c >= 0xC1 && c <= 0xDA) {
        return c - 0x80;          /* SHIFTed keys -> upper case */
    }
    if (c >= 0x20 && c <= 0x3F) {
        return c;
    }
    return 0;
}

void text_clear(unsigned char x, unsigned char y, unsigned char len)
{
    unsigned char *scr = SCREEN_RAM + (unsigned int)y * SCR_W + x;

    while (len--) {
        *scr++ = 0x20;
    }
}

unsigned char text_put(unsigned char x, unsigned char y,
                       const char *s, unsigned char color)
{
    unsigned int   offset = (unsigned int)y * SCR_W + x;
    unsigned char *scr = SCREEN_RAM + offset;
    unsigned char *col = COLOR_RAM + offset;
    unsigned char  n = 0;

    while (*s && x + n < SCR_W) {
        *scr++ = ascii_to_screen((unsigned char)*s++);
        *col++ = color;
        ++n;
    }
    return n;
}

char *str_lit(char *dst, const char *literal)
{
    unsigned char c;

    while (*literal) {
        c = petscii_to_ascii((unsigned char)*literal++);
        if (c) {
            *dst++ = (char)c;
        }
    }
    *dst = '\0';
    return dst;
}

char *str_asc(char *dst, const char *ascii)
{
    while (*ascii) {
        *dst++ = *ascii++;
    }
    *dst = '\0';
    return dst;
}

char *str_num(char *dst, unsigned int value)
{
    /* Decimal digits are $30-$39 in both encodings, so utoa's output needs
       no conversion. */
    utoa(value, dst, 10);
    return dst + strlen(dst);
}

/* Pad with spaces until the string is `to_width` characters long, counting
   from `start`. Useful for lining up columns. */
char *str_pad(char *dst, unsigned char to_width, const char *start)
{
    while ((unsigned char)(dst - start) < to_width) {
        *dst++ = 0x20;
    }
    *dst = '\0';
    return dst;
}
