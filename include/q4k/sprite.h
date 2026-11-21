/*
 * q4k -- the four ways, and what flies (the bestiary's shots).
 *
 * The open world's engine and its fight draw their own sprites; the screens
 * of text only hide them (sprite_hide(), src/ow/owstand.c).
 */

#ifndef Q4K_SPRITE_H
#define Q4K_SPRITE_H

#define DIR_DOWN   0
#define DIR_UP     1
#define DIR_LEFT   2
#define DIR_RIGHT  3

/* Hide every sprite, for screens that are not the map. */
void sprite_hide(void);

/* What flies: each has its own shape and colour. */
#define SHOT_ARROW 0   /* yellow; the hero's bow and the monsters' */
#define SHOT_FIRE  1   /* orange; Maldrath's breath */
#define SHOT_WATER 2   /* light blue; the Drowned King's */
#define SHOT_BOLT  3   /* white; lightning, the storm giant's */

#endif /* Q4K_SPRITE_H */
