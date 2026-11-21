/*
 * Quest for the King -- the disk the game's files are read from.
 *
 * The open world's engine saves its game block itself (save_game() in
 * src/ow/engine.c); the talk reads its files from save_device.
 */

#ifndef Q4K_SAVEGAME_H
#define Q4K_SAVEGAME_H

/* The unit the game was loaded from, which the disk files are read from. */
extern unsigned char save_device;

#endif /* Q4K_SAVEGAME_H */
