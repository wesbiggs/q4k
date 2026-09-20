/*
 * q4k -- ASCII text straight to screen RAM.
 *
 * conio's cputs() takes PETSCII and converts it, which is wrong twice over for
 * text that arrives from the host: the bytes are ASCII, and the game runs a
 * custom charset where $41-$5A are A-Z and $01-$1A are a-z. These routines
 * write screen codes directly, so what the host sent is what appears.
 */

#ifndef Q4K_TEXT_H
#define Q4K_TEXT_H

/* One ASCII byte as a screen code in the lowercase/mixed charset. */
unsigned char ascii_to_screen(unsigned char c);

/* One PETSCII byte (as cgetc returns it) as ASCII, or 0 if unprintable. */
unsigned char petscii_to_ascii(unsigned char c);

void text_clear(unsigned char x, unsigned char y, unsigned char len);

/* Write an ASCII string. Returns how many characters landed. */
unsigned char text_put(unsigned char x, unsigned char y,
                       const char *s, unsigned char color);

#endif /* Q4K_TEXT_H */
