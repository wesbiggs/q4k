/*
 * Quest for the King -- what the game's modules read and write of game.c's,
 * sprite.c's, savegame.c's, music.s's and view.c's, for the open world's
 * screens of text: owtown (src/owtown.c: shops, inns, the party, help, a
 * fight) and owtalk (src/owtalk.c: a conversation). Defined here instead,
 * as the engine does, so that none of those is linked, nor the map, the
 * land generator and the vault that game.c brings with it. The game block
 * (owgame.h) is where they come from and go back to.
 *
 * Not in the library: game.o defines the same names there, and which of
 * the two a program got would depend on the order the linker meets them.
 * The Makefile links this object into both programs, ahead of the library.
 */

#include <c64.h>

#include <q4k/game.h>
#include <q4k/ow.h>
#include <q4k/savegame.h>
#include <q4k/town_data.h>

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

void sprite_hide(void)
{
    VIC.spr_ena = 0;
}

/* The music the engine left paused, carried on (src/lib/ow_play.s). */
void owt_music_pause(void);
void owt_music_resume(void);

/* The drive is unit 8, the music is ow_play.s's (the talk pauses it round
   the disk), and there is no map to put a message under. */
unsigned char save_device = 8;
void music_pause(void)
{
    owt_music_pause();
}
void music_resume(void)
{
    owt_music_resume();
}
/* A talk's file (savegame.h): from the REU's cache with the music playing
   on, or from the disk with it paused round the drive (and stashed, for
   the next time). */
unsigned char __fastcall__ talk_load(const char *name, void *at)
{
    if (ow_cached(name)) {
        return ow_cload(name, at) != 0;
    }
    owt_music_pause();
    if (ow_cload(name, at)) {
        owt_music_resume();
        return 1;
    }
    owt_music_resume();
    return 0;
}
void __fastcall__ view_message(const char *s)
{
    (void)s;
}
