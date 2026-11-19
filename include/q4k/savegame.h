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
/* A talk's file (talk.c) to `at`, through the REU's cache, the music
   paused round the disk if it is not there: 1 if it loaded
   (src/ow/owstand.c). */
unsigned char __fastcall__ talk_load(const char *name, void *at);

#endif /* Q4K_SAVEGAME_H */
