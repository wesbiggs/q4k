/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Quest for the King -- what the screens of text are handed.
 *
 * The game's modules that owtown and owtalk link (shop.c, inn.c, help.c,
 * talk.c, gift.c) were once engines of their own, loaded into one slot by
 * the game; their entries and the block they are handed keep that shape.
 * Their code and constants still go in the segments named for those engines
 * (the Makefile's ENG_ lists), which the open world's cfgs place.
 */

#ifndef Q4K_ENGINE_H
#define Q4K_ENGINE_H

/* What an entry is handed, besides the game's own state, and what it hands
   back: a line of text (a name going in, a note coming out). */
extern unsigned char eng_a, eng_b;
extern char          eng_text[41];

/* Talk: eng_a is TALK_NPC (eng_b the person, eng_text their name) or
   TALK_INN (eng_b the inn, eng_text its name). For a person with no lines
   on the disk, it returns 1 with what they handed over in eng_text. */
#define TALK_NPC  0
#define TALK_INN  1
unsigned char talk_entry(void);

/* The shop: eng_a is the kind (SHOP_*), or SHOP_GEAR to take up what a
   beaten monster dropped. The same entry holds the screens that are not a
   place: the help screen (SHOP_HELP) and the party's (SHOP_PARTY). */
#define SHOP_GEAR  0xFF
#define SHOP_HELP  0xFE
#define SHOP_PARTY 0xFD
unsigned char shop_entry(void);

#endif /* Q4K_ENGINE_H */
