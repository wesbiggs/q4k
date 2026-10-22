#include <conio.h>
#include <c64.h>

#include <q4k/game.h>
#include <q4k/help.h>
#include <q4k/sprite.h>
#include <q4k/text.h>
#include <q4k/view.h>

/* Two of the fight lines live in the charset's spare glyph space because
   HIGH is full; see CLAUDE.md. */
#pragma rodata-name ("CHDATA3")
static const char fight_strike[] = "Walk into foe to strike; SPACE guards";
#pragma rodata-name ("CHDATA2")
static const char fight_flee[]   = "Flee by a side gap (if you can)";
#pragma rodata-name ("HIDATA")

/* What to do next, by quest stage. Two lines each, because the useful
   version of this names the person and the place. */
static const char *const task[10][2] = {
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
    if (quest_stage > 0 && quest_stage < 9) {
        view_message(task[quest_stage][0]);
        view_message(task[quest_stage][1]);
    }
}

void help_screen(void)
{
    unsigned char stage = quest_stage;
    char seed[4];
    char turn[5];

    if (stage > 9) {
        stage = 9;
    }

    sprite_hide();
    clrscr();

    textcolor(COLOR_YELLOW);
    cputsxy(1, 0, "Quest for the King");
    textcolor(COLOR_GRAY2);
    cputsxy(SCR_W - 20, 0, "Turn");
    *str_num(turn, game_turn) = 0;
    text_put(SCR_W - 15, 0, turn, COLOR_GRAY2);
    cputsxy(SCR_W - 9, 0, "Seed");
    *str_num(seed, lore_seed) = 0;
    text_put(SCR_W - 4, 0, seed, COLOR_GRAY2);
    cputsxy(1, 24, "Press any key");
    textcolor(COLOR_GRAY1);
    chlinexy(0, 1, SCR_W);
    chlinexy(0, 18, SCR_W);

    textcolor(COLOR_LIGHTBLUE);
    cputsxy(1, 2, "Moving");
    cputsxy(1, 7, "Commands");
    cputsxy(1, 14, "In a fight");
    cputsxy(1, 20, "Your task");

    textcolor(COLOR_WHITE);
    // Moving
    cputsxy(3, 3, "W A S D  or the cursor keys");
    cputsxy(3, 4, "Walk into people to talk or trade");
    cputsxy(3, 5, "Step onto a port to sail");
    cputsxy(3, 6, "> <  enter/descend, exit/ascend");

    // Commands
    cputsxy(3,  8, "P  Drink a healing potion");
    cputsxy(3,  9, "L  Loot a chest, dig for treasure");
    cputsxy(3, 10, "I  Inventory and treasure maps");
    cputsxy(3, 11, "F1 Save          F3 Restore");
    cputsxy(3, 12, "M  Music         X  Effects");
    cputsxy(3, 13, "?  This screen   Q  Quit");

    // In a fight
    cputsxy(3, 15, fight_strike);
    cputsxy(3, 16, "F then a direction shoots your bow");
    cputsxy(3, 17, fight_flee);

    // Your task
    textcolor(COLOR_LIGHTGREEN);
    cputsxy(3, 21, task[stage][0]);
    cputsxy(3, 22, task[stage][1]);

    cgetc();
}
