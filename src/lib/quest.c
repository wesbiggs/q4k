/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * q4k -- what you are supposed to be doing next.
 *
 * In the core (HIGH) rather than with the help screen, which is in the shop's
 * engine: the map says the next step whenever the story moves on, and the
 * help screen shows the same two lines.
 */

#include <q4k/game.h>
#include <q4k/help.h>
#include <q4k/ow.h>
#include <q4k/view.h>

/* What to do next, by quest stage. Two lines each, because the useful
   version of this names the person and the place. */
const char *const quest_task[11][2] = {
  //  1234567890123456789012345678901234567890
    {
      "A letter has come from the king.",
      "Garrett the smith will arm you."
    },
    {
      "Go to Castle Matheson and",
      "speak with the king."
    },
    {
      "The Iron Sigil opens the Hollow",
      "Crown. Seek the Emberstone."
    },
    {
      "Sail @ to Wardlow and take the",
      "Emberstone to Vane."
    },
    {
      "Sail @ with the Tidal Key to the",
      "Drowned Stair. Find the Silver Tear."
    },
    {
      "Sail @ to Ashcombe and bring the",
      "Silver Tear to Mother Ash."
    },
    {
      "Sail @ with the Ashen Key, open",
      "Blackpit, and seek the Dragon's Heart."
    },
    {
      "Take all three stones to Garrett, the",
      "smith in Millford, to forge Dragonbane."
    },
    {
      "Dragonbane is yours. Maldrath's lair",
      "lies high in a mountain hollow."
    },
    {
      "Maldrath is banished! Keldmarch is",
      "free. Bring news to the king!"
    },
    {                                       /* QT_NO_SHIP */
      "Show the king the Emberstone. His",
      "ship waits at the castle's port."
    },
};

/* The continents by compass point (ow.h's slot[] is by the same), and the
   story's land by stage: Wardlow's at Q_EMBER, the Drowned Stair's, then
   Ashcombe's and Blackpit's (slot.c's order). */
static const char *const compass[4] = { "north", "east", "south", "west" };

const char *quest_line(unsigned char half)
{
    static char buf[44];
    unsigned char stage = quest_stage > Q_VICTORY ? Q_VICTORY : quest_stage;
    unsigned char i;
    const char *s, *w;
    char *d = buf;

    if (stage == Q_EMBER && !(items & IT_SHIP)) {
        stage = QT_NO_SHIP;
    }
    for (s = quest_task[stage][half]; *s; ++s) {
        if (*s == '@') {
            for (i = 0; i < 3 && OW->slot[i] != stage - Q_EMBER; ++i) {
            }
            for (w = compass[i]; *w; ) {
                *d++ = *w++;
            }
        } else {
            *d++ = *s;
        }
    }
    *d = 0;
    return buf;
}

/* The next step, in the message window: called when the story moves on. */
void quest_hint(void)
{
    if (quest_stage < 9) {
        view_message(quest_task[quest_stage][0]);
        view_message(quest_task[quest_stage][1]);
    }
}

