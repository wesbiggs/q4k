/*
 * q4k -- talking to people.
 *
 * Takes over the screen and runs a conversation; the caller redraws the map
 * afterwards. The two builds differ here and nowhere else: the game, offline
 * (talk.c), loads what each person can say from the disk, and the player picks
 * from a menu; the experimental networked one (Q4K_ONLINE, make online:
 * dialog.c) dials the host over the modem port, and the player types while the
 * NPC answers.
 * Both are in the talk's engine (engine.h), and both are called the same way.
 */

#ifndef Q4K_DIALOG_H
#define Q4K_DIALOG_H

#define DIALOG_OK        0
#define DIALOG_ENOLINE   1   /* nobody to ask: no host, or no file */
#define DIALOG_ESILENT   2   /* nobody answered in time   */

/* What the host can decide a conversation has earned, sent as "GIVE <g>".
   The host adjudicates; the model never does. */
#define GRANT_NONE   0
#define GRANT_TIDAL  1   /* Vane heard the fishers' word for the dry hour */
#define GRANT_SIGIL  2   /* the king handed over the Sigil, past hello    */
#define GRANT_DAGGER 3   /* Garrett handed over a dagger, once asked      */
#define GRANT_ASHEN  4   /* Mother Ash gave up the Ashen Key for the Tear */
#define GRANT_FORGE  5   /* Garrett agreed to reforge Dragonbane          */
/* Nobody to ask, so nothing is gated: the game stays winnable without the
   modem. Passed to dialog_talk()'s `given` when there is nobody to ask, and
   offline for every line marked to give. */
#define GRANT_OPEN   0xFF

/* Talk to NPC number `npc` (NPC_* in town_data.h), headed `name`, until the
   player leaves (RUN/STOP) or the NPC ends it.

   `given` hands over whatever the conversation has earned and returns the
   line announcing it (a C literal), or 0; the line goes into the transcript.
   Over the modem it is called after every answer, with what the host has
   granted so far; offline, with GRANT_OPEN after a line marked to give.

   DIALOG_ENOLINE if there is nobody to talk to -- no host on the line (and
   then nothing has been drawn), or no file on the disk -- and DIALOG_ESILENT
   if the host has stopped answering, which takes the line down for the rest
   of the game. Either way talk_entry() (gift.c) hands over what the quest
   needs with GRANT_OPEN, and the map says so. */
unsigned char dialog_talk(unsigned char npc, const char *name,
                          const char *(*given)(unsigned char granted));

/* One of inn `inn`'s rumours for the quest stage, appended to whatever panel
   the caller has set up: over the modem, an authored hint the host draws from
   assets/hints.json (no model is involved); offline, one from the inn's file
   on the disk, which is loaded the first time and kept until dialog_forget().
   DIALOG_OK if one was told -- or, over the modem, if there is no host at
   all, which the title screen has already said. */
unsigned char dialog_hint(unsigned char inn);

/* Forget what is loaded (offline: the buffer is someone else's scratch
   after). Does nothing over the modem. */
void dialog_forget(void);

#ifdef Q4K_ONLINE

/* The line to the host, which is in the core (line.c), since the title
   screen asks it before any engine is loaded. */

/* What dialog_probe() found on the line when the game started. */
#define LINE_UP       0   /* a host answered, and can talk               */
#define LINE_SILENT   1   /* nobody answered: no modem, or no host on it */
#define LINE_NOMODEL  2   /* a host answered, but has no model to talk   */

extern unsigned char dialog_line;

/* Open the modem port and ask whoever is there whether they can talk, for
   the title screen to report. Sets dialog_line and returns it; a few
   seconds when nobody answers. Anything but LINE_UP and the game plays
   without conversations -- see dialog_talk(). */
unsigned char dialog_probe(void);

/* The host's next line, from its verb on, in reply[]; 0 if `ticks` jiffies
   pass in silence. */
#define REPLY_MAX 180
extern char reply[REPLY_MAX];
char *line_answer(unsigned int ticks);

#endif /* Q4K_ONLINE */

#endif /* Q4K_DIALOG_H */
