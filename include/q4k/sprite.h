/*
 * q4k -- the hero as a hardware sprite.
 *
 * The hero is drawn by the VIC over the text screen instead of as a screen
 * code in it, so the square underneath shows through and a step can glide
 * the eight pixels between squares rather than jump them.
 *
 * Three sprites make one figure, front to back: sprite 0 is the tunic, in
 * a colour picked from the hero's name; sprite 1 is the hero in hero.color;
 * sprite 2 is the same shape grown by a pixel in black. The outline is what
 * keeps a white figure readable on grass and snow.
 *
 * Sprite data lives in the cassette buffer ($0340, $0380 and $03C0, blocks
 * 13 to 15), the only free 64-byte blocks in VIC bank 0: the charset fills
 * $3800-$3FFF and the program everything below it. All three are taken.
 *
 * Sprite 3 is a shot: an arrow, a breath of fire or of water. Its data sits inside the charset, over
 * reverse-video glyphs $C0-$C7 that the game never draws -- see sprite.c.
 * The same trick has room for more: the reverse half is 128 glyphs, and
 * only $A0 (the input cursor) is spoken for.
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

/* What flies: each has its own shape and colour. */
#define SHOT_ARROW 0   /* yellow; the hero's bow and the monsters' */
#define SHOT_FIRE  1   /* orange; Maldrath's breath */
#define SHOT_WATER 2   /* light blue; the Drowned King's */
#define SHOT_BOLT  3   /* white; lightning, the storm giant's */

/* Fly a shot of `kind` from screen cell (col,row) `squares` squares in
   `dir`, and hide it again at the end. Takes two frames a square. */
void sprite_shot(unsigned char col, unsigned char row,
                 unsigned char dir, unsigned char squares,
                 unsigned char kind);

#endif /* Q4K_SPRITE_H */
