/*
 * Quest for the King -- the screen before the game.
 *
 * Sets the scene and asks who you are, before view_init() takes over.
 */

#ifndef Q4K_TITLE_H
#define Q4K_TITLE_H

/* Show the opening and make the party: up to MAX_MADE members, each a name
   (PETSCII, as the panel wants it; RETURN alone picks one at random) and a
   class, made with party_make(). F3 restores the saved game instead; returns
   1 if it did, 0 for a new game. */
unsigned char title_screen(void);

#endif /* Q4K_TITLE_H */
