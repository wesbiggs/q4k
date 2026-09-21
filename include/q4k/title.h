/*
 * Quest for the King -- the screen before the game.
 *
 * Sets the scene and asks who you are, before view_init() takes over.
 */

#ifndef Q4K_TITLE_H
#define Q4K_TITLE_H

/* Show the opening and read a name into `name` (at most `max`-1 characters,
   PETSCII, as the panel wants it). Leaves `name` alone if nothing is typed,
   so whatever the caller put there stands as the default. */
void title_screen(char *name, unsigned char max);

#endif /* Q4K_TITLE_H */
