#include <conio.h>
#include <c64.h>

#include <q4k/game.h>
#include <q4k/help.h>
#include <q4k/sprite.h>
#include <q4k/view.h>

/* What to do next, by quest stage. Two lines each, because the useful
   version of this names the person and the place. */
static const char *const task[10][2] = {
    { "A letter has come from the king.",   "Garrett the smith will arm you."    },
    { "Go to Castle Matheson and",          "speak with the king."               },
    { "The Iron Sigil opens the Hollow",    "Crown. Bring back the Emberstone."  },
    { "Take the Emberstone to Vane in",     "Wardlow, over the sea. Try the ports." },
    { "Take the Tidal Key over the sea to", "the Drowned Stair. Bring the Tear." },
    { "Take the Silver Tear to Mother Ash", "in Ashcombe, over the sea."         },
    { "Take the Ashen Key over the sea to", "Blackpit. Bring the Dragon's Heart." },
    { "Take all three stones to Garrett",   "the smith in Millford."             },
    { "Dragonbane is yours. Maldrath",      "lairs in the mountains of Keldmarch." },
    { "Maldrath is banished.",              "Keldmarch is free."                 },
};

void help_screen(void)
{
    unsigned char stage = quest_stage;

    if (stage > 9) {
        stage = 9;
    }

    sprite_hide();
    clrscr();

    textcolor(COLOR_YELLOW);
    cputsxy(1, 0, "Quest for the King");
    textcolor(COLOR_GRAY1);
    chlinexy(0, 1, SCR_W);

    textcolor(COLOR_LIGHTBLUE);
    cputsxy(1, 3, "Moving");
    textcolor(COLOR_WHITE);
    cputsxy(3, 4, "W A S D  or the cursor keys");
    cputsxy(3, 5, "Walk into someone to talk or trade");
    cputsxy(3, 6, "Step onto a port to sail");

    textcolor(COLOR_LIGHTBLUE);
    cputsxy(1, 7, "Commands");
    textcolor(COLOR_WHITE);
    cputsxy(3,  8, "T      Talk to whoever is beside you");
    cputsxy(3,  9, "P      Drink a healing draught");
    cputsxy(3, 10, "F1     Save       F3  Restore");
    cputsxy(3, 11, "SPACE  Wait a turn");
    cputsxy(3, 12, "L      Look, dig     I  Maps");
    cputsxy(3, 13, "?      This screen   M  Music");
    cputsxy(3, 14, "Q      Quit          X  Effects");

    textcolor(COLOR_LIGHTBLUE);
    cputsxy(1, 15, "In a fight");
    textcolor(COLOR_WHITE);
    cputsxy(3, 16, "Walk into it to strike; SPACE guards");
    cputsxy(3, 17, "F then a direction shoots your bow");
    cputsxy(3, 18, "Flee by a side gap (not keepers)");

    textcolor(COLOR_GRAY1);
    chlinexy(0, 19, SCR_W);
    textcolor(COLOR_LIGHTBLUE);
    cputsxy(1, 20, "Your task");
    textcolor(COLOR_LIGHTGREEN);
    cputsxy(3, 21, task[stage][0]);
    cputsxy(3, 22, task[stage][1]);

    textcolor(COLOR_GRAY2);
    cputsxy(1, 24, "Any key to return");

    cgetc();
}
