/*
 * Arena: pick a fight with anything, on any ground.
 *
 * Every monster in the game is on the menu, keepers and Maldrath included,
 * so a new one -- or a change to how they fight -- can be tried out without
 * walking the overworld until the right thing turns up. The fight is the
 * game's own, from battle.c, not a copy of it.
 *
 *   a-o      fight that monster
 *   1-6      the ground a wanderer is met on (keepers are always met in
 *            a dungeon room and Maldrath in his lair, as in the game)
 *   F1       hero level, 1-10, which also sets full health
 *   F3/F5/F7 weapon, armour, bow
 *
 * The hero is healed and given three draughts before every fight, and
 * whatever a fight changed -- experience, gold, a level gained -- is put
 * back afterwards.
 *
 *   make run PROG=arena
 */

#include <conio.h>
#include <c64.h>

#include <q4k/actor.h>
#include <q4k/arena_data.h>
#include <q4k/battle.h>
#include <q4k/game.h>
#include <q4k/probe.h>
#include <q4k/sprite.h>
#include <q4k/tiles.h>
#include <q4k/view.h>

#define K_A   0x41      /* PETSCII a, unshifted */
#define K_1   0x31
#define K_F1  0x85
#define K_F3  0x86
#define K_F5  0x87
#define K_F7  0x88

#define MAX_LEVEL 10
#define LIST_Y    2

static unsigned char level = 1, ground = 0;
static unsigned char count;      /* monsters on the menu */

/* Menu entry n: the wanderers in order, then the keepers, then Maldrath. */
static const Monster *entry(unsigned char n)
{
    if (n < num_wanderers) {
        return &wanderers[n].m;
    }
    n -= num_wanderers;
    return n < NUM_DUNGEONS ? &keepers[n] : &maldrath;
}

static void draw_menu(void)
{
    const Monster *m;
    unsigned char n;

    view_restore();
    sprite_hide();

    textcolor(COLOR_YELLOW);
    cputsxy(0, 0, "ARENA");
    textcolor(COLOR_GRAY1);
    cputs(" on ");
    textcolor(COLOR_WHITE);
    cputs(tile_names[arena_tile[ground] - TILE_BASE]);

    for (n = 0; n < count; ++n) {
        m = entry(n);
        textcolor(COLOR_GRAY1);
        cputcxy(0, LIST_Y + n, K_A + n);
        textcolor(m->color);
        cputsxy(2, LIST_Y + n, m->name);
    }

    /* The panel, with the fight's leftovers cleared from where it names
       the foe. */
    view_draw_panel();
    cclearxy(PANEL_X, 10, PANEL_W);
    cclearxy(PANEL_X, 11, PANEL_W);
}

/* Put the hero back as the menu has them: this level, full health, no
   spoils from the last fight. */
static void kit_out(void)
{
    hero.level  = level;
    hero.hp_max = 20 + 6 * (level - 1);   /* as award() grows it */
    hero.xp     = 0;
    heal_full();
    potions = 3;
    gold    = 0;
}

/* Wait for a key, stirring the random numbers with how long it took, as
   the game does -- or every fight would open the same way. */
static unsigned char key(void)
{
    unsigned int spins = 0;

    while (!kbhit()) {
        ++spins;
    }
    rng_stir(spins);
    reseed();
    return cgetc();
}

static void fight(unsigned char n)
{
    if (n < num_wanderers) {
        battle_fight(entry(n), arena_tile[ground], 1);
    } else if (n < num_wanderers + NUM_DUNGEONS) {
        battle_fight(entry(n), T_FLOOR, 0);
    } else {
        battle_fight(entry(n), T_LAIR, 0);
    }
}

int main(void)
{
    unsigned char k;

    tiles_install();    /* before game_init() writes over the font */
    game_init();
    view_init();
    count = num_wanderers + NUM_DUNGEONS + 1;

    while (kbhit()) {   /* the launcher's RUN and RETURN are not input */
        cgetc();
    }

    view_message("A letter fights. 1-6 picks the ground.");
    view_message("F1 level, F3 weapon, F5 armour, F7 bow.");

    for (;;) {
        kit_out();
        draw_menu();
        probe_stage(1);

        k = key();
        if (k >= 0xC1 && k <= 0xDA) {
            k -= 0x80;               /* fold SHIFTed letters onto unshifted */
        }

        if (k >= K_A && k < K_A + count) {
            fight(k - K_A);
        } else if (k >= K_1 && k < K_1 + NUM_ARENAS) {
            ground = k - K_1;
        } else if (k == K_F1) {
            level = level % MAX_LEVEL + 1;
        } else if (k == K_F3) {
            weapon_lv = (weapon_lv + 1) % NUM_WEAPONS;
        } else if (k == K_F5) {
            armour_lv = (armour_lv + 1) % NUM_ARMOUR;
        } else if (k == K_F7) {
            bow_lv = (bow_lv + 1) % NUM_BOWS;
        }
    }
    /* not reached; the loop is the program */
}
