/*
 * Quest for the King -- which continent holds which story place.
 *
 * game.h's slot_of(), on its own so that every program that needs it links
 * the one copy and nothing else: the open world's bootstrap and its
 * screens of text (each defines lore_seed itself).
 */

#include <q4k/game.h>

/* A shuffle of 0-3 from the game's seed, which is a byte: not every order
   comes up, but the common ones do and no seed gives a sequence a player
   could tell was not drawn by chance. */
unsigned char slot_of(unsigned char d)
{
    unsigned char p[4], i, j, t, v = lore_seed;

    for (i = 0; i < 4; ++i) {
        p[i] = i;
    }
    for (i = 3; i > 0; --i) {
        v = v * 5 + 3;
        j = (v >> 5) % (i + 1);
        t = p[i]; p[i] = p[j]; p[j] = t;
    }
    return p[d];
}
