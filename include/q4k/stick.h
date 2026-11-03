/*
 * Quest for the King -- the joystick.
 *
 * stick_key() waits for a key or a move of the joystick in port 2 and
 * returns what cgetc() would for the key; the stick's directions come back
 * as the cursor keys and its button as STICK_FIRE, which no key types. A
 * held direction repeats. stick_spins is how many times it went round the
 * wait, for rng_stir(). See stick.s.
 */

#ifndef Q4K_STICK_H
#define Q4K_STICK_H

#define STICK_FIRE 0xFE

extern unsigned int stick_spins;

unsigned char stick_key(void);

#endif /* Q4K_STICK_H */
