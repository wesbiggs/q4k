/*
 * q4k -- what you are supposed to be doing next.
 *
 * In the core (HIGH) rather than with the help screen, which is in the shop's
 * engine: the map says the next step whenever the story moves on, and the
 * help screen shows the same two lines.
 */

#include <q4k/game.h>
#include <q4k/help.h>
#include <q4k/view.h>

/* What to do next, by quest stage. Two lines each, because the useful
   version of this names the person and the place. */
const char *const quest_task[10][2] = {
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
      "Take the Emberstone to Vane in",
      "Wardlow, over the sea."
    },
    {
      "Take the Tidal Key over the sea to the",
      "Drowned Stair. Find the Silver Tear."
    },
    {
      "Bring the Silver Tear to Mother Ash",
      "in Ashcombe, over the sea."
    },
    {
      "Take the Ashen Key over the sea, open",
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
};

/* The next step, in the message window: called when the story moves on. */
void quest_hint(void)
{
    if (quest_stage < 9) {
        view_message(quest_task[quest_stage][0]);
        view_message(quest_task[quest_stage][1]);
    }
}

