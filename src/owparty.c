/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Quest for the King -- the open world's party, made: a program of its own.
 *
 * F1 on the splash screen is a new game. The bootstrap (src/openworld.c)
 * builds the lands and then loads this over itself, with OWB_NEW in the
 * hand-off block (q4k/ow.h): it tells the story and asks who answered the
 * king's letter, as the game's title screen does (src/lib/ow_party.c),
 * puts the new game in the game block (q4k/owgame.h) at Millford's gate,
 * and loads the engine over itself (ow_back()), which starts there. Nothing
 * of this is in memory after that. OWB_QUICK, the tests' start
 * (screendump.py --splash quick), makes a lone warrior outside Millford
 * without asking.
 *
 * Linked with cfg/c64-tool.cfg, as the small tools are: it all fits below
 * the charset at $3800, and its BSS ($4000 on) is clear of the game block.
 */

#include <conio.h>
#include <c64.h>

#include <q4k/charset.h>
#include <q4k/game.h>
#include <q4k/ow.h>
#include <q4k/owgame.h>
#include <q4k/tiles.h>

/* What the game's modules here (actor.c, owgame.c, ow_party.c) read and
   write of game.c's: defined here instead, so that game.c is not linked,
   nor the map, the land generator and the vault it brings with it. The
   game block is where they go. */
unsigned int  game_turn, gold;
unsigned char potions, arrows, quest_stage, items, lore_seed;
unsigned char looted[LOOTED_BYTES];

int main(void)
{
    unsigned char mode = OWB->mode;

    if (mode != OWB_NEW && mode != OWB_QUICK) {
        cputs("Load OPENWORLD: it builds the world.");
        for (;;) {
        }
    }
    charset_install();          /* the character set, before any text */
    VIC.bordercolor = COLOR_BLACK;
    VIC.bgcolor0    = COLOR_BLACK;
    VIC.addr        = CHARSET_D018;
    *(unsigned char *)0x0291 = 0x80;    /* C=+Shift keeps this set */
    clrscr();

    ow_new_game(mode == OWB_QUICK);     /* (turns the screen on to ask) */
    ow_back();                  /* the engine, at Millford */
    return 0;
}
