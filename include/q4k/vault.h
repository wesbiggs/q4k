/*
 * q4k -- the RAM under the ROMs.
 *
 * Home is 64 by 64 and lives for the whole game under the I/O area; the
 * continent you left to go into a town can be put under the KERNAL, where
 * local[] is not needed. All of it is out of reach of ordinary code, so it
 * is read through these, each of which takes interrupts away for the length
 * of a copy -- see vault.s.
 */

#ifndef Q4K_VAULT_H
#define Q4K_VAULT_H

#define VAULT_SLOTS 4           /* local[]'s worth of map, 64 by 31, each */

/* Row y of home (64 squares) into dst, and one square of it. */
void          vault_row(unsigned char y, unsigned char *dst);
unsigned char vault_tile(unsigned char x, unsigned char y);

/* local[], less its last row, into a slot and back. */
void          vault_park(unsigned char slot);
void          vault_unpark(unsigned char slot);

#endif /* Q4K_VAULT_H */
