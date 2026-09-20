/*
 * q4k -- custom character set.
 *
 * charset_install() copies the ROM font to RAM at $3800 so glyphs become
 * writable; charset_define() then replaces individual codes with tile art.
 * Point the VIC at it with VIC.addr = CHARSET_D018.
 */

#ifndef Q4K_CHARSET_H
#define Q4K_CHARSET_H

#define CHARSET_RAM   ((unsigned char *)0x3800)

/* $D018: screen matrix at $0400 (bits 7-4 = %0001),
          character data at $3800 (bits 3-1 = %111). */
#define CHARSET_D018  0x1E

/* Copy the ROM font into RAM. Call once, before touching VIC.addr. */
void charset_install(void);

/* Replace one glyph. `bitmap` is 8 bytes, top row first, MSB leftmost. */
void charset_define(unsigned char code, const unsigned char *bitmap);

#endif /* Q4K_CHARSET_H */
