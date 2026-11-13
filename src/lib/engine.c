/*
 * q4k -- loading engines into the slot: see engine.h.
 *
 * Each engine is a file on the disk, linked to the slot's address, with the
 * two-byte load address in front that every PRG has, so the KERNAL's LOAD
 * puts it where it belongs. Each scene has a file of music too, which goes
 * into the score area the player reads (music_data.s), in the same pause:
 * the silence a load needs is where one tune ends and the next begins. For
 * now every scene's file is the same tune (tools/mkengines.py copies it).
 */

#include <cbm.h>
#include <conio.h>

#include <q4k/engine.h>
#include <q4k/music.h>
#include <q4k/savegame.h>
#include <q4k/sprite.h>
#include <q4k/view.h>

/* Top of BASIC memory: see savegame.c, which raises it for the same reason. */
#define MEMSIZ (*(unsigned int *)0x0283)

#define NONE   0xFF

unsigned char eng_held;          /* 0, ENG_TITLE, until something loads */
unsigned char eng_a, eng_b;
char          eng_text[41];
unsigned char fight_spoils;

/* The music each engine brings, if any (0: none, keep what is playing), and
   which of those the score area holds (0: the one linked into the PRG). */
static unsigned char score_held;

static const char *const eng_file[ENG_COUNT] = {
    0, "e.map", "e.fight", "e.talk", "e.shop",
};
static const unsigned char eng_score[ENG_COUNT] = { 0, 1, 2, 3, 0 };
static const char *const score_file[4] = { 0, "m.map", "m.fight", "m.talk" };

/* The load addresses at the head of each engine's file: the slot's. */
extern unsigned char _SLOT_START__[];
#pragma rodata-name (push, "MAPADDR")
const void *const map_load_at = _SLOT_START__;
#pragma rodata-name (pop)
#pragma rodata-name (push, "FIGHTADDR")
const void *const fight_load_at = _SLOT_START__;
#pragma rodata-name (pop)
#pragma rodata-name (push, "TALKADDR")
const void *const talk_load_at = _SLOT_START__;
#pragma rodata-name (pop)
#pragma rodata-name (push, "SHOPADDR")
const void *const shop_load_at = _SLOT_START__;
#pragma rodata-name (pop)

/* LOAD `name` at its own address, asking for the disk until it works. */
static void fetch(const char *name)
{
    while (cbm_load(name, save_device, 0) == 0) {
        view_message("Put the game disk back, then a key.");
        cgetc();
    }
}

void engine_load(unsigned char e)
{
    unsigned char s;

    if (eng_held == e) {
        return;
    }
    /* Every enabled sprite takes cycles from the serial bus (CLAUDE.md), and
       whatever screen is up is about to be replaced anyway. */
    sprite_hide();
    MEMSIZ = 0xCF00;
    music_pause();
    eng_held = NONE;                /* half a file is no engine at all */
    fetch(eng_file[e]);
    eng_held = e;
    s = eng_score[e];
    if (s && s != score_held) {
        fetch(score_file[s]);
        score_held = s;
        music_rewind();
    }
    music_resume();
}

unsigned char engine_call(unsigned char e, unsigned char (*entry)(void))
{
    static unsigned char was, r;

    was = eng_held;
    engine_load(e);
    r = entry();
    engine_load(was);
    return r;
}
