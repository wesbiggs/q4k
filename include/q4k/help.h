/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Quest for the King -- the keys, and what you are supposed to be doing.
   Takes over the screen; the caller redraws. */

#ifndef Q4K_HELP_H
#define Q4K_HELP_H

/* In the shop's engine: the map calls it through shop_entry() (SHOP_HELP). */
void help_screen(void);

/* Say what to do next, in the message window (quest.c, in the core). */
void quest_hint(void);

/* The two lines of what to do next, by quest stage (up to Q_VICTORY), and
   last the Emberstone's before the king has lent his ship (QT_NO_SHIP). An
   '@' is the compass point of the land the story wants next. */
#define QT_NO_SHIP 10
extern const char *const quest_task[11][2];

/* Line `half` (0 or 1) of what to do next, '@' made a compass point:
   north, east, south or west, by the seed's shuffle (ow.h's slot[]). */
const char *quest_line(unsigned char half);

#endif /* Q4K_HELP_H */
