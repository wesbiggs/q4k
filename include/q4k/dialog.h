/*
 * q4k -- talking to people.
 *
 * Takes over the screen, dials the host over the modem port, and runs a
 * conversation: the player types, the NPC answers. The caller redraws the map
 * afterwards.
 */

#ifndef Q4K_DIALOG_H
#define Q4K_DIALOG_H

#define DIALOG_OK        0
#define DIALOG_ENOLINE   1   /* the port would not open   */
#define DIALOG_ESILENT   2   /* nobody answered in time   */

/* `stage` is the quest stage; the daemon uses it to tell the NPC where the
   story has got to. Stays a single digit -- see Q_* in game.h. */
unsigned char dialog_talk(const char *npc_id, const char *place,
                          unsigned char stage);

#endif /* Q4K_DIALOG_H */
