/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * q4k -- a member's figure on a screen of text (src/lib/ow_figure.c): their
 * class's two frames (src/lib/ow_figs.s, on sprite blocks the VIC sees in
 * bank 0), doubled, in their colours from the game block (OWG_DRESS,
 * OWG_SKIN), idling between the two while a key is awaited, as the map and
 * the fights show them. Sprite 0 is the figure, multicolour; sprite 1 their
 * skin, hi-res, over it. For the party screen (owtown's I) and the party's
 * making (owparty).
 */

#ifndef Q4K_OWFIG_H
#define Q4K_OWFIG_H

#define K_CRSR_RIGHT 0x1D
#define K_CRSR_LEFT  0x9D
#define K_CRSR_DOWN  0x11
#define K_CRSR_UP    0x91

/* Member `who`'s figure, its top left at character `col`, `row`. */
void fig_show(unsigned char who, unsigned char col, unsigned char row);

/* Member `who`'s colours on the figure, after their byte of tunic[]
   changed. */
void fig_colours(unsigned char who);

/* A key, with the figure idling meanwhile (every sixteenth jiffy). */
unsigned char fig_key(unsigned char who);

/* The cursor keys change `who`'s clothes, K their skin, round every colour
   but black (the outline); 1 if `key` was one of them. */
unsigned char fig_recolour(unsigned char who, unsigned char key);

/* The figure gone. */
void fig_off(void);

#endif /* Q4K_OWFIG_H */
