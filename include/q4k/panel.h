/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * A scrolling text panel, written straight to screen RAM in ASCII.
 *
 * The dialogue screen and the battle screen are the same thing underneath:
 * a titled region that text is appended to, scrolling off the top when full.
 */

#ifndef Q4K_PANEL_H
#define Q4K_PANEL_H

/* Claim rows `top`..`top+rows-1` as the panel and clear them. */
void panel_begin(unsigned char top, unsigned char rows);

/* Append one line as given, or word-wrapped across as many as it needs.
   These take ASCII; for a C string literal, which cc65 has encoded as
   PETSCII, use panel_lit. */
void panel_line(const char *ascii, unsigned char color);
void panel_wrap(const char *ascii, unsigned char color);
void panel_lit(const char *literal, unsigned char color);

/* The screen row the last line went to. */
unsigned char panel_row(void);

/* Forget the last line, so a placeholder can be replaced by the real thing. */
void panel_undo(void);

#endif /* Q4K_PANEL_H */
