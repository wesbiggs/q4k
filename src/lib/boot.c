/*
 * q4k -- the game's main(): start-up, then the map's engine.
 *
 * The map loop is map_entry(), in src/q4k.c, which is in the map's engine on
 * the disk, so main() is here in the core: it runs the title screen (which the
 * PRG brings in the slot), loads the map's engine over it, and hands over for
 * good. See engine.h.
 */

#include <q4k/actor.h>
#include <q4k/engine.h>
#include <q4k/game.h>
#include <q4k/music.h>
#include <q4k/savegame.h>
#include <q4k/tiles.h>
#include <q4k/title.h>
#include <q4k/view.h>

int main(void)
{
    unsigned char restored;

    save_init();                    /* $BA is where the game came from */
    tiles_install();                /* before anything writes local[] */
    game_init();                    /* a party of one, until the title's */
    restored = title_screen();
    if (!restored) {
        potions = party_count;      /* a draught for each who answered */
    }

    /* The title screen and what installed the charset are gone after this:
       the slot is the map's from here on, bar a fight or a conversation. */
    engine_load(ENG_MAP);
    view_init();
    music_start();
    map_entry(restored);
    return 0;
}
