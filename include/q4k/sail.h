/*
 * q4k -- the ships.
 *
 * The voyage is shown, not skipped: the ship is drawn over the sea square it
 * sails on, a square at a time, over whichever map it is on, and the view
 * follows it. Out from a port that is the overworld's own water, and then
 * the new continent's own; home again is the same two in the other order.
 */

#ifndef Q4K_SAIL_H
#define Q4K_SAIL_H

/* The hero has stepped onto a port on the overworld: sail to its continent
   and come ashore at the landing there. */
void sail_out(void);

/* The hero has stepped onto a continent's landing: sail home to the port
   they left from, which is `place`. */
void sail_home(void);

#endif /* Q4K_SAIL_H */
