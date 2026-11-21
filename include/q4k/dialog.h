/*
 * q4k -- talking to people.
 *
 * Takes over the screen and runs a conversation (talk.c): what each person
 * can say is loaded from the disk, and the player picks from a menu.
 */

#ifndef Q4K_DIALOG_H
#define Q4K_DIALOG_H

#define DIALOG_OK        0
#define DIALOG_ENOLINE   1   /* nobody to ask: no file on the disk */

/* What a conversation has earned, for gift.c's quest_after_talk(). A line
   marked to give hands over whatever the quest needs at that point, which
   is GRANT_OPEN; the others name one gift each. */
#define GRANT_NONE   0
#define GRANT_TIDAL  1   /* Vane's word for the dry hour  */
#define GRANT_SIGIL  2   /* the king's Sigil              */
#define GRANT_DAGGER 3   /* Garrett's dagger              */
#define GRANT_ASHEN  4   /* Mother Ash's Ashen Key        */
#define GRANT_FORGE  5   /* Garrett's reforged Dragonbane */
#define GRANT_OPEN   0xFF

/* Talk to NPC number `npc` (NPC_* in town_data.h), headed `name`, until the
   player leaves (RUN/STOP) or the NPC ends it.

   `given` hands over whatever the conversation has earned and returns the
   line announcing it (a C literal), or 0; the line goes into the transcript.
   It is called with GRANT_OPEN after a line marked to give.

   DIALOG_ENOLINE if there is nobody to talk to: no file on the disk. Then
   talk_entry() (gift.c) hands over what the quest needs with GRANT_OPEN. */
unsigned char dialog_talk(unsigned char npc, const char *name,
                          const char *(*given)(unsigned char granted));

/* One of inn `inn`'s rumours for the quest stage, appended to whatever panel
   the caller has set up: one from the inn's file on the disk (made from
   assets/hints.json), which is loaded the first time and kept until
   dialog_forget(). DIALOG_OK if one was told. */
unsigned char dialog_hint(unsigned char inn);

/* Forget what is loaded: the buffer is someone else's scratch after. */
void dialog_forget(void);

#endif /* Q4K_DIALOG_H */
