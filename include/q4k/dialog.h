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

/* What the host can decide a conversation has earned, sent as "GIVE <g>".
   The host adjudicates; the model never does. */
#define GRANT_NONE   0
#define GRANT_TIDAL  1   /* Vane heard the fishers' word for the dry hour */
#define GRANT_SIGIL  2   /* the king handed over the Sigil, past hello    */

/* Set by dialog_talk(): the last grant the host sent, or GRANT_NONE. It
   sticks for the whole conversation, so talking on afterwards loses nothing. */
extern unsigned char dialog_granted;

/* `stage` is the quest stage; the daemon uses it to tell the NPC where the
   story has got to. Stays a single digit -- see Q_* in game.h. `seed` is the
   run's lore_seed, from which the host draws this game's secrets.

   `given` is called after every answer with dialog_granted so far. It hands
   over whatever the conversation has earned and returns the line announcing
   it (a C literal), or 0; the line goes into the transcript, not the map.

   The NPC may end the conversation (the host answers BYE): their last words
   stay up until a key is pressed, and dialog_talk returns DIALOG_OK. */
unsigned char dialog_talk(const char *npc_id, const char *place,
                          unsigned char stage, unsigned char seed,
                          const char *(*given)(unsigned char granted));

/* What the keeper of inn `inn_id` lets slip over an ale: one authored hint,
   drawn host-side from assets/hints.json, appended to whatever panel the
   caller has set up. No model is involved. DIALOG_OK if one arrived. */
unsigned char dialog_hint(const char *inn_id, unsigned char stage,
                          unsigned char seed);

#endif /* Q4K_DIALOG_H */
