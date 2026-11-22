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
#define K_STOP       0x03

/* Member `who`'s figure, its top left at character `col`, `row`. */
void fig_show(unsigned char who, unsigned char col, unsigned char row);

/* Member `who`'s colours on the figure, after their byte of tunic[]
   changed. */
void fig_colours(unsigned char who);

/* A key, with the figure idling meanwhile (every sixteenth jiffy). */
unsigned char fig_key(unsigned char who);

/* Member `who`'s clothes' colour (or their skin's) a step on round every
   colour but black (the outline), or back. */
void fig_step(unsigned char who, unsigned char skin, unsigned char back);

/* Members `a` and `b` change places: who they are and their colours. */
void fig_swap(unsigned char a, unsigned char b);

/* The figure gone. */
void fig_off(void);

#endif /* Q4K_OWFIG_H */
