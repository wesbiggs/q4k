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

/* Building ASCII strings.
 *
 * cc65 encodes C string literals as PETSCII, so a literal cannot be handed
 * to anything that expects ASCII -- it comes out as blanks. These append to
 * `dst` and return the new end, so they chain:
 *
 *     end = str_lit(buf, "HP ");
 *     end = str_num(end, hero.hp);
 *     end = str_lit(end, " OF ");
 *
 * str_lit converts as it copies; str_asc is for text that is already ASCII,
 * such as anything that arrived from the host. */
char *str_lit(char *dst, const char *literal);
char *str_asc(char *dst, const char *ascii);
char *str_num(char *dst, unsigned int value);
char *str_pad(char *dst, unsigned char to_width, const char *start);

/* Write an ASCII string. Returns how many characters landed. */
unsigned char text_put(unsigned char x, unsigned char y,
                       const char *s, unsigned char color);

#endif /* Q4K_TEXT_H */
