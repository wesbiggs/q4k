#include <conio.h>
#include <c64.h>

#include <q4k/game.h>
#include <q4k/help.h>
#include <q4k/sprite.h>
#include <q4k/view.h>

/* What to do next, by quest stage. Two lines each, because the useful
   version of this names the person and the place. */
static const char *const task[10][2] = {
    { "A letter has come from the king.",   "Go south to Castle Matheson."       },
    { "Go south to Castle Matheson and",    "speak with the king."               },
    { "The Iron Sigil opens the Hollow",    "Crown. Bring back the Emberstone."  },
    { "Take the Emberstone to Loremaster",  "Vane in Wardlow."                   },
    { "The Tidal Key opens the Drowned",    "Stair. Bring back the Silver Tear." },
    { "Take the Silver Tear to Mother Ash", "in Ashcombe."                       },
    { "The Ashen Key opens Blackpit.",      "Bring back the Dragon's Heart."     },
    { "Take all three stones to Garrett",   "the smith in Millford."             },
    { "Dragonbane is yours. Maldrath",      "lairs high in the Sundering."       },
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

    textcolor(COLOR_LIGHTBLUE);
    cputsxy(1, 7, "Commands");
    textcolor(COLOR_WHITE);
    cputsxy(3,  8, "T      Talk to whoever is beside you");
    cputsxy(3,  9, "P      Drink a healing draught");
    cputsxy(3, 10, "F1     Save       F3  Restore");
    cputsxy(3, 11, "SPACE  Wait a turn");
    cputsxy(3, 12, "?      This screen");
    cputsxy(3, 13, "Q      Quit");

    textcolor(COLOR_LIGHTBLUE);
    cputsxy(1, 15, "In a fight");
    textcolor(COLOR_WHITE);
    cputsxy(3, 16, "Walk into it; SPACE guards; R runs");
    cputsxy(3, 17, "Underground: A Attack  D Defend");

    textcolor(COLOR_GRAY1);
    chlinexy(0, 18, SCR_W);
    textcolor(COLOR_LIGHTBLUE);
    cputsxy(1, 19, "Your task");
    textcolor(COLOR_LIGHTGREEN);
    cputsxy(3, 20, task[stage][0]);
    cputsxy(3, 21, task[stage][1]);

    textcolor(COLOR_GRAY2);
    cputsxy(1, 23, "Any key to return");

    cgetc();
}
