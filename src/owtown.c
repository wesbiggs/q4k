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
 *                       what they carry, a wizard's spells, their colour
 *   OWB_HELP            ?: the open world's keys, and what to do next
 *   OWB_FIGHT           a fight (src/lib/ow_fight.c, with the multicolour
 *                       fight module, which loads over this program's code
 *                       above $4000: what runs after it is below)
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
#include <q4k/ow.h>
#include <q4k/owgame.h>
#include <q4k/help.h>
#include <q4k/shop.h>
#include <q4k/tiles.h>
#include <q4k/town_data.h>

/* The game's own variables and the few functions of game.c's and the
   rest that the modules here use are src/ow/owstand.c's, shared with
   owtalk. The music the engine left paused (src/lib/ow_play.s): */
void owt_music_on(void);
void owt_music_off(void);

/* --- I: the party ----------------------------------------------------------

   One member at a time, with their figure as the map and the fights show
   it: their class, idling between its two frames (assets/monsters.txt,
   tools/figures.py), doubled, in their colour from the game block. The
   cursor keys go left and right through the party and up and down through
   the colours of their clothes (all but black, the outline), and K round
   their skin's, which is seen on the figure as it changes (the skin is a
   hi-res sprite of its own over the figure, as everywhere): the hero on the map is drawn in the leader's, and a fight
   has everyone's. The rows are shop.c's, as the game's party screen has
   them. The figure is a multicolour sprite whose frames are
   src/lib/ow_figs.s, which the cfg puts on a sprite block in VIC bank 0
   (OWFIGS). */

extern const unsigned char ow_figs[];

/* Above $4000, which only a fight overwrites, and none comes after this. */
#pragma code-name (push, "HICODE")

#define FIG_COL  18             /* the figure's top left, in characters */
#define FIG_ROW  8
#define K_CRSR_RIGHT 0x1D
#define K_CRSR_LEFT  0x9D
#define K_CRSR_DOWN  0x11
#define K_CRSR_UP    0x91

static unsigned char fig_bob;

/* Member `who`'s clothes and skin on the figure (OWG_DRESS, OWG_SKIN). */
static void colours(unsigned char who)
{
    VIC.spr0_color = OWG_DRESS(G->tunic[who]);
    VIC.spr1_color = OWG_SKIN(G->tunic[who]);
}

/* The figure for member `who`, doubled, below their rows. */
static void figure(unsigned char who)
{
    VIC.spr0_x      = (unsigned char)(24 + FIG_COL * 8);
    VIC.spr_hi_x    = 24 + FIG_COL * 8 > 255;
    VIC.spr0_y      = 50 + FIG_ROW * 8;
    VIC.spr1_x      = VIC.spr0_x;       /* their skin, hi-res, over it */
    VIC.spr1_y      = VIC.spr0_y;
    VIC.spr_hi_x   |= VIC.spr_hi_x << 1;
    colours(who);
    VIC.spr_mcolor0 = COLOR_GRAY1;      /* the outline: black on the map and
                                           in a fight, but this is black */
    VIC.spr_mcolor  = 1;
    VIC.spr_exp_x   = 3;
    VIC.spr_exp_y   = 3;
    fig_bob = 0xFF;                     /* (fig_key() sets the frame) */
    VIC.spr_ena     = 3;
}

/* A key, with the figure idling meanwhile: every sixteenth jiffy it shifts
   its feet. */
static unsigned char fig_key(unsigned char who)
{
    unsigned char b;

    while (!kbhit()) {
        b = (PEEK(0xA2) >> 4) & 1;
        if (b != fig_bob) {
            fig_bob = b;
            b += (unsigned char)((unsigned int)ow_figs >> 6)
                 + (party[who].cls << 1);
            POKE(0x07F8, b);
            POKE(0x07F9, b + 6);        /* (the skins follow the figures) */
        }
    }
    return cgetc();
}

static void hint(unsigned char y, const char *s)
{
    textcolor(COLOR_GRAY2);
    cputsxy(0, y, s);
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
    }
    member_rows(who, 2);
    party_totals(15);
    if (party_count > 1) {
        hint(20, "Cursor left, right  the others");
    }
    hint(21, "Cursor up, down  clothes   K  skin");
    if (party_spells(who) > 1) {
        hint(22, "S  prepare spells");
    }
    hint(24, "Any other key goes back");
}

static void party_look(void)
{
    unsigned char who = 0, key, c, draw = 1;

    for (;;) {
        if (draw) {
            page(who);
            figure(who);
        }
        draw = 1;
        key = fig_key(who);
        c = OWG_DRESS(G->tunic[who]);
        switch (key) {
        case K_CRSR_RIGHT:
            who = who + 1 == party_count ? 0 : who + 1;
            continue;
        case K_CRSR_LEFT:
            who = who ? who - 1 : party_count - 1;
            continue;
        case K_CRSR_DOWN:
            c = c == 15 ? 1 : c + 1;
            break;
        case K_CRSR_UP:
            c = c <= 1 ? 15 : c - 1;
            break;
        default:
            if ((key & 0x7F) == 'k') {
                /* The skin: round every colour but black, the outline. */
                c = OWG_SKIN(G->tunic[who]);
                c = c == 15 ? 1 : c + 1;
                G->tunic[who] = (c << 4) | OWG_DRESS(G->tunic[who]);
                colours(who);
                draw = 0;
                continue;
            }
            if ((key & 0x7F) == 's' && party_spells(who) > 1) {
                while (party_prepare(who)) {
                    page(who);
                }
                continue;
            }
            goto done;
        }
        /* the clothes: only the figure changes */
        G->tunic[who] = (G->tunic[who] & 0xF0) | c;
        colours(who);
        draw = 0;
    }
done:
    VIC.spr_ena = 0;            /* (the disk's icon, ow_disk.s, sets the
                                   rest of the VIC's sprites as it wants) */
}

#pragma code-name (pop)

/* ?: the open world's keys (the game's help.c has the game's), and what
   the quest wants next (quest.c's, as the game's help screen has it). */
static void help(void)
{
    unsigned char stage = quest_stage > Q_VICTORY ? Q_VICTORY : quest_stage;

    textcolor(COLOR_YELLOW);
    cputsxy(1, 0, "Quest for the King");
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
    cputsxy(3, 6, "Step onto a ship, a town, a dungeon");
    cputsxy(3, 8, "P  The worst hurt drinks a draught");
    cputsxy(3, 9, "L  Look: a chest, something hidden");
    cputsxy(3, 10, "I  The party, what they carry");
    cputsxy(3, 11, "F1 Save          F3 Restore");
    cputsxy(3, 12, "M  Music   SHIFT+M  Effects");
    cputsxy(3, 13, "?  This screen    SPACE  Party lines");
    cputsxy(3, 14, "RUN/STOP  Quit to the title");
    cputsxy(3, 16, "Each in turn: walk into foe to strike");
    cputsxy(3, 17, "^+dir shoots; * (a wizard's ^) casts");
    cputsxy(3, 18, "SPACE guards; all flee by a side gap");
    textcolor(COLOR_LIGHTGREEN);
    cputsxy(1, 21, quest_task[stage][0]);
    cputsxy(1, 22, quest_task[stage][1]);
    textcolor(COLOR_GRAY2);
    cputsxy(1, 24, "Press any key");
    cgetc();
}

int main(void)
{
    unsigned char mode = OWB->mode;

    if (mode < OWB_SHOP || mode > OWB_HELP ||
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
    /* rand() starts from the same seed in every program, so every visit
       would hear the same greeting and the same rumour: the game's turn
       and seed make it another one each time, and the same in a test. */
    srand(game_turn ^ ((unsigned int)lore_seed << 8));
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
        strcpy(eng_text, OWB->arg < NUM_INNS ? inn_names[OWB->arg]
                                             : town_gen_inn());
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
    }
    owt_music_off();            /* paused, for the engine to carry on with */
    owg_out();
    ow_back();                  /* the engine, which carries on from the
                                   game block (owgame.h) */
    return 0;
}
