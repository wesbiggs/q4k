/*
 * Quest for the King -- the nearest place.
 *
 * Set near_x and near_y to a square and call place_near(): it returns which
 * of this land's places is nearest (the number place_at() gives it) and
 * leaves that place's square in near_fx and near_fy. See place_near.s.
 */

#ifndef Q4K_PLACE_NEAR_H
#define Q4K_PLACE_NEAR_H

extern unsigned char near_x, near_y, near_fx, near_fy;

unsigned char place_near(void);

#endif /* Q4K_PLACE_NEAR_H */
