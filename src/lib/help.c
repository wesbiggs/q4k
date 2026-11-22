/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <conio.h>
#include <c64.h>

#include <q4k/game.h>
#include <q4k/help.h>
#include <q4k/sprite.h>
#include <q4k/text.h>
#include <q4k/view.h>

/* In the shop's engine (engine.h): shop_entry() with SHOP_HELP. */
static const char fight_strike[] = "Each in turn: walk into foe to strike";
static const char fight_flee[]   = "SPACE guards; flee by a side gap";

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
    cputsxy(3, 5, "Step onto a ship to sail");
    cputsxy(3, 6, "> <  enter/descend, exit/ascend");

    // Commands
    cputsxy(3,  8, "P  The worst hurt drinks a draught");
    cputsxy(3,  9, "L  Loot a chest, dig for treasure");
    cputsxy(3, 10, "I  The party, spells, treasure maps");
    cputsxy(3, 11, "F1 Save          F3 Restore");
    cputsxy(3, 12, "M  Music         X  Effects");
    cputsxy(3, 13, "?  This screen   Q  Quit");

    // In a fight
    cputsxy(3, 15, fight_strike);
    cputsxy(3, 16, "F+dir shoots a bow, C casts a spell");
    cputsxy(3, 17, fight_flee);

    // Your task
    textcolor(COLOR_LIGHTGREEN);
    cputsxy(3, 21, quest_task[stage][0]);
    cputsxy(3, 22, quest_task[stage][1]);

    cgetc();
}
