/*
 * q4k -- a grid RPG roguelike for the Commodore 64.
 *
 * Overworld walkabout: wrapping 64x64 map, centred viewport, terrain-aware
 * movement. One hero, steered through the party array so a full party can be
 * added without touching the movement or drawing code.
 */

#include <conio.h>
#include <c64.h>

#include <q4k/actor.h>
#include <q4k/charset.h>
#include <q4k/dialog.h>
#include <q4k/probe.h>
#include <q4k/tiles.h>
#include <q4k/view.h>
#include <q4k/world.h>

unsigned int game_turn;

/* PETSCII key codes, spelled numerically on purpose: cc65 translates C
   character literals to PETSCII at compile time, and relying on that for
   comparisons against raw key codes is a good way to get a subtle bug. */
#define K_CRSR_DOWN   0x11
#define K_CRSR_RIGHT  0x1D
#define K_CRSR_UP     0x91
#define K_CRSR_LEFT   0x9D
#define K_SPACE       0x20
#define K_W           0x57
#define K_A           0x41
#define K_S           0x53
#define K_D           0x44
#define K_Q           0x51
#define K_T           0x54

/* Who lives where. The npc id goes on the wire to the dialogue daemon (see
   assets/npcs.json); the place name is only ever printed. */
typedef struct {
    unsigned char x, y;
    const char   *npc;
    const char   *place;
} Place;

static const Place places[] = {
    { 29, 10, "fisher",  "WARDLOW -- OLD BRANN"        },
    { 40, 19, "granger", "GREENHOLLOW -- GRANGER HESK" },
    { 16, 23, "collier", "MILLBROOK -- COLLIER REN"    },
    { 53, 43, "reeve",   "STONEREACH -- REEVE ALDA"    },
    { 28, 51, "steward", "THE CASTLE -- STEWARD MERRIT" },
};

#define NUM_PLACES (sizeof(places) / sizeof(places[0]))

/* Mirror the hero's state into the probe page so the headless harness can
   assert on it. Stage doubles as a turn counter, which is what lets a test
   say "run three moves, then show me the screen". */
static void publish(void)
{
    PROBE_SCRATCH[0] = hero.x;
    PROBE_SCRATCH[1] = hero.y;
    PROBE_SCRATCH[2] = world_tile(hero.x, hero.y);
    PROBE_COUNTER    = (unsigned char)game_turn;
    probe_stage(game_turn < 254 ? (unsigned char)game_turn + 1 : 255);
}

/* The launcher types RUN and RETURN to start us; drop anything still queued
   so it is not read as the player's first move. */
static void drain_keyboard(void)
{
    while (kbhit()) {
        cgetc();
    }
}

static void arrive(unsigned char code)
{
    switch (code) {
        case T_TOWN:    view_message("A TOWN. PRESS T TO TALK.");     break;
        case T_CASTLE:  view_message("THE CASTLE. PRESS T TO TALK.");  break;
        case T_DUNGEON: view_message("A DARK OPENING IN THE ROCK.");   break;
        default:                                                     break;
    }
}

/* Talk to whoever is at the hero's feet, over the modem port. */
static void try_talk(void)
{
    unsigned char i;

    for (i = 0; i < NUM_PLACES; ++i) {
        if (places[i].x == hero.x && places[i].y == hero.y) {
            dialog_talk(places[i].npc, places[i].place);
            view_restore();
            view_message("YOU TAKE YOUR LEAVE.");
            ++game_turn;
            return;
        }
    }
    view_message("THERE IS NOBODY HERE TO TALK TO.");
}

static void try_move(signed char dx, signed char dy)
{
    unsigned char nx   = (hero.x + dx) & WORLD_MASK;
    unsigned char ny   = (hero.y + dy) & WORLD_MASK;
    unsigned char code = world_tile(nx, ny);

    switch (tile_flags[code - TILE_BASE]) {
        case TF_BLOCK:
            /* Bumping into terrain costs no turn -- roguelike convention,
               and it keeps misclicks from burning food. */
            view_message("BLOCKED.");
            return;

        case TF_WATER:
            view_message("THOU CANST NOT SWIM.");
            return;

        case TF_ENTER:
            hero.x = nx;
            hero.y = ny;
            arrive(code);
            break;

        default:
            hero.x = nx;
            hero.y = ny;
            break;
    }

    ++game_turn;
}

int main(void)
{
    unsigned char key;

    party_init();
    view_init();
    drain_keyboard();
    view_message("THOU ART IN THE LAND OF Q4K.");
    view_message("MOVE: W A S D OR CURSOR KEYS.");
    view_message("T TO TALK, SPACE TO WAIT, Q TO QUIT.");

    for (;;) {
        view_draw_map();
        view_draw_panel();
        publish();

        key = cgetc();
        PROBE_SCRATCH[3] = key;   /* last key read, for the test harness */
        if (key >= 0xC1 && key <= 0xDA) {
            key -= 0x80;            /* fold SHIFTed letters onto unshifted */
        }

        switch (key) {
            case K_W: case K_CRSR_UP:    try_move( 0, -1); break;
            case K_S: case K_CRSR_DOWN:  try_move( 0,  1); break;
            case K_A: case K_CRSR_LEFT:  try_move(-1,  0); break;
            case K_D: case K_CRSR_RIGHT: try_move( 1,  0); break;

            case K_T:
                try_talk();
                break;

            case K_SPACE:
                view_message("THOU WAITEST.");
                ++game_turn;
                break;

            case K_Q:
                VIC.addr = 0x15;    /* hand the ROM charset back to BASIC */
                clrscr();
                return 0;

            default:
                break;
        }
    }
}
