/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Quest for the King -- the open world's town: its screens of text.
 *
 * The smooth-scrolling prototype's engine (src/ow/engine.c) has no room for
 * anything but the map, so the screens of text are this program, loaded
 * over the engine when they are wanted (ow_run), with the hand-off block
 * (q4k/ow.h's OWB) saying what for (the party's making, after the splash
 * screen's F1, is a program of its own: src/owparty.c):
 *
 *   (OWB_TALK, a conversation, is a program of its own: src/owtalk.c)
 *   OWB_SHOP            a shop (src/lib/shop.c), arg its kind (SHOP_*)
 *   OWB_INN             an inn (src/lib/inn.c): ale and a rumour from the
 *                       inn's file on the disk, or beds; arg which inn
 *   OWB_PARTY           I: the party, one at a time with their figure:
 *                       what they carry, a wizard's spells, their
 *                       colour and pronouns, and their places
 *   OWB_HELP            ?: the open world's keys, and what to do next
 *   OWB_FIGHT           a fight (src/lib/ow_fight.c, with the multicolour
 *                       fight module, which loads over this program's code
 *                       above $4000: what runs after it is below)
 *   OWB_SPOILS          what a fight won left behind, arms or a treasure
 *                       map (src/lib/ow_tmap.c): this program again, loaded
 *                       by itself after the fight (ow_back()), since the
 *                       shop's code that hands arms out was under the module
 *
 * The music the engine was playing carries on here where it paused
 * (src/lib/ow_play.s); a fight plays its own scene's (ow_fight.c's
 * fight_music()), carrying on with the engine's if it is the same song.
 *
 * It reads and writes the game through the game block (q4k/owgame.h), and
 * when it is done the engine is loaded back (ow_back()) and carries on from
 * there: in the same town, where the hero stood. Everything the engine
 * keeps is rebuilt from the disk on the way (OW-DATA, which this program
 * writes over, is loaded again), or is still in memory: the vault under the
 * I/O area and the music under the KERNAL are not reached. Linked with
 * cfg/c64-owtown.cfg (one load).
 */

#include <cbm.h>
#include <conio.h>
#include <c64.h>
#include <peekpoke.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/charset.h>
#include <q4k/dialog.h>
#include <q4k/engine.h>
#include <q4k/game.h>
#include <q4k/gift.h>
#include <q4k/inn.h>
#include <q4k/town_gen.h>
#include <q4k/isles.h>
#include <q4k/ow.h>
#include <q4k/owfig.h>
#include <q4k/owgame.h>
#include <q4k/owtmap.h>
#include <q4k/help.h>
#include <q4k/shop.h>
#include <q4k/text.h>
#include <q4k/tiles.h>
#include <q4k/town_data.h>

/* The game's own variables and the few functions of game.c's and the
   rest that the modules here use are src/ow/owstand.c's, shared with
   owtalk. The music the engine left paused (src/lib/ow_play.s): */
void owt_music_on(void);
void owt_music_off(void);
extern unsigned char owt_nowait;    /* the pause need not wait for VICE */

/* --- I: the party ----------------------------------------------------------

   One member at a time, with their figure as the map and the fights show
   it: their class, idling between its two frames (assets/monsters.txt,
   tools/figures.py), doubled, in their colours from the game block. The
   cursor keys go left and right through the party; 1-3 puts the member
   shown in that place (the first leads, and is the hero on the map); M
   lists the treasure maps carried (src/lib/ow_tmap.c); T
   steps their clothes' colour on (all but black, the outline; SHIFT+T
   back), P their pronouns (both are kept in the game block, so the engine
   and a save have them), S prepares a wizard's spells; RUN/STOP goes back
   to the game. The skin and the name, like the class, are the party's
   making's (owparty) alone. The hero on the map is drawn in the leader's
   colours, and a fight has everyone's. The rows are shop.c's, as the
   game's party screen has them. The figure is src/lib/ow_figure.c's
   (q4k/owfig.h), which owparty shows too: a multicolour sprite whose
   frames are src/lib/ow_figs.s, which the cfg puts on a sprite block in
   VIC bank 0 (OWFIGS), with a hi-res one over it for the skin. */

/* Above $4000, which only a fight overwrites, and none comes after this. */
#pragma code-name (push, "HICODE")

#define FIG_COL  18             /* the figure's top left, in characters */
#define FIG_ROW  8

static void hint(unsigned char y, const char *s)
{
    textcolor(COLOR_GRAY2);
    cputsxy(0, y, s);
}

/* P steps a member's pronouns on in the order owparty asks them: he/him,
   she/her, they/them, it, and round again. */
static const unsigned char pron_next[4] = {
    PRON_IT, PRON_SHE, PRON_THEY, PRON_HE       /* after they, he, she, it */
};

/* Member `who`'s pronouns, past their arms. */
static void pronouns(unsigned char who)
{
    textcolor(COLOR_GRAY2);
    cclearxy(29, 3, 10);
    cputsxy(29, 3, pron_names[ACT_PRON(party[who])]);
}

/* Member `who`'s page: shop.c's rows for them and for the party. */
static void page(unsigned char who)
{
    party_head();
    if (party_count > 1) {
        textcolor(COLOR_GRAY2);
        gotoxy(34, 0);
        cputc('0' + who + 1);
        cputs(" of ");
        cputc('0' + party_count);
        hint(19, "Cursor left, right  the others");
        hint(20, party_count > 2 ? "1-3  Change places"
                                 : "1 2  Change places");
    }
    member_rows(who, 2);
    pronouns(who);
    party_totals(15);
    hint(21, "T  Tunic colour   P  Pronouns");
    if (party_spells(who) > 1) {
        hint(22, "S  Prepare spells");
    }
    hint(23, "M  Treasure maps");
    hint(24, "RUN/STOP  Back to the game");
}

static void party_look(void)
{
    unsigned char who = 0, key, k;

    for (;;) {
        page(who);
        fig_show(who, FIG_COL, FIG_ROW);
        for (;;) {
            key = fig_key(who);
            k = key & 0x7F;
            if (k == 'p') {
                ACT_SET_PRON(party[who], pron_next[ACT_PRON(party[who])]);
                pronouns(who);
            } else if (k == 't') {
                fig_step(who, 0, key & 0x80);
            } else if (k == 'm') {
                tmap_list();
                break;
            } else if (key == K_STOP || key == K_CRSR_RIGHT
                       || key == K_CRSR_LEFT
                       || (k == 's' && party_spells(who) > 1)
                       || (unsigned char)(key - '1') < party_count) {
                break;
            }
        }
        if (key == K_STOP) {
            break;
        }
        if (k == 'm') {
            /* (the page again) */
        } else if (key == K_CRSR_RIGHT) {
            who = who + 1 == party_count ? 0 : who + 1;
        } else if (key == K_CRSR_LEFT) {
            who = who ? who - 1 : party_count - 1;
        } else if (k == 's') {
            while (party_prepare(who)) {
                page(who);
            }
        } else {
            key -= '1';                 /* the place they are put in */
            fig_swap(who, key);
            who = key;
        }
    }
    fig_off();
}

#pragma code-name (pop)

/* ?: the open world's keys (the game's help.c has the game's), and what
   the quest wants next (quest.c's, as the game's help screen has it). */
static void help(void)
{
    textcolor(COLOR_YELLOW);
    cputsxy(1, 0, "Quest for the King");
    /* The world's seed (SHIFT+F1 at the title makes a world from one) */
    str_num(str_lit(eng_text, "Seed "), lore_seed);
    text_put(30, 0, eng_text, COLOR_GRAY2);
    textcolor(COLOR_GRAY1);
    chlinexy(0, 1, 40);
    chlinexy(0, 19, 40);
    textcolor(COLOR_LIGHTBLUE);
    cputsxy(1, 2, "Moving");
    cputsxy(1, 7, "Commands");
    cputsxy(1, 15, "In a fight");
    cputsxy(1, 20, "Your task");
    textcolor(COLOR_WHITE);
    cputsxy(3, 3, "W A S D, the cursor keys or a stick");
    cputsxy(3, 4, "Q E Z X  diagonally; 1 2  slow, fast");
    cputsxy(3, 5, "Walk into people to talk or trade");
    cputsxy(3, 6, "+ or fire  go in, down, or dig;  - up");
    cputsxy(3, 8, "P  The worst hurt drinks a draught");
    cputsxy(3, 9, "L  Look about the map (SPACE, STOP)");
    cputsxy(3, 10, "I  The party, their gear, maps");
    cputsxy(3, 11, "F1 Save          F3 Restore");
    cputsxy(3, 12, "M  Music   SHIFT+M  Effects");
    cputsxy(3, 13, "?  This screen    SPACE  Party lines");
    cputsxy(3, 14, "RUN/STOP  Quit to the title");
    cputsxy(3, 16, "Each in turn: walk into foe to strike");
    cputsxy(3, 17, "^ aims, SPACE shoots; * casts a spell");
    cputsxy(3, 18, "SPACE guards; all flee by a side gap");
    textcolor(COLOR_LIGHTGREEN);
    cputsxy(1, 21, quest_line(0));
    cputsxy(1, 22, quest_line(1));
    textcolor(COLOR_GRAY2);
    cputsxy(1, 24, "Press any key");
    cgetc();
}

int main(void)
{
    unsigned char mode = OWB->mode, stage;

    if (mode < OWB_SHOP || mode > OWB_SPOILS ||
        mode == OWB_LOAD) {     /* (a conversation is owtalk's) */
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
    while (kbhit()) {
        cgetc();
    }
    VIC.ctrl1 |= 0x10;          /* set up: the screen on */

    owg_in();
    stage = quest_stage;        /* (a fight may move the quest on) */
    /* rand() starts from the same seed in every program, so every visit
       would hear the same greeting and the same rumour: the game's turn
       and seed make it another one each time, and the same in a test. */
    srand(game_turn ^ ((unsigned int)lore_seed << 8)
          ^ (mode == OWB_SPOILS ? OWB->npc : 0));
    land = G->land;             /* the shops' tiers, a made town's inn */
    if (mode != OWB_FIGHT) {    /* (the fight module plays its own) */
        owt_music_on();
    }
    switch (mode) {
    case OWB_SHOP:              /* the game's shops (shop.c) */
        eng_a = OWB->arg;
        shop_entry();
        break;
    case OWB_INN:               /* ale and a rumour, or beds (inn.c) */
        if (G->place > TOWN_PROC) {     /* an island's (q4k/isles.h) */
            land = G->place - (TOWN_PROC + 1) + ISL_LAND;
        }
        strcpy(eng_text, OWB->arg < NUM_INNS ? inn_names[OWB->arg]
                                             : town_gen_inn());
        land = G->land;
        inn_open(OWB->arg);
        break;
    case OWB_FIGHT:             /* the fight module, over HIGH */
        ow_fight(OWB->arg, OWB->npc);
        break;
    case OWB_PARTY:             /* who they are and what they carry */
        party_look();
        break;
    case OWB_HELP:
        help();
        break;
    case OWB_HOARD:             /* a sea cave's chest (shop.c) */
        cave_hoard(OWB->arg);
        break;
    case OWB_SPOILS:            /* what the fight just won left */
        ow_spoils();
        OWB->mode = OWB_BACK;   /* (and then the engine) */
        break;
    }
    /* (no drive: no wait) back to the engine, or to this program again
       for a fight's spoils (OWB_SPOILS, which ow_fight() sets) */
    owt_nowait = ow_cached(OWB->mode == OWB_SPOILS ? "owtown" : "ow-data");
    owt_music_off();            /* paused, for the engine to carry on with */
    if (quest_stage != stage) {
        OW_NEWTASK = 1;         /* for the engine to say */
    }
    owg_out();
    ow_back();                  /* the engine, which carries on from the
                                   game block (owgame.h) */
    return 0;
}
