/*
 * Open world: the party's status in the text window, and the few things
 * the engine says or asks that came after its memory was full.
 *
 * All of it is in OWLO, the engine's file below $0801 (cfg/c64-openworld.cfg:
 * the KERNAL's text screen, which the engine does not show), so that MAIN
 * and the overlays pay a call each. What is heavy is in assembly, stlow.s.
 *
 *  - The two lines under the map show the party as a fight does (its rows
 *    20 and 21: each member's name, then health and a wizard's magic; and
 *    the draughts and arrows in the last column if there is room). Anything
 *    said takes the window over (sm_say()), and the status comes back with
 *    the next step. SPACE hides it and shows it again (OWG_HIDE, kept in
 *    the game block); something said while it is hidden shows it again
 *    from the next step on.
 *  - A new game opens with a greeting.
 *  - F1 and F3 ask first, Y or N, as the game does.
 *  - A dungeon's chest is opened by standing on it, if it was never taken,
 *    and an emptied one is drawn open (chestopen, in WORLD only: local[]
 *    keeps the chest, so that L there says it is empty).
 *  - The keeper idles between two frames, as everyone else does: his
 *    characters swapped with keeperbob's (assets/dungeon_tiles.txt).
 */

#include <c64.h>
#include <stddef.h>

#include <q4k/actor.h>
#include <q4k/ow.h>
#include <q4k/owgame.h>
#include <q4k/tiles.h>

#include "engine.h"

#pragma code-name ("OWLO")
#pragma rodata-name ("OWLO")

/* The rest of the engine's, made public for this. */
void __fastcall__ sm_say_raw(const char *s);
unsigned char __fastcall__ yes(const char *q);
void save_game(void);
void __fastcall__ rebuild(unsigned char mode);

/* stlow.s's, which has these by number. */
void st_draw(void);
extern unsigned char st_up, msg_up;
typedef char st_g[2 * (OWG_ADDR == 0xCE3F) - 1];
typedef char st_count[2 * (offsetof(OwGame, party_count) == 86) - 1];
typedef char st_potions[2 * (offsetof(OwGame, potions) == 94) - 1];
typedef char st_arrows[2 * (offsetof(OwGame, arrows) == 95) - 1];
typedef char st_actor[2 * (sizeof(Actor) == 21) - 1];
typedef char st_hp[2 * (offsetof(Actor, hp) == 2) - 1];
typedef char st_name[2 * (offsetof(Actor, name) == 4) - 1];
typedef char st_mp[2 * (offsetof(Actor, mp) == 17) - 1];
typedef char st_mpmax[2 * (offsetof(Actor, mp_max) == 18) - 1];
typedef char st_chest[2 * (T_CHEST == 0x70) - 1];
typedef char st_monster[2 * (T_MONSTER == 0x8E) - 1];
typedef char st_dungeon[2 * (AREA_DUNGEON == 3) - 1];
typedef char st_bias[2 * (BIAS == 32) - 1];

static const char s_hail[]   = "Hail, adventurers! Press ? for help.";
static const char s_save_q[] = "Save over the saved game? Y or N";
static const char s_load_q[] = "Load the saved game? Y or N";

/* The party under the map, unless SPACE hid it. */
void st_show(void)
{
    if (!(G->quiet & OWG_HIDE)) {
        st_draw();
    }
}

/* Everything said goes through here: the status off the window first, and
   back (shown, even if it was hidden) at the next step: st_step(). */
void __fastcall__ sm_say(const char *s)
{
    if (st_up) {
        st_up = 0;
        sm_text_clear();
    }
    msg_up = 1;
    sm_say_raw(s);
}

/* A step begins: the status back over what was said. */
void st_step(void)
{
    if (msg_up) {
        msg_up = 0;
        G->quiet &= ~OWG_HIDE;
        st_show();
    }
}

/* SPACE. */
void st_toggle(void)
{
    msg_up = st_up = 0;
    G->quiet ^= OWG_HIDE;
    sm_text_clear();
    st_show();
}

/* The engine has the screen: a new game's greeting (nothing has happened
   in it yet), or the status. */
void st_start(void)
{
    if (!G->game_turn) {
        sm_ink = COLOR_YELLOW;
        sm_say(s_hail);
    } else {
        st_show();
    }
}

/* F1 and F3, asked first. */
void st_save(void)
{
    if (yes(s_save_q)) {
        save_game();
    } else {
        st_show();
    }
}

void st_load(void)
{
    if (yes(s_load_q)) {
        rebuild(OWB_LOAD);
    }
    st_show();
}
