/*
 * Quest for the King -- the open world's conversations.
 *
 * When the hero walks into a townsperson the engine (src/ow/engine.c)
 * leaves for this program with OWB_TALK and the person in OWB->npc
 * (q4k/ow.h): the offline talk (src/lib/talk.c, which reads each person's
 * lines from the disk) and what it hands over (src/lib/gift.c), which moves
 * the quest on. The shops, the inns, the party, the help screen and the
 * fights are owtown's (src/owtown.c); the talk is a program of its own so
 * that it has the room to grow (a portrait of the speaker, in VIC bank 1,
 * which cfg/c64-owtalk.cfg keeps free).
 *
 * The music the engine was playing carries on here where it paused
 * (src/lib/ow_play.s). The game comes from the game block (q4k/owgame.h)
 * and goes back to it, and then the engine is loaded back (ow_back()) and
 * carries on, in the same town, where the hero stood. The game's own
 * variables and what talk.c and gift.c use of game.c's are
 * src/ow/owstand.c's, as owtown's are.
 */

#include <conio.h>
#include <c64.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/charset.h>
#include <q4k/dialog.h>
#include <q4k/game.h>
#include <q4k/gift.h>
#include <q4k/ow.h>
#include <q4k/owgame.h>
#include <q4k/tiles.h>
#include <q4k/town_data.h>

/* The music the engine left paused, carried on (src/lib/ow_play.s). */
void owt_music_on(void);
void owt_music_off(void);

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
    if (OWB->mode != OWB_TALK) {
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
       would hear the same greeting: the game's turn and seed make it
       another one each time, and the same in a test. */
    srand(game_turn ^ ((unsigned int)lore_seed << 8));
    land = G->land;
    owt_music_on();
    talk(OWB->npc);
    owt_music_off();            /* paused, for the engine to carry on with */
    owg_out();
    ow_back();                  /* the engine, which carries on from the
                                   game block (owgame.h) */
    return 0;
}
