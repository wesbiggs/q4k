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
 *   +        how many wanderers come at once, 1 to MAX_FOES (keepers
 *            and Maldrath always come alone)
 *   F1       hero level, 1-10, which also sets full health
 *   F3/F5/F7 weapon, armour, bow
 *
 * The hero is healed and given three draughts before every fight, and
 * whatever a fight changed -- experience, gold, a level gained -- is put
 * back afterwards.
 *
 *   *        play by itself (below): what the tuning in battle.c rests on
 *
 *   make run PROG=arena
 *
 * With `*` the arena plays the fight it is poked to, auto_runs times, with
 * the fight's own autoplayer (FIGHT_BOT in battle.c, a copy of the fight
 * compiled for the arena alone) at the keys, and counts what happened in
 * res_*, then stops at probe stage 2. tools/balance.py pokes it and reads
 * them back.
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
#define K_PLUS 0x2B
#define K_F1  0x85
#define K_F3  0x86
#define K_F5  0x87
#define K_F7  0x88

#define MAX_LEVEL 10
#define LIST_Y    2

unsigned char level = 1, ground = 0, group = 1;

/* The autoplayer's fight: menu entry auto_mon, auto_runs times, by a party
   of auto_n whose classes and arms are auto_cls/w/a/b, with auto_draughts.
   A wizard knows every book the level allows, and prepares as many as the
   level's slots in the order of book_pick. All poked: tools/balance.py. */
unsigned char auto_mon, auto_runs = 20, auto_n = 1, auto_draughts = 10;
unsigned char auto_cls[MAX_MADE] = { CL_WARRIOR, CL_ARCHER, CL_WIZARD };
unsigned char auto_w[MAX_MADE], auto_a[MAX_MADE], auto_b[MAX_MADE];
unsigned int  res_won, res_lost, res_stall, res_draughts, res_rounds;
extern unsigned char bot_on;
extern unsigned int  bot_rounds, bot_falls;

static const unsigned char book_level[NUM_SPELLS] = { 1, 1, 2, 3, 5, 7 };
static const unsigned char book_pick[NUM_SPELLS] = {
    SP_SPARK, SP_HEAL, SP_BOLT, SP_BLAZE, SP_WARD, SP_SLEEP,
};
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
    if (group > 1) {                /* short: the map column is 21 wide */
        textcolor(COLOR_GRAY1);
        cputs(" x");
        textcolor(COLOR_WHITE);
        cputc('0' + group);
    }

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
    party_level  = level;
    party_xp     = 0;
    party_grow();                         /* as award() grows them */
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

static unsigned char fight(unsigned char n)
{
    if (n < num_wanderers) {
        return battle_fight(entry(n), arena_tile[ground], 1, group);
    } else if (n < num_wanderers + NUM_DUNGEONS) {
        return battle_fight(entry(n), T_FLOOR, 0, 1);
    }
    return battle_fight(entry(n), T_LAIR, 0, 1);
}

static const char *const bot_names[MAX_MADE] = { "Ana", "Bo", "Cy" };

static void auto_kit(void)
{
    unsigned char i, k, n;
    Actor *a;

    party_count = 0;
    party_level = level;
    party_xp    = 0;
    for (i = 0; i < auto_n; ++i) {
        party_make(i, bot_names[i], auto_cls[i]);
        a = &party[i];
        a->weapon = auto_w[i];
        a->armour = auto_a[i];
        a->bow    = auto_b[i];
        if (a->cls == CL_WIZARD) {
            a->known = a->ready = 0;
            for (k = 0, n = 0; k < NUM_SPELLS; ++k) {
                if (book_level[book_pick[k]] <= level) {
                    a->known |= 1 << book_pick[k];
                    if (n < spell_slots()) {
                        a->ready |= 1 << book_pick[k];
                        ++n;
                    }
                }
            }
        }
    }
    party_rest();
    potions = auto_draughts;
    arrows  = ARROW_MAX;
    gold    = 0;
}

static void autoplay(void)
{
    unsigned char r, run;

    bot_on = 1;
    for (run = 0; run < auto_runs; ++run) {
        auto_kit();
        bot_rounds = 0;
        rng_stir(run);
        reseed();
        r = fight(auto_mon);
        if (r == BATTLE_WON) {
            ++res_won;
        } else if (r == BATTLE_DEAD) {
            ++res_lost;
        } else {
            ++res_stall;
        }
        res_draughts += auto_draughts - potions;
        res_rounds   += bot_rounds;
    }
    bot_on = 0;
    probe_stage(2);
    for (;;) {
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
    view_message("+ sets how many wanderers come.");

    for (;;) {
        kit_out();
        draw_menu();
        probe_stage(1);

        k = key();
        if (k >= 0xC1 && k <= 0xDA) {
            k -= 0x80;               /* fold SHIFTed letters onto unshifted */
        }

        if (k == 0x2A) {             /* * */
            autoplay();
        } else if (k >= K_A && k < K_A + count) {
            fight(k - K_A);
        } else if (k >= K_1 && k < K_1 + NUM_ARENAS) {
            ground = k - K_1;
        } else if (k == K_PLUS) {
            group = group % MAX_FOES + 1;
        } else if (k == K_F1) {
            level = level % MAX_LEVEL + 1;
        } else if (k == K_F3) {
            hero.weapon = (hero.weapon + 1) % NUM_WEAPONS;
        } else if (k == K_F5) {
            hero.armour = (hero.armour + 1) % NUM_ARMOUR;
        } else if (k == K_F7) {
            hero.bow = (hero.bow + 1) % NUM_BOWS;
            arrows = ARROW_MAX;
        }
    }
    /* not reached; the loop is the program */
}
