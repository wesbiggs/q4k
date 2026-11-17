/*
 * Open world: a prototype of smooth scrolling for q4k -- its engine.
 *
 * Keldmarch and the continents across the sea, built by the game's own
 * generator from a seed, walked and sailed with the camera gliding a pixel or
 * two a frame instead of jumping a square at a time: a multicolour tile set
 * (assets/overworld.txt, 16 by 16 pixels a square) on a double-buffered
 * screen, with colour RAM rewritten ahead of the beam. smooth.s explains how.
 *
 * Step onto Millford, the castle, the Hollow Crown or a town to go in: each
 * has a tile set of its own (assets/town_tiles.txt and the rest), its people
 * are sprites, and its walls have faces. Walk into someone with something to
 * say and they say it (src/owtown.c).
 *
 * This is the core: the main loop, the music, and what every area does. The
 * overworld and the dungeons (world.c) and the towns (town.c) are overlays
 * that take turns in one slot of memory, loaded from the disk as the hero
 * goes in and out: engine.h.
 *
 * Built twice, as OW-REU (OW_REU: the REU shifts the screens and keeps the
 * overworld while you are in a place) and OW-CPU (the CPU does it all), and
 * loaded by the bootstrap, src/openworld.c, which builds the lands first and
 * leaves the rest of what it found in the hand-off block (q4k/ow.h). The
 * game itself is untouched: this is where the ideas are tried before they go
 * near its memory map.
 *
 *   cursor keys, WASD or a joystick in port 2   walk
 *   Q, E, Z, X (or the stick's diagonals)       walk diagonally
 *   1 / 2                                        one or two pixels a frame
 *   M                                            the music off and on
 *   SHIFT+M                                      the sound effects off and on
 *   I                                            the party, what they carry
 *                                                and a wizard's spells
 *   P                                            the worst hurt drinks a
 *                                                draught (a gulp, or a
 *                                                refusal if nobody can)
 *   F1                                           save (OW-SAVE): the border is
 *                                                light blue while the drive
 *                                                works, red if it would not
 *   F3                                           the saved game (back through
 *                                                the bootstrap, for its world)
 *   L                                            look: a chest, the town's
 *                                                hidden gold, or the dead
 *   ?                                            the keys, and the quest
 *   RUN/STOP                                     quit, to the title (Y or N)
 *   N                                            the next seed's world (back
 *                                                through the bootstrap)
 *
 * What is said is said in the two lines of text under the map (smooth.s's
 * text window, sm_say()).
 *
 * For tests, through tools/screendump.py: steps can be typed (w, a, s, d),
 * stage 2 is published whenever the hero stands still with nothing typed
 * left, and --poke freeze_at=N stops the world N pixels into a step (the
 * first, or the one after freeze_after steps), at stage 3, so a
 * half-scrolled screen can be looked at.
 */

#include <c64.h>
#include <cbm.h>
#include <errno.h>
#include <conio.h>
#include <peekpoke.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/dungeon.h>
#include <q4k/game.h>
#include <q4k/isle.h>
#include <q4k/music.h>
#include <q4k/music_map.h>
#include <q4k/ow.h>
#include <q4k/town_data.h>
#include <q4k/probe.h>
#include <q4k/sprite.h>
#include <q4k/tiles.h>

#include "engine.h"

/* Where you are: the overworld, or a place, each in its own tile set. A
   place is built the game's way (town_data.c's maps, town_gen(), and
   dungeon_build()) into local[], and copied into WORLD for the engine; the
   overworld is put away while you are in one -- in the REU (OW-REU), or
   else built again from the vault on the way out (OW-CPU). */
unsigned char area;             /* (and game.c's depth: a dungeon's level) */
unsigned char area_w = 64, area_h = 64;

unsigned int  out_wx;           /* where you went into a place */
unsigned char out_wy;
unsigned int  ship_x[SHIPS];
unsigned char ship_y[SHIPS];
unsigned char boat;             /* 1 + the ship you are in, or 0 */

unsigned char town_index;       /* the town to go into, or that you are in */
unsigned char resuming, resume_x, resume_y;     /* back from a talk */
unsigned char dun_index;        /* the dungeon to go into, or that you are in */

/* The overlays (engine.h): the files, by the engine's name, since each
   engine has its own set. In DATA, not RODATA, which is OW-DATA and must
   be the same bytes in both engines. */
#ifdef OW_REU
static char world_file[]   = "ow-reu.world";
static char town_file[]    = "ow-reu.town";
static char dungeon_file[] = "ow-reu.dungeon";
static char hi_file[]      = "ow-reu.hi";
#else
static char world_file[]   = "ow-cpu.world";
static char town_file[]    = "ow-cpu.town";
static char dungeon_file[] = "ow-cpu.dungeon";
static char hi_file[]      = "ow-cpu.hi";
#endif
unsigned char ov_want, ov_have;

/* What the game's own modules here (the town and dungeon generators, the
   vault) read and write of game.c's, actor.c's and the land generator's:
   defined here instead, so that none is linked, nor the view, the sprite
   and the arms they bring with them -- about 6K the prototype has no room
   for. (The land generator is linked: its own variables are its own.) */
unsigned char depth, land, local_w, local_h, quest_stage, lore_seed;
unsigned char items, looted[LOOTED_BYTES];
unsigned char home_fx[NUM_HOME_PLACES], home_fy[NUM_HOME_PLACES];
unsigned char home_biome;
const TownEnt *here_ents;
unsigned char  here_n, here_ex, here_ey;
const unsigned char dungeon_deep[NUM_DUNGEONS] = { 3, 3, 4 };
const unsigned char dungeon_prize[NUM_DUNGEONS] = { IT_EMBER, IT_TEAR, IT_HEART };
unsigned char has(unsigned char item)
{
    return items & item;
}
/* (party[] is the game block's: cfg/c64-openworld.cfg names it.) */
typedef char owgame_is_what_mkowsave_writes[2 * (sizeof(OwGame) == OWG_SIZE) - 1];
typedef char owgame_fits_below_the_probe[2 * (OWG_ADDR + OWG_SIZE <= 0xCF00) - 1];

/* Everything said, in the engine's spare memory past the screens (HICODE):
   OW-DATA, where constants go, has little room left, and cc65 puts string
   literals there whatever the rodata pragma says, so they are arrays. */
#pragma rodata-name (push, "HICODE")
#pragma data-name (push, "HICODE")
static char said[41] = { 0 };
static const char s_draught[] = "You find a draught.";
static const char s_find[] = "You find ";
static const char s_draughts[] = " draughts.";
static const char s_gold[] = " gold.";
static const char s_taken[] = "You've already taken everything.";
static const char s_dead[] = "The ground stirs. The dead rise!";
static const char s_nothing[] = "You see nothing.";
static const char s_quit[] = "Quit to the title? Y or N";
static const char s_onwith[] = "On with the quest, then.";
static const char s_mon[] = "Music on.";
static const char s_moff[] = "Music off.";
static const char s_xon[] = "Sound effects on.";
static const char s_xoff[] = "Sound effects off.";
static const char s_saved[] = "The game is saved.";
static const char s_unsaved[] = "The save failed.";
static const char s_nobody[] = "Nobody needs a draught.";
static const char s_nodraughts[] = "No draughts left.";
static const char s_drinks[] = " drinks a draught.";
static const char s_boot[] = "openworld";
#pragma data-name (pop)
#pragma rodata-name (pop)

/* Poke-able. */
unsigned char seed;             /* lore_seed for the land (the bootstrap's) */
unsigned char speed     = 2;    /* pixels a frame: 1 or 2 */
unsigned char freeze_at = 0;    /* stop this many pixels into a step ... */
unsigned char freeze_after = 0; /* ... once this many steps are done */
unsigned char still     = 0;    /* 1: the water does not flow */
unsigned char enc_always = 0;   /* 1: every roll is a fight, for tests */
unsigned char whole     = 0;    /* 1: a roof change redraws everything, for
                                   checking that the box it does is enough */

/* What the screen is doing, for --peek. */
unsigned char hx, hy;           /* the hero's square, as the engine has it:
                                   biased by BIAS in a place, the world's
                                   round 128 on the overworld */
unsigned char late;             /* frames the main loop missed */
unsigned char shore_time;       /* jiffies sm_shores() took */
unsigned char switch_time;      /* jiffies the last way in or out took */
unsigned char roof_time;        /* jiffies the last roof change took */
unsigned int  steps;
unsigned long idle;             /* turns of the waiting loop while walking */
unsigned int  walked;           /* ... over this many frames */

static unsigned char moving, moved;
unsigned char dir, stride;
unsigned char pre_dir = 0xFF;   /* the back screen is shifted that way */
unsigned char frame;            /* the last frame the main loop handled */
/* The step that ended last frame, + 1, until what it brings is seen to (the
   world's ov_after). Not in the frame the step ended, which starts the next
   one too, and has no room for it on a diagonal. */
unsigned char after;

/* Eight ways, a square at a time: sprite.h's four, then the diagonals,
   as smooth.s numbers them. A diagonal faces the way it goes across. */
const signed char dirdx[8] = { 0, 0, -1, 1, -1, 1, -1, 1 };
const signed char dirdy[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
const unsigned char facing[8] = {
    DIR_DOWN, DIR_UP, DIR_LEFT, DIR_RIGHT,
    DIR_LEFT, DIR_RIGHT, DIR_LEFT, DIR_RIGHT
};

static unsigned int camx0, camy0;   /* the camera when the step began */
static unsigned char bumped;        /* 1 + the way last bumped, while held */

/* The back screen's build, in three parts a frame apart (see sm_build):
   which is still to do. */
#define BUILT  0
#define SHIFT  1
#define ROW    2
#define COL    3
static unsigned char phase;

static void build_some(void)
{
    switch (phase) {
    case SHIFT: sm_shift(dir);    phase = ROW;   break;
    case ROW:   sm_edge_row(dir); phase = COL;   break;
    case COL:   sm_edge_col(dir); phase = BUILT; break;
    }
}

/* The hero's frame: walking, or a ship facing the way it last went across. */
static unsigned char ship_face = 1;

unsigned char hero_frame(unsigned char d, unsigned char st)
{
    if (!boat) {
        return facing[d] * 2 + st;
    }
    if (facing[d] == DIR_LEFT) {
        ship_face = 0;
    } else if (facing[d] == DIR_RIGHT) {
        ship_face = 1;
    }
    return 9 + ship_face;
}

void place_camera(void)
{
    /* The square's top left at (148, 84) on the screen: 2 * square - 18.5
       characters, so four pixels into one. */
    sm_ox = hx * 2 - HERO_PX / 8 - 1;
    sm_fx = 8 - (HERO_PX & 7);
    sm_oy = hy * 2 - HERO_PY / 8 - 1;
    sm_fy = 8 - (HERO_PY & 7);
}

/* What is really at a square: in a place, local[], where it was built
   (WORLD has its roofs drawn over it); on the overworld, the window. */
unsigned char tile_at(unsigned char x, unsigned char y)
{
    if (area != AREA_HOME) {
        return local[LOCAL_AT(x - BIAS, y - BIAS)];
    }
    return WORLD[((unsigned int)(y & 63) << 6) | (x & 63)];
}

unsigned char passable(unsigned char x, unsigned char y)
{
    unsigned char i;

    if ((unsigned char)(x - BIAS) >= area_w ||
        (unsigned char)(y - BIAS) >= area_h) {
        return 0;
    }
    for (i = 0; i < sm_npc_n; ++i) {
        if (sm_npc_x[i] == x && sm_npc_y[i] == y) {
            return 0;           /* someone is standing there */
        }
    }
    return ow_pass[(tile_at(x, y) - 0x60) & 63];
}

unsigned char __fastcall__ place_step(unsigned char d)
{
    signed char dx = dirdx[d], dy = dirdy[d];

    if (!passable(hx + dx, hy + dy)) {
        return 0;
    }
    return !dx || !dy || passable(hx + dx, hy) || passable(hx, hy + dy);
}

/* A direction held down: the stick, or a key still down in the matrix.
   Q, E, Z and C are the diagonals. */
static unsigned char held(void)
{
    unsigned char k = PEEK(0xCB), shift = PEEK(0x28D) & 1;
    unsigned char j = ~PEEK(0xDC00) & 0x0F;

    if (j == 5  || k == 62) return DIR_UL;
    if (j == 9  || k == 14) return DIR_UR;
    if (j == 6  || k == 12) return DIR_DL;
    if (j == 10 || k == 20) return DIR_DR;
    if ((j & 1) || k == 9)  return DIR_UP;
    if ((j & 2) || k == 13) return DIR_DOWN;
    if ((j & 4) || k == 10) return DIR_LEFT;
    if ((j & 8) || k == 18) return DIR_RIGHT;
    if (k == 7) return shift ? DIR_UP : DIR_DOWN;
    if (k == 2) return shift ? DIR_LEFT : DIR_RIGHT;
    return NONE;
}

void next_frame(void)
{
    unsigned char f = sm_frame;

    while (sm_frame == f) {
    }
}

/* --- The music and the disk -----------------------------------------------

   The game's scenes and its SID files (assets/music.txt, music_map.h): a
   scene plays a song of a file. The bootstrap loaded the file a game starts
   with; one that is another is loaded here, with the screen off and the
   interrupt left with nothing to do but the keyboard. */

static unsigned char song_now = NONE;   /* the song playing, or that would be */
unsigned char music = 1;                /* the M key */
/* 1 + the song paused where it was, to carry on with if the next scene
   plays it (the next program's, if this one is leaving: OWB->song), or 0.
   So the theme plays on through a town's gate, a save and a talk. */
static unsigned char carry_on;

/* Before the disk: the music paused (silence has to reach VICE's sound
   buffer before the drive works, or it plays its last one for as long as
   that takes: CLAUDE.md), and the sprites, which take cycles the serial bus
   needs. Returns the sprites to put back. */
static unsigned char disk_on(void)
{
    unsigned char i;

    if (ow_tune_on) {
        carry_on = song_now + 1;    /* (play_for() clears it) */
    }
    ow_tune_pause();
    song_now = NONE;
    for (i = 0; i < 12; ++i) {
        next_frame();
    }
    i = VIC.spr_ena;
    VIC.spr_ena = 0;
    return i;
}

static void disk_off(unsigned char sprites)
{
    VIC.spr_ena = sprites;
    frame = sm_frame;           /* those frames were not missed */
}

static unsigned char scene_now;         /* the scene play_for() was told */

/* A file from unit 8 to its own address, tried three times: a drive that
   says no and then yes is not worth stopping for. Nonzero if it loaded.
   _oserror is cleared first, so that after a failure it says how: the
   KERNAL's error code if LOAD came back with carry set, or 0 if it came
   back with carry clear but an end address of $0000 (cc65's cbm_load()
   returns the end address, and 0 is failure). */
static unsigned char __fastcall__ load3(const char *name)
{
    unsigned char i;

    _oserror = 0;
    for (i = 0; i < 3; ++i) {
        if (cbm_load(name, 8, 0)) {
            return 1;
        }
    }
    return 0;
}

void play_for(unsigned char s)
{
    unsigned char f = tune_file[s], i;

    scene_now = s;
    if (f != OW->tune) {
        OW->tune = OW_NO_TUNE;
        sm_blank(1);
        i = disk_on();
        ow_tune_stop();         /* (another tune: nothing to carry on with) */
        carry_on = 0;
        if (load3(tune_name[f])) {
            OW->tune = f;
        }
        disk_off(i);
    }
    if (OW->tune == f && tune_song[s] != song_now) {
        song_now = tune_song[s];
        if (music) {
            if (carry_on == song_now + 1) {
                ow_tune_resume(song_now);
            } else {
                ow_tune_play(song_now);
            }
        }
    }
    carry_on = 0;
}

/* F1: the game block (owgame.h) into OW-SAVE, over the one there. The
   drive will not overwrite, so the old one is scratched first; the command
   is spelled in bytes (cc65 would shift a literal's S: CLAUDE.md). The
   border is light blue while the drive works, and red for a second if it
   would not. */
static const char scratch_save[] = {
    0x53, 0x30, 0x3A,                           /* S0:      */
    0x4F, 0x57, 0x2D, 0x53, 0x41, 0x56, 0x45,   /* OW-SAVE  */
    0
};

static void save_game(void)
{
    unsigned char s, i, ok, st[2];

    owg_where();
    s = disk_on();
    VIC.bordercolor = COLOR_LIGHTBLUE;
    ok = 0;
    if (cbm_open(15, 8, 15, scratch_save) == 0) {
        cbm_read(15, st, 2);            /* (no save yet is fine) */
        ok = cbm_save(scratch_save + 3, 8, G, OWG_SIZE) == 0;
        st[0] = 0;
        cbm_read(15, st, 2);
        ok &= st[0] == 0x30 && st[1] == 0x30;
        cbm_close(15);
    }
    VIC.bordercolor = ok ? COLOR_BLACK : COLOR_RED;
    for (i = 0; i < 50 && !ok; ++i) {
        next_frame();
    }
    VIC.bordercolor = COLOR_BLACK;
    disk_off(s);
    play_for(scene_now);
    sm_ink = ok ? COLOR_WHITE : COLOR_RED;
    sm_say(ok ? s_saved : s_unsaved);
}

/* The text screens' program (src/owtown.c): the talk, the shops, the
   inns, the party. */
const char owtown[] = "owtown";

#pragma code-name (push, "HICODE")    /* (MAIN is full) */
/* A fight: owtown (src/lib/ow_fight.c) calls the fight module, and the
   engine comes back to where it was, or to Millford. Never returns. */
void __fastcall__ ow_fight_with(unsigned char kind, unsigned char arg)
{
    OWB->mode = OWB_FIGHT;
    OWB->arg  = kind;
    OWB->npc  = arg;
    ow_leave(owtown);
}

/* After a step onto `tile`: something wanders out, now and then, as the
   game's encounter_roll() has it (out of 200, by the ground; towns never
   roll, and nor does a ship). Not with OWG_CALM, the tests'. */
void __fastcall__ wander(unsigned char tile)
{
    unsigned char chance;

    switch (tile) {
    case T_GRASS:  chance = 8;  break;
    case T_SHORE:  chance = 7;  break;
    case T_FOREST: chance = 12; break;
    case T_SWAMP:  chance = 16; break;
    case T_FLOOR:  chance = 5;  break;
    default:       return;
    }
    if (enc_always) {
        chance = 200;           /* (and calm or not) */
    } else if (G->quiet & OWG_CALM) {
        return;
    }
    if ((unsigned char)(rand() % 200) < chance) {
        ow_fight_with(OWF_WANDER, tile);
    }
}

#pragma code-name (pop)

/* --- What is said, and L ------------------------------------------------
   In the engine's spare memory past the screens (HICODE), strings and all:
   OW-DATA, where constants go, has little room left. */
#pragma code-name (push, "HICODE")
#pragma rodata-name (push, "HICODE")
#pragma data-name (push, "HICODE")



/* A line in the text window, in white. */
static void __fastcall__ say_what(const char *s)
{
    sm_ink = COLOR_WHITE;
    sm_say(s);
}

/* L found something: draughts or gold, said, and a turn spent. */
void __fastcall__ loot_found(unsigned char draughts, unsigned int amount)
{
    ++G->game_turn;
    if (draughts) {
        G->potions += (unsigned char)amount;
        if (amount == 1) {
            say_what(s_draught);
            return;
        }
    } else {
        G->gold += amount;
    }
    strcpy(said, s_find);
    utoa(amount, said + 9, 10);
    strcat(said, draughts ? s_draughts : s_gold);
    sm_ink = COLOR_YELLOW;
    sm_say(said);
}

/* Bit `bit` of looted[]: 1 if it was not yet, and it is now; 0, said, if
   it was. */
unsigned char __fastcall__ take_bit(unsigned char bit)
{
    unsigned char mask = 1 << (bit & 7);

    bit >>= 3;
    if (looted[bit] & mask) {
        say_what(s_taken);
        return 0;
    }
    looted[bit] |= mask;
    return 1;
}

/* L where nothing is: one time in grave_odds the dead rise from the
   ground (none in a town, where `ground` is NONE, nor with OWG_CALM), as
   the game's look() has it. Poke-able: 0 is never (and enc_always, the
   tests', is always). */
unsigned char grave_odds = 5;

void __fastcall__ look_nothing(unsigned char ground)
{
    unsigned char i;

    if (ground != NONE && (enc_always || (grave_odds &&
        !(G->quiet & OWG_CALM) && !(rand() % grave_odds)))) {
        sm_ink = COLOR_RED;
        sm_say(s_dead);
        for (i = 0; i < 60; ++i) {
            next_frame();       /* time to read it */
        }
        ow_fight_with(OWF_UNDEAD, ground);
    }
    say_what(s_nothing);
}

void __fastcall__ refuse(const char *why)
{
    sfx_play(SFX_NO);
    sm_ink = COLOR_CYAN;
    sm_say(why);
}

/* A question, Y or N (RUN/STOP is no), waited for with the music playing. */
static unsigned char __fastcall__ yes(const char *q)
{
    unsigned char k;

    sm_ink = COLOR_YELLOW;
    sm_say(q);
    do {
        k = cbm_k_getin();
    } while (k != 'y' && k != 'n' && k != 0x03);
    return k == 'y';
}

/* RUN/STOP: back to the title, through the bootstrap, which shows it again
   when it finds the world block empty; or, at N, on with the game. */
static void quit(void)
{
    if (!yes(s_quit)) {
        say_what(s_onwith);
        return;
    }
    ow_tune_stop();
    OW->magic = 0;
    OWB->mode = OWB_NONE;
    ow_leave(s_boot);
}

/* The text screens' program for the keys (?) and the party (I). */
static void __fastcall__ screen(unsigned char mode)
{
    OWB->mode = mode;
    ow_leave(owtown);
}

/* M and X, said. */
static void toggles(unsigned char k)
{
    if (k == 'm') {             /* the music off, or on from the top */
        music ^= 1;
        if (!music) {
            ow_tune_stop();
        } else if (song_now != NONE) {
            ow_tune_play(song_now);
        }
        say_what(music ? s_mon : s_moff);
    } else {                    /* the effects off and on */
        sfx_toggle();
        say_what(sfx_on() ? s_xon : s_xoff);
    }
}

#pragma data-name (pop)
#pragma rodata-name (pop)
#pragma code-name (pop)

/* P: a draught for whoever of the party is worst hurt, as the game has it
   (arms.c's most_hurt() and drink_potion()): a gulp if someone drank, a
   refusal if there is none to drink or nobody hurt. */
static void quaff(void)
{
    unsigned char i, who = NONE, lack = 0, amount;
    Actor *a;

    for (i = 0; i < G->party_count; ++i) {
        a = &party[i];
        if (a->hp && a->hp_max - a->hp > lack) {
            lack = a->hp_max - a->hp;
            who = i;
        }
    }
    if (who == NONE || !G->potions) {
        sfx_play(SFX_NO);
        say_what(who == NONE ? s_nobody : s_nodraughts);
        return;
    }
    --G->potions;
    a = &party[who];
    amount = (unsigned char)(((unsigned int)a->hp_max * POTION_SHARE) / 100);
    if (amount < POTION_FLOOR) {
        amount = POTION_FLOOR;
    }
    a->hp = amount < lack ? a->hp + amount : a->hp_max;
    sfx_play(SFX_DRINK);
    strcpy(said, a->name);
    strcat(said, s_drinks);
    say_what(said);
}

/* Back through the bootstrap, which builds a world: the saved game's
   (OWB_LOAD, F3), or the next seed's (N), with the party in it. The game
   block first, which is what goes on if there is no save. */
static void rebuild(unsigned char mode)
{
    owg_where();
    ow_tune_stop();
    OWB->song = 0;
    sm_quit();
    OWB->mode = mode;
    OW->seed = seed + 1;
    ow_run("openworld");
}

/* A file that would not load: its name and what the drive says, on the
   KERNAL's text screen (row 12), until a key; then it is tried again.
   `up` is whether the engine's screen is going, to be put back after.
   (On VICE every load works; this is for a drive that says no.) */
static void __fastcall__ load_failed(const char *name, unsigned char up)
{
    unsigned char *row = (unsigned char *)0x0400 + 40 * 12;

    sm_quit();
    memset(row, ' ', 40);
    strcpy((char *)row, name);  /* (PETSCII as screen codes: readable) */
    row += strlen(name);
    row[0] = ' ';
    row[1] = 0x45;              /* E, and the KERNAL's code (load3()) */
    row[2] = '0' + _oserror;
    row += 4;
    if (cbm_open(15, 8, 15, "") == 0) {
        cbm_read(15, row, 24);
        cbm_close(15);
    }
    memset((void *)(0xD800 + 40 * 12), COLOR_WHITE, 40);
    VIC.bordercolor = COLOR_RED;
    while (!cbm_k_getin()) {
    }
    VIC.bordercolor = COLOR_BLACK;
    if (up) {
        sm_show();
    }
}

/* The overlay ov_want asks for, into the slot (the screen is off), and
   started. The game's disk is needed: load_failed() says what the drive
   said, and it is tried again after a key.
   An overlay's ov_enter() may ask for the other in turn. */
static void ov_switch(void)
{
    unsigned char s;
    const char *name;

    while (ov_want != ov_have) {
        s = disk_on();
        name = ov_want == OV_TOWN ? town_file :
               ov_want == OV_DUNGEON ? dungeon_file : world_file;
        while (!load3(name)) {
            load_failed(name, 1);
        }
        VIC.bordercolor = COLOR_BLACK;
        disk_off(s);
        ov_have = ov_want;
        ov_enter();
    }
}

/* --- Going in and out of places --------------------------------------- */

/* The screen off. (The world's overlay puts the overworld away too.) */
void leave_area(void)
{
    switch_time = PEEK(0xA2);
    sm_blank(1);
    next_frame();               /* the blank has reached the screen */
}

/* A place's map, from local[], into WORLD, with the people in it, in the
   tile set `set` (which says what is beyond the map's edge). */
void build_area(const unsigned char *set, unsigned char w, unsigned char h)
{
    unsigned char y;

    sm_npc_n = 0;               /* nobody, until the people are shown */
    sm_torus = 0;               /* a place has edges */
    sm_tileset(set);
    memset(WORLD, sm_offtile, 64 * 64);
    for (y = 0; y < h; ++y) {
        memcpy(WORLD + (unsigned int)y * 64, local + LOCAL_AT(0, y), w);
    }
    area_w = w;
    area_h = h;
}

/* The camera on the hero at engine square (x, y) of the new area, drawn,
   and on. */
void __fastcall__ show_area(unsigned char x, unsigned char y)
{
#ifdef OW_REU
    if (area != AREA_HOME)      /* (the REU kept the overworld's) */
#endif
    {
        shore_time = PEEK(0xA2);
        sm_shores();
        shore_time = PEEK(0xA2) - shore_time;
    }
    hx = x;
    hy = y;
    pre_dir = NONE;
    place_camera();
    sm_draw_all();
    sm_npcs();
    sm_blank(0);
    switch_time = PEEK(0xA2) - switch_time;
    frame = sm_frame;           /* that took frames, which were not missed */
}

/* Something typed: a step, or a setting. PETSCII. */
#pragma code-name (push, "OWTOP")  /* past the sprite blocks */
static unsigned char key_dir(unsigned char k)
{
    switch (k) {
    case 'w': case 0x91: return DIR_UP;
    case 's': case 0x11: return DIR_DOWN;
    case 'a': case 0x9D: return DIR_LEFT;
    case 'd': case 0x1D: return DIR_RIGHT;
    case 'q': return DIR_UL;
    case 'e': return DIR_UR;
    case 'z': return DIR_DL;
    case 'x': return DIR_DR;
    }
    return NONE;
}

static unsigned char typed(void)
{
    unsigned char k = cbm_k_getin(), d = key_dir(k);

    if (d != NONE) {
        return d;
    }
    switch (k) {
    case '1': speed = 1; break;
    case '2': speed = 2; break;
    case 'm':                   /* the music */
    case 'M':                   /* SHIFT+M: the effects */
        toggles(k);
        break;
    case 'p':                   /* a draught */
        quaff();
        break;
    case 'l':                   /* look (the overlay's) */
        ov_look();
        break;
    case 'i':                   /* the party's screen (owtown) */
        screen(OWB_PARTY);      /* (neither returns) */
    case '?':                   /* the keys (owtown) */
        screen(OWB_HELP);
    case 0x03:                  /* RUN/STOP: quit */
        quit();
        break;
    case 0x85:                  /* F1: save */
        save_game();
        break;
    case 0x86:                  /* F3: the saved game */
        rebuild(OWB_LOAD);
    case 'n':                   /* the next world */
        rebuild(OWB_NONE);
    }
    return NONE;
}

#pragma code-name (pop)

static void publish(void)
{
    PROBE_SCRATCH[0] = hx - BIAS;
    PROBE_SCRATCH[1] = hy - BIAS;
    PROBE_SCRATCH[2] = late;
    PROBE_SCRATCH[3] = sm_colend;
    PROBE_SCRATCH[4] = sm_ox;
    PROBE_SCRATCH[5] = sm_oy;
    PROBE_SCRATCH[6] = area;
    PROBE_SCRATCH[7] = depth;
}

/* The frame's camera, `moved` pixels into the step, and the screen
   flipped if that takes it into another character. Returns whether it
   flipped.

   A diagonal one way across and the other way down would wrap its two fine
   scrolls on different frames (the camera rests four pixels into a
   character, which is three from wrapping one way and four the other), and
   two flips two frames apart leave no frame to build in between. So the
   axis going the positive way runs a pixel behind until the last frame:
   then both wrap together and every flip is one, as on a straight step. */
static unsigned char glide(void)
{
    signed char dx = dirdx[dir], dy = dirdy[dir];
    unsigned char px = moved, py = moved, ox, oy, flip;
    unsigned int c;

    if (dx && dy && dx != dy && moved != 16) {
        if (dx > 0) {
            px = moved - 1;
        } else {
            py = moved - 1;
        }
    }
    c = dx > 0 ? camx0 + px : dx < 0 ? camx0 - px : camx0;
    ox = (unsigned char)(c >> 3);
    sm_fx = (unsigned char)c & 7;
    c = dy > 0 ? camy0 + py : dy < 0 ? camy0 - py : camy0;
    oy = (unsigned char)(c >> 3);
    sm_fy = (unsigned char)c & 7;

    flip = ox != sm_ox || oy != sm_oy;
    if (flip) {
        /* Never wanted at one or two pixels a frame, but a flip to a screen
           half built would show it: finish it first, late or not. It builds
           from the old origin, so before that moves. */
        build_some();
        build_some();
        build_some();
    }
    sm_ox = ox;
    sm_oy = oy;
    sm_commit(flip);
    return flip;
}

/* The game block (owgame.h), which the bootstrap, the party's making or
   another program left: the game, and where it is. The overlays take the
   rest of the bootstrap's hand-off (ow.h). */
static void take_block(void)
{
    seed = lore_seed = OW->seed;
    quest_stage = G->quest_stage;
    items       = G->items;
    memcpy(looted, G->looted, sizeof looted);
    home_biome = OW->home_biome;
    memcpy(home_fx, OW->home_fx, NUM_HOME_PLACES);
    memcpy(home_fy, OW->home_fy, NUM_HOME_PLACES);
    if (G->ships) {
        memcpy(ship_x, G->ship_x, sizeof ship_x);
        memcpy(ship_y, G->ship_y, sizeof ship_y);
        boat = G->boat;
    } else {
        memcpy(ship_x, OW->ship_x, sizeof ship_x);
        memcpy(ship_y, OW->ship_y, sizeof ship_y);
    }
    land = G->land;
    if (land != ISLE_HOME) {
        isle_biome = OW->ibiome[land];
    }
    out_wx = G->wx;
    out_wy = G->wy;
    depth  = G->depth;
    if (G->x != OWG_GATE) {
        resuming = 1;
        resume_x = G->x;
        resume_y = G->y;
    }
    switch (G->area) {
    case AREA_HOME:
        ov_want = OV_WORLD;
        break;
    case AREA_DUNGEON:
        dun_index = G->place;
        ov_want = OV_DUNGEON;
        break;
    default:
        town_index = G->place;
        ov_want = OV_TOWN;
        break;
    }
    if (G->quiet & 1) {
        music = 0;
    }
    carry_on = OWB->song;       /* the music another program paused */
    OWB->song = 0;
    if (G->quiet & 2) {
        sfx_toggle();
    }
    /* The dice for what wanders out, from where the game has got to: the
       same walk meets the same things, as a test wants, and a game moves
       on from it with every turn. */
    srand(G->game_turn ^ ((unsigned int)seed << 8));
    OWB->mode = OWB_NONE;
}

void owg_where(void)
{
    static unsigned int counted;

    G->game_turn += steps - counted;    /* a step is a turn, as in the game */
    counted = steps;
    G->quest_stage = quest_stage;
    G->items       = items;
    memcpy(G->looted, looted, sizeof looted);
    G->area  = area;
    G->land  = land;
    G->depth = depth;
    G->place = area == AREA_DUNGEON ? dun_index : town_index;
    G->boat  = boat;
    G->ships = 1;
    memcpy(G->ship_x, ship_x, sizeof ship_x);
    memcpy(G->ship_y, ship_y, sizeof ship_y);
    if (area == AREA_HOME) {
        G->wx = wx;
        G->wy = wy;
        G->x  = OWG_GATE;
    } else {
        G->wx = out_wx;
        G->wy = out_wy;
        G->x  = hx - BIAS;
        G->y  = hy - BIAS;
    }
    G->quiet = (G->quiet & OWG_CALM) | !music | (sfx_on() ? 0 : 2);
}

void __fastcall__ ow_leave(const char *name)
{
    owg_where();
    disk_on();                  /* the music paused where it is, and ... */
    OWB->song = carry_on;       /* ... the next program may carry on */
    sm_quit();
    memset((void *)0x0400, 0x20, 1000);     /* the load shows a clear screen, */
    memset((void *)0xD800, 0, 1000);        /* black, colour RAM too */
    VIC.bgcolor0 = COLOR_BLACK;             /* and the background, which is
                                               green on the map */
    VIC.bordercolor = COLOR_BLACK;
    ow_run(name);
}

int main(void)
{
    unsigned char d, flipped;

    /* The C stack, which cc65 starts at the top of MAIN, into the tape
       buffer instead ($0334-$03FF, down from $0400), which nothing else
       here uses. (main() keeps nothing on it, and never returns.) */
    sm_stack();                 /* (the screen is off: the loader's doing) */

    if (OW->magic != OW_MAGIC) {
        cputs("\r\nLoad OPENWORLD: it builds the world.");
        for (;;) {
        }
    }
    /* The engine's code past the screens, $52C0-$59FF (OWTOP, HICODE): a
       file of its own, so that this one is not 5K of empty screens longer.
       Nothing in it may run before this. */
    while (!load3(hi_file)) {
        load_failed(hi_file, 0);
    }
    VIC.bordercolor = COLOR_BLACK;
    take_block();
    /* (Nothing is in OWLOW now, the code run at $0400 that was copied out of
       screen A's space here: the roofs' flood and drawing were, and are in
       the town's overlay. Put some there and the copy has to come back.) */

    /* The interrupt going, with the screen off, before the first overlay:
       the disk waits for frames. The overlay draws its area and turns the
       screen on. */
    sm_init();
    sm_sprite(0);
    VIC.bgcolor0 = COLOR_BLACK;
    sm_blank(1);
    sm_show();
    ov_switch();

    while (kbhit()) {
        cgetc();
    }
    publish();
    probe_stage(1);

    frame = sm_frame;
    for (;;) {
        /* The next frame after the last one handled: a step's build can
           run past an interrupt, and then this does not wait at all. */
        while (sm_frame == frame) {
            if (gen_state & 1) {
                gen_run(frame); /* a continent's build has the rest of it */
            } else if (moving) {
                ++idle;         /* how much of the frame is left over */
            }
        }
        if (moving) {
            ++walked;
        }
        if ((unsigned char)(sm_frame - frame) > 1) {
            ++late;
        }
        frame = sm_frame;
        PROBE_COUNTER = frame;
        if (after) {
            ov_after();
            after = 0;
        }

        if (moving) {
            if (freeze_at && moved == freeze_at && steps >= freeze_after) {
                publish();
                probe_stage(3);
                for (;;) {
                }
            }
            moved += speed;
            flipped = glide();
            if (flipped) {
                pre_dir = NONE;     /* the back screen is the old front */
            }
            if (sm_npc_n) {
                sm_npcs();      /* the people move with the map */
            }
            if (moved == 8) {
                sm_sprite(hero_frame(dir, stride ^= 1));
            }
            if (flipped) {
                /* The old front is on show until the interrupt: build from
                   next frame, if the step has another character to go. */
                if (moved < 9) {
                    phase = SHIFT;
                }
            } else if (phase != BUILT) {
                build_some();
            } else if (moved > 12 && pre_dir == NONE) {
                /* Nothing left to build this step, and no flip before it
                   ends: shift the back screen for the next one now, if it
                   can be seen coming, so that the frame that ends this
                   step and starts that one need not (on NTSC it has no
                   room for it). If it goes another way after all, or none,
                   this is wasted, not wrong. */
                d = held();
                if (d == NONE && PEEK(0xC6)) {
                    d = key_dir(PEEK(0x277));
                }
                if (d != NONE) {
                    sm_shift(d);
                    pre_dir = d;
                }
            }
            if (moved == 16) {
                moving = 0;
                if (area != AREA_HOME) {
                    hx += dirdx[dir];
                    hy += dirdy[dir];
                }
                ++steps;
                ov_arrived();   /* (the overworld's step is the world's) */
                ov_switch();    /* into a town or a dungeon, or out */
                publish();
            }
        } else if ((frame & 15) == 0 && !still) {
            sm_animate();
        }

        if (!moving && !flipped) {
            if (PEEK(0xC6) != 0) {
                probe_stage(1); /* not still: something is to be read */
            }
            d = held();
            if (d != NONE) {
                POKE(0xC6, 0);  /* a tap is a step, not two */
            } else {
                d = typed();
            }
            if (d != NONE) {
                sm_sprite(hero_frame(d, stride));
                if (ov_can_step(d)) {
                    dir = d;
                    moving = 1;
                    moved = 0;
                    camx0 = (unsigned int)sm_ox * 8 + sm_fx;
                    camy0 = (unsigned int)sm_oy * 8 + sm_fy;
                    if (dir == pre_dir) {
                        phase = ROW;    /* shifted already */
                    } else {
                        phase = SHIFT;
                        build_some();   /* the edges in the next frames */
                    }
                    sm_sprite(hero_frame(dir, stride ^= 1));
                    if (!boat) {    /* a footstep, by what it lands on */
                        sfx_step(tile_at(hx + dirdx[d], hy + dirdy[d]));
                    }
                    bumped = 0;
                } else if (bumped != d + 1) {
                    /* Into a person (a talk, in a town), or a way not open
                       yet: once a press, not every frame it is held. */
                    bumped = d + 1;
                    ov_bump(d);
                }
            } else {
                bumped = 0;
                if (PEEK(0xC6) == 0 && steps != 0) {
                    probe_stage(2);
                }
            }
        }
        /* A frame with nothing built in it: the window's next job. */
        if (area == AREA_HOME) {
            ov_idle();
        }
        flipped = 0;
    }
    return 0;
}
