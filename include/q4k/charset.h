/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * q4k -- custom character set.
 *
 * charset_install() copies the font to RAM at $3800, for the screens of
 * text. Point the VIC at it with VIC.addr = CHARSET_D018.
 *
 * The font is embedded rather than read from the character ROM: it may not
 * be the machine's font at all, and copying from a table needs none of the
 * bank-switching that reading ROM under I/O does.
 */

#ifndef Q4K_CHARSET_H
#define Q4K_CHARSET_H

#define CHARSET_RAM   ((unsigned char *)0x3800)
#define CHARSET_BYTES 2048
/* Glyphs $00-$91 (text, and what were the tiles), and $A0, the input cursor, are all that is
   copied: see charset_install(). */
#define CHARSET_LOW    0x490
#define CHARSET_CURSOR 0x500

/* The character set the game draws with, built by tools/mkfont.py. */
extern const unsigned char font_image[CHARSET_BYTES];

/* $D018: screen matrix at $0400 (bits 7-4 = %0001),
          character data at $3800 (bits 3-1 = %111). */
#define CHARSET_D018  0x1E

/* Put the font in RAM. Call before
   touching VIC.addr, and before anything writes local[]: the font image is
   reused as the map once it has been copied, so later calls do nothing. */
void charset_install(void);

#endif /* Q4K_CHARSET_H */
