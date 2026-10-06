/*
 * q4k -- the towns that are made rather than drawn.
 *
 * Every land has one, a place to rest and spend: an inn, a shop, and a house or
 * two, laid out on a grid of lots with a street between them so that no
 * building overlaps another and every door opens on to the street. Built into
 * local[] from the game's seed and the land, so it is the same town each
 * visit and a different one in the next game.
 */

#ifndef Q4K_TOWN_GEN_H
#define Q4K_TOWN_GEN_H

/* Build the town of the land you are on into local[], set hero to its doorstep
   and the town's people and way out (here_ents, here_n, here_ex, here_ey). */
void town_gen(void);

/* Its name, and its inn's. */
const char *town_gen_name(void);
const char *town_gen_inn(void);

#endif /* Q4K_TOWN_GEN_H */
