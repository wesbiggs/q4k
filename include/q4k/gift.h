/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Quest for the King -- what people hand over when you talk to them (gift.c).
 */

#ifndef Q4K_GIFT_H
#define Q4K_GIFT_H

/* Who the player is talking to (an NPC_* from town_data.h): set before the
   conversation, read by quest_after_talk(). */
extern unsigned char talking_to;

/* Hand over the quest items the conversation has earned. `granted` is what
   the host has decided so far, or GRANT_OPEN when there was no host to ask
   -- the game stays winnable without one. Called after every answer, so each
   case hands its item over once only. Returns the line announcing the gift,
   or 0. */
const char *quest_after_talk(unsigned char granted);

#endif /* Q4K_GIFT_H */
