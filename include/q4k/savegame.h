/*
 * Quest for the King -- saving and restoring.
 *
 * Writes one file to disk unit 8. The map you are standing in is not saved:
 * towns are fixed and dungeon levels regenerate exactly from their seed, so
 * rebuilding them is both smaller and faithful. What is saved is everything
 * that could not be worked out again -- your character, your progress, and
 * which squares of the level you have actually seen.
 */

#ifndef Q4K_SAVEGAME_H
#define Q4K_SAVEGAME_H

#define SAVE_OK       0
#define SAVE_EDISK    1   /* the drive would not do it            */
#define SAVE_EFORMAT  2   /* that file is not one of ours         */

/* Call first thing in main(): remembers the device the game came from. */
void save_init(void);
unsigned char save_game(void);
unsigned char load_game(void);

#endif /* Q4K_SAVEGAME_H */
