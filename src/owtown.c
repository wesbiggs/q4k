/*
 * Quest for the King -- the open world's town: its screens of text.
 *
 * The smooth-scrolling prototype's engine (src/ow/engine.c) has no room for
 * anything but the map, so the screens of text are this program, loaded
 * over the engine when they are wanted (ow_run), with the hand-off block
 * (q4k/ow.h's OWB) saying what for:
 *
 *   OWB_NEW, OWB_QUICK  the party's making, after the splash screen's F1
 *                       (src/lib/ow_party.c), which ends in Millford
 *   OWB_TALK            a conversation, when the hero walks into a
 *                       townsperson: the offline talk (src/lib/talk.c,
 *                       which reads each person's lines from the disk),
 *                       and what it hands over (src/lib/gift.c), which
 *                       moves the quest on
 *   OWB_SHOP            a shop (src/lib/shop.c), arg its kind (SHOP_*)
 *   OWB_INN             an inn (src/lib/inn.c): ale and a rumour from the
 *                       inn's file on the disk, or beds; arg which inn
 *   OWB_PARTY           I: the party, what they carry, a wizard's spells
 *
 * It reads and writes the game through the game block (q4k/owgame.h), and
 * when it is done the engine is loaded back (ow_back()) and carries on from
 * there: in the same town, where the hero stood. Everything the engine
 * keeps is rebuilt from the disk on the way (OW-DATA, which this program
 * writes over, is loaded again), or is still in memory: the vault under the
 * I/O area and the music under the KERNAL are not reached. Linked like the
 * test programs, with cfg/c64-flat.cfg (one load).
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
#include <q4k/tiles.h>
#include <q4k/town_data.h>

/* What the game's modules here read and write of game.c's and sprite.c's:
   defined here instead, as the engine does, so that neither is linked, nor
   the map, the land generator and the vault that game.c brings with it.
   The game block (owgame.h) is where they come from and go back to. */
unsigned char land, local_w, local_h, depth;
unsigned int  game_turn, gold;
unsigned char potions, arrows, quest_stage, items, lore_seed;
unsigned char looted[LOOTED_BYTES];
unsigned char tmap[TM_SIZE];
unsigned char home_biome;
const TownEnt *here_ents;
unsigned char  here_n, here_ex, here_ey;

unsigned char has(unsigned char item)
{
    return items & item;
}

void give(unsigned char item)
{
    items |= item;
}

/* Which continent holds which story place (the shops' tiers follow it):
   game.c's, from the seed. */
unsigned char slot_of(unsigned char d)
{
    unsigned char p[4], i, j, t, v = lore_seed;

    for (i = 0; i < 4; ++i) {
        p[i] = i;
    }
    for (i = 3; i > 0; --i) {
        v = v * 5 + 3;
        j = (v >> 5) % (i + 1);
        t = p[i]; p[i] = p[j]; p[j] = t;
    }
    return p[d];
}

void sprite_hide(void)
{
    VIC.spr_ena = 0;
}

/* And of savegame.c's, music.s's and view.c's, for the same reason: the
   drive is unit 8, there is no music here to pause (the engine's player is
   not running), and no map to put a message under. */
unsigned char save_device = 8;
void music_pause(void) {}
void music_resume(void) {}
void __fastcall__ view_message(const char *s)
{
    (void)s;
}

/* A line said on a screen of its own: what a person handed over when
   there was nothing to read of theirs. Any key goes on. */
static void note_screen(const char *s)
{
    clrscr();
    textcolor(COLOR_YELLOW);
    cputsxy(1, 10, s);
    textcolor(COLOR_GRAY2);
    cputsxy(1, 12, "Press a key.");
    cgetc();
}

/* A conversation, and whatever it hands over (the game's quest_after_talk(),
   gift.c): the Sigil, the keys, Garrett's arms and Dragonbane, the king's
   reward. With nothing on the disk for the person, what they would have
   given is given all the same, as the game does. */
static void talk(unsigned char npc)
{
    const char *note;

    talking_to = npc;
    if (dialog_talk(npc, npc_names[npc], quest_after_talk) != DIALOG_OK) {
        note = quest_after_talk(GRANT_OPEN);
        if (note) {
            note_screen(note);
        }
    }
}

int main(void)
{
    unsigned char mode = OWB->mode;

    if (mode < OWB_TALK || mode > OWB_PARTY || mode == OWB_BACK) {
        cputs("Load OPENWORLD: it builds the world.");
        for (;;) {
        }
    }

    tiles_install();            /* the character set, before any text */
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
    switch (mode) {
    case OWB_TALK:
        talk(OWB->npc);
        break;
    case OWB_SHOP:              /* the game's shops (shop.c) */
        eng_a = OWB->arg;
        shop_entry();
        break;
    case OWB_INN:               /* ale and a rumour, or beds (inn.c) */
        strcpy(eng_text, OWB->arg < NUM_INNS ? inn_names[OWB->arg]
                                             : town_gen_inn());
        inn_open(OWB->arg);
        break;
    case OWB_PARTY:             /* who they are and what they carry */
        eng_a = SHOP_PARTY;
        shop_entry();
        break;
    default:
        ow_new_game(mode == OWB_QUICK);
        break;
    }
    if (G->magic == OWG_MAGIC) {
        owg_out();
    }
    ow_back();                  /* the engine, which carries on from the
                                   game block (owgame.h) */
    return 0;
}
