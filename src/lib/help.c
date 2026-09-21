#include <conio.h>
#include <c64.h>

#include <q4k/game.h>
#include <q4k/help.h>
#include <q4k/view.h>

/* What to do next, by quest stage. Two lines each, because the useful
   version of this names the person and the place. */
static const char *const task[10][2] = {
    { "A LETTER HAS COME FROM THE KING.",   "GO SOUTH TO CASTLE MATHESON."       },
    { "GO SOUTH TO CASTLE MATHESON AND",    "SPEAK WITH THE KING."               },
    { "THE IRON SIGIL OPENS THE HOLLOW",    "CROWN. BRING BACK THE EMBERSTONE."  },
    { "TAKE THE EMBERSTONE TO LOREMASTER",  "VANE IN WARDLOW."                   },
    { "THE TIDAL KEY OPENS THE DROWNED",    "STAIR. BRING BACK THE SILVER TEAR." },
    { "TAKE THE SILVER TEAR TO MOTHER ASH", "IN ASHCOMBE."                       },
    { "THE ASHEN KEY OPENS BLACKPIT.",      "BRING BACK THE DRAGON'S HEART."     },
    { "TAKE ALL THREE STONES TO GARRETT",   "THE SMITH IN MILLFORD."             },
    { "DRAGONBANE IS YOURS. MALDRATH",      "LAIRS HIGH IN THE SUNDERING."       },
    { "MALDRATH IS BANISHED.",              "KELDMARCH IS FREE."                 },
};

void help_screen(void)
{
    unsigned char stage = quest_stage;

    if (stage > 9) {
        stage = 9;
    }

    clrscr();

    textcolor(COLOR_YELLOW);
    cputsxy(1, 0, "QUEST FOR THE KING");
    textcolor(COLOR_GRAY1);
    chlinexy(0, 1, SCR_W);

    textcolor(COLOR_LIGHTBLUE);
    cputsxy(1, 3, "MOVING");
    textcolor(COLOR_WHITE);
    cputsxy(3, 4, "W A S D  OR THE CURSOR KEYS");
    cputsxy(3, 5, "WALK INTO SOMEONE TO TALK OR TRADE");

    textcolor(COLOR_LIGHTBLUE);
    cputsxy(1, 7, "COMMANDS");
    textcolor(COLOR_WHITE);
    cputsxy(3,  8, "T      TALK TO WHOEVER IS BESIDE YOU");
    cputsxy(3,  9, "P      DRINK A HEALING DRAUGHT");
    cputsxy(3, 10, "F1     SAVE     F3  RESTORE");
    cputsxy(3, 11, "SPACE  WAIT A TURN");
    cputsxy(3, 12, "?      THIS SCREEN");
    cputsxy(3, 13, "Q      QUIT");

    textcolor(COLOR_LIGHTBLUE);
    cputsxy(1, 15, "IN A FIGHT");
    textcolor(COLOR_WHITE);
    cputsxy(3, 16, "A ATTACK  D DEFEND  P POTION  R RUN");

    textcolor(COLOR_GRAY1);
    chlinexy(0, 18, SCR_W);
    textcolor(COLOR_LIGHTBLUE);
    cputsxy(1, 19, "YOUR TASK");
    textcolor(COLOR_LIGHTGREEN);
    cputsxy(3, 20, task[stage][0]);
    cputsxy(3, 21, task[stage][1]);

    textcolor(COLOR_GRAY2);
    cputsxy(1, 23, "ANY KEY TO RETURN");

    cgetc();
}
