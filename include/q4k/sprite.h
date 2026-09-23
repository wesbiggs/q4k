/*
 * q4k -- the hero as a hardware sprite.
 *
 * The hero is drawn by the VIC over the text screen instead of as a screen
 * code in it, so the square underneath shows through and a step can glide
 * the eight pixels between squares rather than jump them.
 *
 * Two sprites make one figure: sprite 0 is the hero in hero.color, and
 * sprite 1 is the same shape grown by a pixel all round in black, behind
 * it. The outline is what keeps a white figure readable on grass and snow.
 *
 * Sprite data lives in the cassette buffer ($0340 and $0380, blocks 13 and
 * 14), the only free 64-byte blocks in VIC bank 0: the charset fills
 * $3800-$3FFF and the program everything below it.
 */

#ifndef Q4K_SPRITE_H
#define Q4K_SPRITE_H

#define DIR_DOWN   0
#define DIR_UP     1
#define DIR_LEFT   2
#define DIR_RIGHT  3

/* Build the sprite data and set the registers up, hidden. */
void sprite_init(void);

/* Put the hero on screen cell (col,row) and make sure it is showing. */
void sprite_show(unsigned char col, unsigned char row);

/* Hide the hero, for screens that are not the map. */
void sprite_hide(void);

/* Turn to face a direction without moving. */
void sprite_face(unsigned char dir);

/* Walk one square in `dir` from wherever the sprite is now, taking a few
   frames over it. The map is not redrawn here -- view_draw() does that
   afterwards and puts the sprite where the hero now stands. */
void sprite_walk(unsigned char dir);

#endif /* Q4K_SPRITE_H */
