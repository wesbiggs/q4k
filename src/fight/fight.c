/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Quest for the King -- the multicolour fight (include/q4k/fight.h).
 *
 * The rules are src/lib/battle.c's, kept as they are there: each of the
 * party takes a turn and then the monsters do, a monster goes for whoever it
 * can reach soonest by a flood from every member at once, walking into
 * something is how it is struck, and the middle of each side is the way
 * out. What is new is where it happens. The field is 20 squares by 9 of
 * multicolour metatiles, drawn beforehand by tools/mkfields.py and loaded
 * as a picture; the party, the monsters and whatever flies between them
 * are sprites (mux.s shows more of them than the VIC has), so they glide
 * from square to square and the field under them is never redrawn; and the
 * text is six rows under it:
 *
 *   row 18      solid black, where the background changes colour (mux.s)
 *   row 19      what is being fought, and the health of each of them
 *   rows 20-21  the party, a column of ten each: name, health, magic; and
 *               the draughts and arrows in the last column
 *   rows 22-24  what is happening
 *
 * Text is in the same multicolour mode as the field, where a character
 * whose colour is 0-7 is an ordinary one: so its colours are those eight.
 *
 * Unlike battle.c it goes eight ways, as the open world walks: the party
 * steps and strikes diagonally too (Q E Z X, or the stick's diagonals),
 * and the monsters' flood, their blows and their shots go eight ways, as
 * the party's arrows and spells do (the arrow's picture is copied into its
 * one sprite block for the way it flies: arrow_to()).
 * The up-arrow key shoots, * casts, M is the music and SHIFT+M the effects,
 * as in the open world.
 *
 * Left to whoever called: the experience and the pay (fq_foes says for how
 * many), the levels they bring, and what a monster drops.
 */

#include <cbm.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/fight.h>
#include <q4k/game.h>
#include <q4k/music.h>          /* sfx_*, SFX_* */
#include <q4k/sprite.h>         /* DIR_*, SHOT_* */
#include <q4k/tiles.h>          /* T_*, for the steps' sounds */

/* The rest of the block (block.c). */
extern unsigned char fq_tunic[MAX_PARTY];
extern char          fq_name[FIGHT_NAME];
extern unsigned char fq_hp, fq_atk, fq_def, fq_colour, fq_speed, fq_size;
extern unsigned char fq_shot, fq_reach, fq_kind;
extern unsigned char fq_count, fq_flee;
extern unsigned int  fq_seed;
extern char          fq_field[FIGHT_FNAME], fq_sprite[FIGHT_FNAME];
extern unsigned char fq_foes;
extern unsigned char fq_song, fq_carry, fq_quiet;
extern unsigned int  fq_rounds;
extern unsigned int  fq_xp_each, fq_to_level;

#ifdef FIGHT_BOT
#define FIGHT_BOT_ON 1
#else
#define FIGHT_BOT_ON 0
#endif

/* The music (src/lib/ow_tune.s, called by mux.s's interrupt once a frame,
   with the effects): the tune under the KERNAL that the caller says. */
void __fastcall__ ow_tune_play(unsigned char song);
void __fastcall__ ow_tune_resume(unsigned char song);
void ow_tune_pause(void);
void ow_tune_stop(void);
extern unsigned char ow_tune_on;

/* Keys, as PETSCII numbers. */
#define K_W          0x57
#define K_A          0x41
#define K_S          0x53
#define K_D          0x44
#define K_POTION     0x50
#define K_FIRE       0x5E       /* the up-arrow key: an arrow */
#define K_Q          0x51
#define K_E          0x45
#define K_Z          0x5A
#define K_X          0x58       /* down and right, as in the open world */
#define K_CAST       0x2A       /* *: a burst of magic */
#define K_M          0x4D       /* the music */
#define K_SHIFT_M    0xCD       /* the effects */
#define K_SPACE      0x20
#define K_HELP       0x3F       /* ? */
#define K_CRSR_DOWN  0x11
#define K_CRSR_RIGHT 0x1D
#define K_CRSR_UP    0x91
#define K_CRSR_LEFT  0x9D

#define NONE 0xFF

/* Eight ways: sprite.h's four, then the diagonals, as the open world's
   engine numbers them (src/ow/engine.h). */
#define DIR_DL 4
#define DIR_DR 5
#define DIR_UL 6
#define DIR_UR 7

/* For the tests (tools/screendump.py --stage): the probe page's stage is 3
   while the fight waits for a key with none typed ahead, and 2 while it is
   busy. The page is the caller's; every program here keeps it at $CF00. */
#define PROBE_STAGE (*(unsigned char *)0xCF02)
#define KEYS_AHEAD  (*(unsigned char *)0x00C6)

/* battle.c's tuning (see there), but for an archer's eye, which is two
   more: with shots stopping at the party, the archers lose turns waiting
   for a clear line (CLAUDE.md has what tools/balance.py --ow measured).
   (Overridable with -D for a sweep.) */
#ifndef ARCHER_EYE
#define ARCHER_EYE  (5 + (party_level >> 1))
#endif
#define LONE_HP     3
#define LONE_ATK    2
#define WARD        4
#ifndef WIZ_GUARD
#define WIZ_GUARD   ((party_level << 1) / 3)
#endif

/* Text colours: 0-7 only. */
#define C_WHITE  1
#define C_RED    2
#define C_CYAN   3
#define C_GREEN  5
#define C_YELLOW 7

/* The sprites asked of mux.s: the party, the monsters (or the four
   quarters of a big one), whatever is flying, and the cursor. */
#define V_FOE    4
#define V_SHOT   10
#define V_CUR    11
#define V_SKIN   12             /* 12-15: each member's skin, over them */
extern const unsigned char fight_skins[];   /* fight_data.s: SPR_SKIN's */

#define ROW_FOE   19
#define ROW_NAMES 20
#define ROW_STATS 21
#define ROW_LOG   22

/* --- text ----------------------------------------------------------------- */

static unsigned char *tp;       /* where the next character goes */
static unsigned char tcol;
static char msg[41];            /* PETSCII */

static void t_at(unsigned char x, unsigned char y, unsigned char colour)
{
    tp = F_SCREEN + (unsigned int)y * 40 + x;
    tcol = colour;
}

/* A PETSCII character as the ROM's screen code, by number (cc65 turns the
   literals into PETSCII), and from there to where the letters are: $80 on. */
static void t_ch(unsigned char c)
{
    if (c >= 0xC0) {
        c -= 0x80;
    } else if (c >= 0x60) {
        c -= 0x20;
    } else if (c >= 0x40) {
        c -= 0x40;
    }
    *tp = c | 0x80;
    tp[0xD800 - 0x4000] = tcol;
    ++tp;
}

static void t_str(const char *s)
{
    while (*s) {
        t_ch(*s++);
    }
}

static void t_num(unsigned int n)
{
    char buf[6];

    utoa(n, buf, 10);
    t_str(buf);
}

static void t_clear(unsigned char x, unsigned char y, unsigned char n)
{
    memset(F_SCREEN + (unsigned int)y * 40 + x, 0xA0, n);
}

/* The log: three lines, the newest at the bottom. A row at a time: one
   memcpy() of both rows up over themselves came out with the newest line
   in both (memcpy() is not memmove(): it may copy in any order). */
static void say(const char *s)
{
    memcpy(F_SCREEN + ROW_LOG * 40, F_SCREEN + (ROW_LOG + 1) * 40, 40);
    memcpy(F_SCREEN + (ROW_LOG + 1) * 40, F_SCREEN + (ROW_LOG + 2) * 40, 40);
    t_clear(0, ROW_LOG + 2, 40);
    t_at(0, ROW_LOG + 2, C_WHITE);
    t_str(s);
}

static void cat_num(unsigned int value)
{
    utoa(value, msg + strlen(msg), 10);
}

/* --- the field ------------------------------------------------------------ */

#define RIM_W      (FW + 2)
#define RIM_H      (FH + 2)

static unsigned char wall[RIM_W * RIM_H];    /* 1 where nothing can stand */
static unsigned char roomy[RIM_W * RIM_H];   /* 0 where a 2x2 body fits */
static unsigned char seen[RIM_W * RIM_H];
static unsigned char queue_x[256], queue_y[256];
static unsigned char *seen_row[RIM_H];
static unsigned char hx, hy, mx, my;
static unsigned char size;

static unsigned char mem_x[MAX_PARTY], mem_y[MAX_PARTY], guard[MAX_PARTY];
/* What each of the party owes for slow ground, in tenths of a turn: a step
   onto a square costs FF_COST of it, of which a turn pays 10, and a turn
   is lost for every 10 owed. */
static unsigned char mire[MAX_PARTY];
/* ... and what the monster's last step cost past 10 (foe_step()). */
static unsigned char slow;
static unsigned char act, ward, sleep;
/* Who of the party has got away by a side: off the field, their turns
   skipped, until the rest follow them out (or fall). */
static unsigned char gone[MAX_PARTY], fled_n;

/* Member `i` is on the field: standing, and not got away. */
static unsigned char here(unsigned char i)
{
    return party[i].hp && !gone[i];
}
static unsigned char rel, lone, lone_bank, fury;

static unsigned char foes, alive, cur;
static unsigned char foe_x[MAX_FOES], foe_y[MAX_FOES];
static unsigned char foe_pace[MAX_FOES];
static unsigned int  foe_hp[MAX_FOES];
/* Each one's pronouns (PRON_*, actor.h), from the monster's (MP_* in
   fq_kind's top bits: a person's rolled for each), and the last to fall. */
static unsigned char foe_pron[MAX_FOES], fell;

static const char *const p_subj[4] = { "They", "He", "She", "It" };
static const char *const p_obj[4]  = { "them", "him", "her", "it" };

/* Whoever is still standing, as one: "them" for more than one. */
static unsigned char foes_pron(void)
{
    unsigned char i;

    if (alive == 1) {
        for (i = 0; !foe_hp[i]; ++i) {
        }
        return foe_pron[i];
    }
    return PRON_THEY;
}

/* Three pieces into msg, and said as one line. */
static void cat3(const char *a, const char *b, const char *c)
{
    strcpy(msg, a);
    strcat(msg, b);
    strcat(msg, c);
}

/* "She falls.", "They fall.": the verb agrees. */
static void say_did(unsigned char p, const char *verb)
{
    cat3(p_subj[p], verb, p ? "s." : ".");
    say(msg);
}

#define flag_at(x, y) (F_INFO[FI_FLAGS + (unsigned char)((y) * FW + (x))])
#define wall_at(x, y) (wall[(unsigned char)(((y) + 1) * RIM_W + (x) + 1)])

/* --- sprites -------------------------------------------------------------- */

static unsigned char tick, anim;
static unsigned char aiming;    /* the cursor is on a foe to shoot: yellow, */
static unsigned char cx, cy;    /* not blinking, there */
static unsigned char ground;    /* the game's tile for the field's ground */

/* Sprite `v` on square (x,y). */
static void v_put(unsigned char v, unsigned char x, unsigned char y)
{
    unsigned int px = 24 + ((unsigned int)x << 4);

    vs_x[v]  = (unsigned char)px;
    vs_xh[v] = (unsigned char)(px >> 8);
    vs_y[v]  = 50 + (y << 4);
}

static const signed char dir_dx[8] = { 0, 0, -1, 1, -1, 1, -1, 1 };  /* DIR_* */
static const signed char dir_dy[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };

/* The way of a step of (dx, dy), each -1, 0 or 1, not both 0. */
static unsigned char dir_of(signed char dx, signed char dy)
{
    if (!dx) {
        return dy > 0 ? DIR_DOWN : DIR_UP;
    }
    if (!dy) {
        return dx > 0 ? DIR_RIGHT : DIR_LEFT;
    }
    return (dy > 0 ? DIR_DL : DIR_UL) + (dx > 0);
}

/* `n` sprites from `v`, `step` pixels the way of `dir`. */
static void v_nudge(unsigned char v, unsigned char n, unsigned char dir,
                    signed char step)
{
    signed char dx = dir_dx[dir] * step, dy = dir_dy[dir] * step;
    unsigned int px;

    for (; n; --n, ++v) {
        px = ((unsigned int)vs_xh[v] << 8 | vs_x[v]) + dx;
        vs_x[v]  = (unsigned char)px;
        vs_xh[v] = (unsigned char)(px >> 8) & 1;
        vs_y[v] += dy;
    }
}

/* Which block each of them shows: everyone idles between two frames. */
static void poses(void)
{
    unsigned char i;

    for (i = 0; i < party_count; ++i) {
        vs_ptr[i] = SPR_PARTY + (party[i].cls << 1) + anim;
    }
    if (size == 1) {
        for (i = 0; i < MAX_FOES; ++i) {
            vs_ptr[V_FOE + i] = SPR_MONSTER + anim;
        }
    } else {
        for (i = 0; i < 4; ++i) {
            vs_ptr[V_FOE + i] = SPR_MONSTER + (anim << 2) + i;
        }
    }
    vs_col[V_CUR] = anim | aiming ? C_YELLOW : C_WHITE;
}

/* One frame: what moved is shown, and every sixteenth they shift their
   feet. */
static void frame(void)
{
    unsigned char i;

    if (!(++tick & 15)) {
        anim ^= 1;
        poses();
    }
    /* Each member's skin is a sprite of its own (one frame: the bob is it
       sunk a row, tools/figures.py), standing where they do. */
    for (i = 0; i < party_count; ++i) {
        vs_x[V_SKIN + i]  = vs_x[i];
        vs_xh[V_SKIN + i] = vs_xh[i];
        vs_y[V_SKIN + i]  = vs_y[i] ? vs_y[i] + anim : 0;
    }
    mx_plan();
#ifndef FIGHT_BOT
    mx_wait();                  /* (the autoplayer does not wait to see) */
#endif
}

static void pause(unsigned char frames)
{
    for (; frames; --frames) {
        frame();
    }
}

/* Foe `i`'s sprites: one, or all four of a big one's. */
static unsigned char fv, fn;

static void foe_sprites(unsigned char i)
{
    if (size == 1) {
        fv = V_FOE + i;
        fn = 1;
    } else {
        fv = V_FOE;
        fn = 4;
    }
}

static void foe_place(unsigned char i)
{
    unsigned char q;

    foe_sprites(i);
    for (q = 0; q < fn; ++q) {
        v_put(fv + q, foe_x[i] + (q & 1), foe_y[i] + (q >> 1));
        vs_col[fv + q] = fq_colour;
    }
}

/* `n` sprites from `v` flash white, as something lands on them, with the
   blow's star at square (x,y). */
static void flash(unsigned char v, unsigned char n, unsigned char x,
                  unsigned char y)
{
    unsigned char was = vs_col[v], i;

    v_put(V_SHOT, x, y);
    vs_ptr[V_SHOT] = SPR_HIT;
    vs_col[V_SHOT] = C_YELLOW;
    for (i = 0; i < n; ++i) {
        vs_col[v + i] = C_WHITE;
    }
    pause(5);
    for (i = 0; i < n; ++i) {
        vs_col[v + i] = was;
    }
    vs_y[V_SHOT] = 0;
    pause(3);
}

/* ... and blink out, fallen. */
static void vanish(unsigned char v, unsigned char n)
{
    unsigned char i, k, y[4];

    for (i = 0; i < n; ++i) {
        y[i] = vs_y[v + i];
    }
    for (k = 0; k < 6; ++k) {
        for (i = 0; i < n; ++i) {
            vs_y[v + i] = (k & 1) ? y[i] : 0;
        }
        pause(3);
    }
    for (i = 0; i < n; ++i) {
        vs_y[v + i] = 0;
    }
}

/* Sprite(s) lean `dir` and come back: a blow struck. */
static void lunge(unsigned char v, unsigned char n, unsigned char dir)
{
    v_nudge(v, n, dir, 3);
    frame();
    v_nudge(v, n, dir, 3);
    pause(3);
    v_nudge(v, n, dir, -6);
    frame();
}

static const unsigned char shot_colour[4] = { 7, 8, 14, 1 };  /* SHOT_* */

/* The arrow's eight pictures, by DIR_* (fight_data.s): two bytes a row,
   copied into its one block for the way it is about to fly. */
extern const unsigned char fight_arrow[8][32];
#define ARROW_BLOCK ((unsigned char *)0x4000 + SPR_ARROW * 64)

static void arrow_to(unsigned char dir)
{
    const unsigned char *s = fight_arrow[dir];
    unsigned char *d = ARROW_BLOCK;
    unsigned char i;

    for (i = 0; i < 16; ++i, s += 2, d += 3) {
        d[0] = s[0];
        d[1] = s[1];
    }
}

/* Something flies from square (x,y), `flew` squares the way of `dir`. */
static void shoot(unsigned char x, unsigned char y, unsigned char dir,
                  unsigned char flew, unsigned char kind)
{
    v_put(V_SHOT, x, y);
    if (kind == SHOT_ARROW) {
        arrow_to(dir);
        vs_ptr[V_SHOT] = SPR_ARROW;
    } else {
        vs_ptr[V_SHOT] = SPR_FIRE - 1 + kind;
    }
    vs_col[V_SHOT] = shot_colour[kind];
    for (flew <<= 1; flew; --flew) {
        v_nudge(V_SHOT, 1, dir, 8);
        frame();
    }
    vs_y[V_SHOT] = 0;
}

/* --- the panel ------------------------------------------------------------ */

/* A sprite's colour as one a letter can have. */
static const unsigned char as_text[16] = {
    C_WHITE, 1, 2, 3, 4, 5, 6, 7, C_YELLOW, C_YELLOW, C_RED, C_WHITE,
    C_WHITE, C_GREEN, C_CYAN, C_WHITE,
};

static void show_foe(void)
{
    unsigned char i, x = 22;

    t_clear(0, ROW_FOE, 40);
    t_at(0, ROW_FOE, as_text[fq_colour & 15]);
    t_str(fq_name);
    if (alive > 1) {
        t_str(" x");
        t_num(alive);
    }
    t_at(19, ROW_FOE, C_CYAN);
    t_str("HP");
    for (i = 0; i < foes; ++i) {
        if (foe_hp[i] || foes == 1) {
            t_at(x, ROW_FOE, C_WHITE);
            t_num(foe_hp[i]);
            x += 3;
        }
    }
}

static void show_party(void)
{
    unsigned char i, x;
#ifndef FIGHT_BOT
    unsigned char c, h;
#endif
    const Actor *a;

    for (i = 0, x = 0; i < party_count; ++i, x += 10) {
        a = &party[i];
        t_clear(x, ROW_NAMES, 10);
        t_clear(x, ROW_STATS, 10);
        t_at(x, ROW_NAMES, i == act ? C_YELLOW : C_WHITE);
        t_ch(i == act ? 0x3E : 0x20);           /* > */
        t_str(a->name);
        if (gone[i]) {
            t_at(x + 1, ROW_STATS, C_CYAN);
            t_str("fled");
            continue;
        }
#ifndef FIGHT_BOT
        /* Green over half, yellow down to a quarter, red under it (as the
           open world's party lines, src/ow/stlow.s). FIGHTBOT, which no one
           watches, has no room for it: red at none, else green. */
        c = C_GREEN;
        h = a->hp;
        if (h <= a->hp_max >> 1) {
            c = h < (unsigned char)((a->hp_max + 3) >> 2) ? C_RED : C_YELLOW;
        }
        t_at(x + 1, ROW_STATS, c);
#else
        t_at(x + 1, ROW_STATS, a->hp ? C_GREEN : C_RED);
#endif
        t_num(a->hp);
        t_ch(0x2F);                             /* / */
        t_num(a->hp_max);
        if (a->mp_max) {
            t_at(x + 7, ROW_STATS, C_CYAN);
            t_num(a->mp);
        }
    }
    if (party_count < MAX_PARTY) {
        t_clear(30, ROW_NAMES, 10);
        t_clear(30, ROW_STATS, 10);
        t_at(30, ROW_NAMES, C_CYAN);
        t_str("Draught ");
        tcol = C_WHITE;
        t_num(potions);
        t_at(30, ROW_STATS, C_CYAN);
        t_str("Arrows ");
        tcol = C_WHITE;
        t_num(arrows);
    }
}

/* --- who is where --------------------------------------------------------- */

static unsigned char on_foe(unsigned char x, unsigned char y)
{
    return (unsigned char)(x - mx) < size && (unsigned char)(y - my) < size;
}

static unsigned char foe_at(unsigned char x, unsigned char y)
{
    unsigned char i;

    for (i = 0; i < foes; ++i) {
        if (foe_hp[i] && (unsigned char)(x - foe_x[i]) < size
                      && (unsigned char)(y - foe_y[i]) < size) {
            return i;
        }
    }
    return NONE;
}

static unsigned char member_at(unsigned char x, unsigned char y)
{
    unsigned char i;

    for (i = 0; i < party_count; ++i) {
        if (here(i) && mem_x[i] == x && mem_y[i] == y) {
            return i;
        }
    }
    return NONE;
}

/* --- blows ---------------------------------------------------------------- */

static unsigned char strike(unsigned int m_hp, unsigned char atk)
{
    unsigned char hit;

    hit = atk + party_level + (unsigned char)(rand() % 3);
    hit = (hit > fq_def) ? hit - fq_def : 1;

    cat_num(hit);
    strcat(msg, ".");
    say(msg);
    return hit >= m_hp ? (unsigned char)m_hp : hit;
}

static void say_who(unsigned char who, const char *rest)
{
    strcpy(msg, party[who].name);
    strcat(msg, rest);
}

/* Member `who` takes a blow of `atk`; msg holds the line up to the number.
   Returns 1 if that was the last of the party standing. */
static unsigned char bitten(unsigned char who, unsigned char atk)
{
    Actor *a = &party[who];
    unsigned char hit, def;

    def = armour_def[a->armour] + guard[who] + ward
        + (a->cls == CL_WIZARD ? WIZ_GUARD : 0);
    hit = atk + (unsigned char)(rand() % 3);
    hit = hit > def ? hit - def : 1;

    cat_num(hit);
    strcat(msg, ".");
    say(msg);
    flash(who, 1, mem_x[who], mem_y[who]);

    if (hit < a->hp) {
        a->hp -= hit;
        show_party();
        return 0;
    }
    a->hp = 0;                /* fallen: nobody with no health stands */
    show_party();
    vanish(who, 1);
    say_who(who, " falls!");
    say(msg);
    for (hit = 0; hit < party_count; ++hit) {
        if (here(hit)) {
            return 0;
        }
    }
    return 1;                 /* (the rest may have got away: over()) */
}

/* --- keys ----------------------------------------------------------------- */

static unsigned char held;      /* frames the stick has been held */

#ifdef FIGHT_BOT
/* --- the autoplayer -------------------------------------------------------
 *
 * Only in FIGHTBOT, the module compiled a second time with FIGHT_BOT, which
 * the multicolour arena's `*` (src/owarena.c) loads to fight a poked fight
 * many times over for tools/balance.py --ow: battle.c's autoplayer, eight
 * ways. It feeds next_key() what a careful player would press, and knows
 * nothing the player could not see.
 *
 * Each turn: a draught for whoever is worst hurt, if they are badly hurt; a
 * wizard heals, wards against something big, burns a crowd, or bolts what
 * is in line; whoever has a bow shoots what is in line, any of eight ways;
 * whoever is beside it strikes; otherwise each closes in, the ranged ones to
 * get in a line without standing beside it, the rest to get beside it. */
static unsigned char bot_q[4], bot_n, bot_i;

static const unsigned char dir_key[8] = {
    K_S, K_W, K_A, K_D, K_Z, K_X, K_Q, K_E,
};

static unsigned char pinched(unsigned char px, unsigned char py,
                             unsigned char x, unsigned char y);

static void bot_push(unsigned char k)
{
    bot_q[bot_n++] = k;
}

/* Is a foe in line `dir` of the active member, within `range`, with nothing
   solid and none of the party between? (A shot hits whoever is in the way,
   as loose() has it.) */
static unsigned char bot_line(unsigned char dir, unsigned char range)
{
    unsigned char x = hx, y = hy, n;

    for (n = 0; n < range; ++n) {
        x += dir_dx[dir];
        y += dir_dy[dir];
        if (x >= FW || y >= FH
            || pinched(x - dir_dx[dir], y - dir_dy[dir], x, y)) {
            return 0;
        }
        if (foe_at(x, y) != NONE) {
            return 1;
        }
        if (member_at(x, y) != NONE
            || (flag_at(x, y) & FF_KIND) == FF_BLOCK) {
            return 0;                   /* (a shot would hit them) */
        }
    }
    return 0;
}

static unsigned char bot_aim(unsigned char range)
{
    unsigned char d;

    for (d = 0; d < 8; ++d) {
        if (bot_line(d, range)) {
            return d;
        }
    }
    return NONE;
}

/* Could the active member step `d` from (hx,hy), as fight() allows? */
static unsigned char bot_open(unsigned char d)
{
    unsigned char x = hx + dir_dx[d], y = hy + dir_dy[d];

    return x < FW && y < FH && !wall_at(x, y)
        && member_at(x, y) == NONE && foe_at(x, y) == NONE
        && !(d > DIR_RIGHT && wall_at(x, hy) && wall_at(hx, y));
}

/* Is any foe beside (x,y), any of eight ways? */
static unsigned char bot_beside(unsigned char x, unsigned char y)
{
    unsigned char k;

    for (k = 0; k < 8; ++k) {
        if (foe_at(x + dir_dx[k], y + dir_dy[k]) != NONE) {
            return 1;
        }
    }
    return 0;
}

static unsigned char bot_alive(void)
{
    unsigned char i, n = 0;

    for (i = 0; i < party_count; ++i) {
        n += here(i);
    }
    return n;
}

/* Cast spell `s` at `arg`: a direction (kind 1), a member (2), or nothing. */
static void bot_cast(unsigned char s, unsigned char arg, unsigned char kind)
{
    bot_push(K_CAST);
    if (party[act].ready & (party[act].ready - 1)) {
        bot_push(0x31 + s);     /* (one spell ready is cast unasked) */
    }
    if (kind == 1) {
        bot_push(dir_key[arg]);
    } else if (kind == 2 && bot_alive() > 1) {
        bot_push(0x31 + arg);
    }
}

/* How good a square (x,y) is to stand on, against a foe at (tx,ty): lower
   is better. The rest want to be beside it, by the eight-way distance; the
   ranged want to be in one of its eight lines with none of the party (but
   whoever moves) or anything solid in between, and nearer along it. */
static unsigned char tx, ty, ranged;

static unsigned char bot_clear(unsigned char x, unsigned char y)
{
    signed char dx = tx > x ? 1 : tx < x ? -1 : 0;
    signed char dy = ty > y ? 1 : ty < y ? -1 : 0;

    for (;;) {
        x += dx;
        y += dy;
        if (x == tx && y == ty) {
            return 1;
        }
        if ((member_at(x, y) != NONE && (x != hx || y != hy))
            || (flag_at(x, y) & FF_KIND) == FF_BLOCK) {
            return 0;
        }
    }
}

static unsigned char bot_score(unsigned char x, unsigned char y)
{
    unsigned char ax = x > tx ? x - tx : tx - x;
    unsigned char ay = y > ty ? y - ty : ty - y;
    unsigned char dist = ax > ay ? ax : ay, off;

    if (!ranged) {
        return (dist << 4) + ax + ay;
    }
    off = ax < ay ? ax : ay;                     /* off the row or column */
    if ((unsigned char)(dist - off) < off) {
        off = dist - off;                         /* ... or the diagonal */
    }
    if (!off && dist && !bot_clear(x, y)) {
        off = 1;                                  /* in line, but blocked */
    }
    return (off << 4) + dist;
}

static void bot_plan(void)
{
    Actor *a = &party[act];
    unsigned char i, low = NONE, d, best, s, nx, ny, dist;

    for (i = 0; i < party_count; ++i) {
        if (here(i) && (low == NONE
            || (unsigned int)party[i].hp * party[low].hp_max
               < (unsigned int)party[low].hp * party[i].hp_max)) {
            low = i;
        }
    }
    /* In danger: the hardest blow it has could fell them, or they are under
       a third of their health. */
    i = (fq_shot > fq_atk ? fq_shot : fq_atk) + 2;
    d = armour_def[party[low].armour] + ward
      + (party[low].cls == CL_WIZARD ? WIZ_GUARD : 0);
    s = party[low].hp <= (i > d ? i - d : 1)
     || party[low].hp * 3 < party[low].hp_max;

    if (a->cls == CL_WIZARD && (a->ready & (1 << SP_HEAL))
        && a->mp >= spell_cost[SP_HEAL] && s) {
        bot_cast(SP_HEAL, low, 2);
        return;
    }
    if (potions && s && party[low].hp < party[low].hp_max) {
        bot_push(K_POTION);
        if (bot_alive() > 1) {
            bot_push(0x31 + low);
        }
        return;
    }
    if (a->cls == CL_WIZARD) {
        if ((a->ready & (1 << SP_WARD)) && !ward && fq_hp >= 40
            && a->mp >= spell_cost[SP_WARD]) {
            bot_cast(SP_WARD, 0, 0);
            return;
        }
        if ((a->ready & (1 << SP_BLAZE)) && alive >= 3
            && a->mp >= spell_cost[SP_BLAZE]) {
            bot_cast(SP_BLAZE, 0, 0);
            return;
        }
        if ((a->ready & (1 << SP_SLEEP)) && !sleep && alive >= 3
            && a->mp >= spell_cost[SP_SLEEP]) {
            bot_cast(SP_SLEEP, 0, 0);
            return;
        }
        if ((a->ready & (1 << SP_BOLT)) && a->mp >= spell_cost[SP_BOLT]
            && (d = bot_aim(14)) != NONE) {
            bot_cast(SP_BOLT, d, 1);
            return;
        }
        if ((a->ready & (1 << SP_SPARK)) && a->mp >= spell_cost[SP_SPARK]
            && (d = bot_aim(8)) != NONE) {
            bot_cast(SP_SPARK, d, 1);
            return;
        }
    }
    if (a->bow && arrows && (d = bot_aim(bow_range[a->bow])) != NONE) {
        bot_push(K_FIRE);
        bot_push(dir_key[d]);
        return;
    }
    for (d = 0; d < 8; ++d) {
        if (foe_at(hx + dir_dx[d], hy + dir_dy[d]) != NONE) {
            bot_push(dir_key[d]);
            return;
        }
    }

    /* The nearest, by the nearest square of its body. */
    best = 255;
    tx = hx;
    ty = hy;
    for (i = 0; i < foes; ++i) {
        if (!foe_hp[i]) {
            continue;
        }
        nx = foe_x[i] + (hx > foe_x[i] ? (hx - foe_x[i] < size ? hx - foe_x[i]
                                                                : size - 1) : 0);
        ny = foe_y[i] + (hy > foe_y[i] ? (hy - foe_y[i] < size ? hy - foe_y[i]
                                                                : size - 1) : 0);
        s = nx > hx ? nx - hx : hx - nx;
        dist = ny > hy ? ny - hy : hy - ny;
        if (dist > s) {
            s = dist;
        }
        if (s < best) {
            best = s;
            tx = nx;
            ty = ny;
        }
    }
    ranged = (a->bow && arrows)
          || (a->cls == CL_WIZARD && a->mp >= spell_cost[SP_SPARK]
              && (a->ready & (1 << SP_SPARK)));

    /* The step that does best, if any does better than standing. */
    best = bot_score(hx, hy);
    i = NONE;
    for (d = 0; d < 8; ++d) {
        nx = hx + dir_dx[d];
        ny = hy + dir_dy[d];
        if (!bot_open(d) || (ranged && bot_beside(nx, ny))) {
            continue;
        }
        s = bot_score(nx, ny);
        if (s < best) {
            best = s;
            i = d;
        }
    }
    bot_push(i == NONE ? K_SPACE : dir_key[i]);
}

static unsigned char bot_key(void)
{
    if (bot_i == bot_n) {
        bot_i = bot_n = 0;
        bot_plan();
    }
    return bot_q[bot_i++];
}
#endif

/* The next key, with SHIFTed letters folded onto unshifted ones, or the
   stick in port 2: its directions as the cursor keys and its diagonals as
   Q, E, Z and X, repeating while held, and its button alone as the up
   arrow, which shoots. SHIFT+M stays itself (the effects). Everyone goes
   on idling meanwhile, and the cursor marks whose turn it is. */
static unsigned char next_key(void)
{
    /* By the stick's four direction bits: up 1, down 2, left 4, right 8. */
    static const unsigned char stick[16] = {
        K_FIRE, K_CRSR_UP, K_CRSR_DOWN, K_FIRE,
        K_CRSR_LEFT, K_Q, K_Z, K_CRSR_LEFT,
        K_CRSR_RIGHT, K_E, K_X, K_CRSR_RIGHT,
        K_FIRE, K_CRSR_UP, K_CRSR_DOWN, K_FIRE,
    };
    unsigned char key, j;

#ifdef FIGHT_BOT
    return bot_key();
#endif
    if (aiming) {
        v_put(V_CUR, cx, cy);
    } else {
        v_put(V_CUR, hx, hy);
    }
    --vs_y[V_CUR];      /* sorted ahead of them, so in front: its art is a
                           row lower to make up for it (mkfields.py) */
    vs_ptr[V_CUR] = SPR_CURSOR;
    for (;;) {
        frame();
        if (!KEYS_AHEAD) {
            PROBE_STAGE = 3;
        }
        key = cbm_k_getin();
        if (key) {
            break;
        }
        j = ~*(unsigned char *)0xDC00 & 0x1F;
        if (!j) {
            held = 0;
            continue;
        }
        ++held;
        if (held == 1 || (held >= 16 && !(j & 0x10))) {
            if (held >= 16) {
                held = 9;               /* again in seven frames */
            }
            key = stick[j & 15];
            break;
        }
        if (held > 200) {
            held = 100;                 /* the button, still down */
        }
    }
    PROBE_STAGE = 2;
    vs_y[V_CUR] = 0;
    return (key >= 0xC1 && key <= 0xDA && key != K_SHIFT_M) ? key - 0x80
                                                              : key;
}

static unsigned char key_dir(unsigned char key)
{
    switch (key) {
        case K_W: case K_CRSR_UP:     return DIR_UP;
        case K_S: case K_CRSR_DOWN:   return DIR_DOWN;
        case K_A: case K_CRSR_LEFT:   return DIR_LEFT;
        case K_D: case K_CRSR_RIGHT:  return DIR_RIGHT;
        case K_Q:                     return DIR_UL;
        case K_E:                     return DIR_UR;
        case K_Z:                     return DIR_DL;
        case K_X:                     return DIR_DR;
    }
    return NONE;
}

static unsigned char pick_member(const char *question)
{
    unsigned char i, n = 0, one = 0;

    for (i = 0; i < party_count; ++i) {
        if (here(i)) {
            ++n;
            one = i;
        }
    }
    if (n == 1) {
        return one;
    }
    strcpy(msg, question);
    strcat(msg, " (1-");
    cat_num(party_count);
    strcat(msg, ")");
    say(msg);
    i = next_key() - 0x31;
    if (i < party_count && here(i)) {
        return i;
    }
    say("Never mind.");
    return NONE;
}

/* ?: the fight's keys, over the text rows until a key, and the rows put
   back. The log is kept in the flood's queue, which nothing uses while
   the party chooses. */
static void help(void)
{
    static const char *const lines[6] = {
        "WASD/cursor step or strike, QEZX aslant",
        "^ aim (keys: next), SPACE shoot  * cast",
        "P  the one whose turn it is drinks",
        "SPACE stands firm   a side gap: flee",
        "M  music   SHIFT+M  effects",
        "Press a key",
    };
    unsigned char i;

    memcpy(queue_x, F_SCREEN + ROW_LOG * 40, 120);
    memcpy(queue_y, (unsigned char *)0xD800 + ROW_LOG * 40, 120);
    for (i = 0; i < 6; ++i) {
        t_clear(0, ROW_FOE + i, 40);
        t_at(0, ROW_FOE + i, i == 5 ? C_CYAN : C_WHITE);
        t_str(lines[i]);
    }
    next_key();
    memcpy(F_SCREEN + ROW_LOG * 40, queue_x, 120);
    memcpy((unsigned char *)0xD800 + ROW_LOG * 40, queue_y, 120);
    show_foe();
    show_party();
}

static unsigned char quaff(void)
{
    unsigned char healed, who;

    if (!potions) {
        say("You have no draughts.");
        return 0;
    }
    who = act;                  /* whose turn it is */
    healed = drink_potion(who);
    if (!healed) {
        say_who(who, " is unhurt.");
        say(msg);
        return 0;
    }
    sfx_play(SFX_DRINK);
    say_who(who, " drinks. ");
    cat_num(healed);
    strcat(msg, " restored.");
    say(msg);
    return 1;
}

/* Whoever fell gets up again when it is over, won or fled. */
static void get_up(void)
{
    unsigned char i;

    for (i = 0; i < party_count; ++i) {
        if (!party[i].hp) {
            party[i].hp = 1;
            say_who(i, " gets up again.");
            say(msg);
        }
    }
    show_party();
}

/* --- the monsters' way ---------------------------------------------------- */

/* Whoever of the party the monster stands beside, any of eight ways. */
static unsigned char next_to(void)
{
    unsigned char i, k, x, y;

    for (i = 0; i < party_count; ++i) {
        if (!here(i)) {
            continue;
        }
        x = mem_x[i];
        y = mem_y[i];
        for (k = 0; k < 8; ++k) {
            if (on_foe(x + dir_dx[k], y + dir_dy[k])) {
                return i;
            }
        }
    }
    return NONE;
}

static unsigned char tail;

static void seed(unsigned char x, unsigned char y)
{
    if (x < RIM_W && y < RIM_H && !seen_row[y][x]) {
        seen_row[y][x] = 1;
        queue_x[tail] = x;
        queue_y[tail] = y;
        ++tail;
    }
}

/* battle.c's flood, but eight ways: out from every one of the party at
   once, over a copy of the field with a wall round it, until it reaches the
   monster whose turn it is; the square it is reached from is its step. A
   diagonal may not squeeze between two corners of wall (a step of the
   party's may not either), which the copy cannot say once the flood has
   been through: that is `stand`, the field as it was. */
static unsigned char step_x, step_y;

static unsigned char flood(void)
{
    register unsigned char *row;
    const unsigned char *stand;
    unsigned char head = 0;
    unsigned char x, y, nx, ny, tx, ty, i, k;

    tail = 0;
    tx = mx + 1;
    ty = my + 1;
    if (size == 1) {
        stand = wall;
        memcpy(seen, wall, sizeof(seen));
        for (i = 0; i < foes; ++i) {
            if (i != cur && foe_hp[i]) {
                seen_row[foe_y[i] + 1][foe_x[i] + 1] = 1;
            }
        }
        for (i = 0; i < party_count; ++i) {
            if (here(i)) {
                seed(mem_x[i] + 1, mem_y[i] + 1);
            }
        }
    } else {
        /* A big one stands by its top left square: it is beside someone
           from any of the twelve places round them that it fits. */
        stand = roomy;
        memcpy(seen, roomy, sizeof(seen));
        for (i = 0; i < party_count; ++i) {
            if (here(i)) {
                x = mem_x[i] + 1;
                y = mem_y[i] + 1;
                seen_row[y - 1][x - 1] = seen_row[y - 1][x] = 1;
                seen_row[y][x - 1]     = seen_row[y][x]     = 1;
            }
        }
        for (i = 0; i < party_count; ++i) {
            if (here(i)) {
                x = mem_x[i] - 1;
                y = mem_y[i] - 1;
                for (k = 0; k < 16; ++k) {
                    seed(x + (k & 3), y + (k >> 2));
                }
            }
        }
    }
    if (!tail) {
        return 0;
    }

    do {
        x = queue_x[head];
        y = queue_y[head];
        ++head;

        for (k = 0; k < 8; ++k) {
            nx = x + dir_dx[k];
            ny = y + dir_dy[k];
            row = seen_row[ny];
            if (row[nx]) {
                continue;
            }
            if (k >= 4 && stand[(unsigned char)(y * RIM_W + nx)]
                       && stand[(unsigned char)(ny * RIM_W + x)]) {
                continue;               /* between two corners */
            }
            if (nx == tx && ny == ty) {
                goto found;
            }
            row[nx] = 1;
            queue_x[tail] = nx;
            queue_y[tail] = ny;
            ++tail;
        }
    } while (head != tail);
    return 0;

found:
    step_x = x;
    step_y = y;
    return 1;
}

/* The monster one square nearer, gliding there; `slow` is what the square
   costs past 10 (by its top left square, for a big one). */
static void foe_step(void)
{
    unsigned char dir, k;

    slow = 0;
    if (flood() && member_at(step_x - 1, step_y - 1) == NONE) {
        --step_x;
        --step_y;
        dir = dir_of(step_x > mx ? 1 : step_x < mx ? -1 : 0,
                     step_y > my ? 1 : step_y < my ? -1 : 0);
        mx = foe_x[cur] = step_x;
        my = foe_y[cur] = step_y;
        slow = FF_COST(flag_at(mx, my)) - 10;
        foe_sprites(cur);
        for (k = 0; k < 4; ++k) {
            v_nudge(fv, fn, dir, 4);
            frame();
        }
    }
}

#define START_ROWS  (FH / 3)
#define START_TRIES 16

static unsigned char foe_start(void)
{
    unsigned char tries;
    const unsigned char *stand = size == 1 ? wall : roomy;

    for (tries = 0; tries < START_TRIES; ++tries) {
        mx = (unsigned char)(rand() % (FW - size + 1));
        my = (unsigned char)(rand() % (START_ROWS - size + 1));
        if (!stand[(unsigned char)((my + 1) * RIM_W + mx + 1)]
            && foe_at(mx, my) == NONE && member_at(mx, my) == NONE
            && flood()) {
            return 1;
        }
    }
    if (cur) {
        return 0;
    }
    mx = F_INFO[FI_MX];
    my = F_INFO[FI_MY];
    return 1;
}

/* The party starts fanned out, the leader two squares toward the monster
   and the other two two squares to either side of where the field puts
   the leader, so that whoever is at the back has a turn or two to find a
   line, or the way out, before anything reaches them:

       ..1..
       .....
       2...3

   A square that is walled, or taken, gives way to the first open one
   round it, and failing that round the field's own start. */
static const signed char form_x[MAX_PARTY] = { 0, -2, 2, 0 };  /* (a fourth: */
static const signed char form_y[MAX_PARTY] = { -2, 0, 0, 0 };  /* between) */

static unsigned char stand_ok(unsigned char x, unsigned char y,
                              unsigned char placed)
{
    unsigned char j;

    if (x >= FW || y >= FH || wall_at(x, y)) {
        return 0;
    }
    for (j = 0; j < placed; ++j) {
        if (mem_x[j] == x && mem_y[j] == y) {
            return 0;
        }
    }
    return 1;
}

static void party_start(void)
{
    unsigned char i, k, x, y, cx, cy, hx0 = F_INFO[FI_HX], hy0 = F_INFO[FI_HY];

    memcpy((void *)(SPR_SKIN * 64 + 0x4000), fight_skins, 3 * 64);
    for (i = 0; i < party_count; ++i) {
        cx = hx0 + form_x[i];
        cy = hy0 + form_y[i];
        x = cx;
        y = cy;
        for (k = 0; !stand_ok(x, y, i) && k < 16; ++k) {
            if (k < 8) {
                x = cx + dir_dx[k];
                y = cy + dir_dy[k];
            } else {
                x = hx0 + dir_dx[k - 8];
                y = hy0 + dir_dy[k - 8];
            }
        }
        if (k == 16 && !stand_ok(x, y, i)) {
            x = hx0;                    /* (never: the field's start is
                                           open, and so is round it) */
            y = hy0;
        }
        mem_x[i] = x;
        mem_y[i] = y;
    }
    for (i = 0; i < party_count; ++i) {
        guard[i] = 0;
        if (party[i].hp) {
            v_put(i, mem_x[i], mem_y[i]);
        }
        vs_col[i] = fq_tunic[i] & 15;           /* their clothes, */
        k = fq_tunic[i] >> 4;                   /* their skin (0: light red) */
        vs_col[V_SKIN + i] = k ? k : 10;
        vs_ptr[V_SKIN + i] = SPR_SKIN + party[i].cls;
    }
}

/* --- shots ---------------------------------------------------------------- */

static unsigned char target;

/* Whether a shot from (px,py) to the square diagonally beside it, (x,y),
   would pass between two corners of wall: it stops, as a step does. */
static unsigned char pinched(unsigned char px, unsigned char py,
                             unsigned char x, unsigned char y)
{
    return px != x && py != y
        && (flag_at(x, py) & FF_KIND) == FF_BLOCK && (flag_at(px, y) & FF_KIND) == FF_BLOCK;
}

#ifdef FIGHT_BOT
/* Something flies from the member whose turn it is: battle.c's loose(),
   eight ways. The autoplayer's (FIGHTBOT has no room for the aiming, and
   its figures were measured with this). */
static unsigned char loose(unsigned char dir, unsigned char range,
                           unsigned char atk, unsigned char kind,
                           const char *what)
{
    unsigned char x = hx, y = hy, flew = 0, hit = 0, ally = NONE;

    while (flew < range) {
        x += dir_dx[dir];
        y += dir_dy[dir];
        if (x >= FW || y >= FH || pinched(x - dir_dx[dir], y - dir_dy[dir],
                                          x, y)) {
            break;                           /* off the field */
        }
        ++flew;
        if ((target = foe_at(x, y)) != NONE) {
            hit = 1;
            break;
        }
        /* Nor does it fly over the party: whoever is in the way takes it,
           as hard as a foe would have. */
        if ((ally = member_at(x, y)) != NONE) {
            break;
        }
        if ((flag_at(x, y) & FF_KIND) == FF_BLOCK) {
            break;
        }
    }

    shoot(hx, hy, dir, flew, kind);
    strcpy(msg, what);
    if (ally != NONE) {
        sfx_play(SFX_BOW);
        strcat(msg, " hits ");
        strcat(msg, party[ally].name);
        strcat(msg, " for ");
        bitten(ally, atk + party_level);   /* (never the last: the one
                                              who loosed it stands) */
        return 0;
    }
    if (hit) {
        sfx_play(SFX_BOW);
        strcat(msg, " hits for ");
        hit = strike(foe_hp[target], atk);
        foe_sprites(target);
        flash(fv, fn, x, y);
        return hit;
    }
    strcat(msg, " goes wide.");
    say(msg);
    return 0;
}

static unsigned char which_way(const char *put_down)
{
    unsigned char dir;

    say("Which way?");
    dir = key_dir(next_key());
    if (dir == NONE) {
        say(put_down);
    }
    return dir;
}
#else

/* --- aiming: a shot flies at a foe the player picks, along the straight
   line to them, not only the eight ways. What can be picked is every foe
   (a square of a big one) within the range, counted as steps are, that
   the line reaches before anything that stops a shot: a wall, a boulder
   or a tree, two corners of wall a step would squeeze between, one of the
   party (no shot flies over them) or another foe. */

static unsigned char aim_n, aim_t[MAX_FOES], aim_x[MAX_FOES], aim_y[MAX_FOES];
static unsigned char lx, ly, path_n;    /* where the line meets the foe */

/* The line from the member whose turn it is to (tx, ty), at most `range`
   steps: the foe it reaches (lx, ly the square, path_n the steps), or
   NONE if anything else stops it first. Bresenham's, eight ways. */
static unsigned char trace(unsigned char tx, unsigned char ty,
                           unsigned char range)
{
    static unsigned char x, y, nx, ny, ax, ay, n, e, k, t;
    static signed char sx, sy;

    x = hx;
    y = hy;
    sx = tx < x ? -1 : tx > x;
    sy = ty < y ? -1 : ty > y;
    ax = tx < x ? x - tx : tx - x;
    ay = ty < y ? y - ty : ty - y;
    n = ax > ay ? ax : ay;
    if (!n || n > range) {
        return NONE;
    }
    e = n >> 1;
    for (k = 1; k <= n; ++k) {
        nx = x;
        ny = y;
        if (ax == n) {
            nx += sx;
            e += ay;
            if (e >= n) {
                e -= n;
                ny += sy;
            }
        } else {
            ny += sy;
            e += ax;
            if (e >= n) {
                e -= n;
                nx += sx;
            }
        }
        if (pinched(x, y, nx, ny)) {
            break;
        }
        x = nx;
        y = ny;
        if ((t = foe_at(x, y)) != NONE) {
            lx = x;
            ly = y;
            path_n = k;
            return t;
        }
        if (member_at(x, y) != NONE || (flag_at(x, y) & FF_KIND) == FF_BLOCK) {
            break;
        }
    }
    return NONE;
}

/* What can be shot at, from left to right. */
static void targets(unsigned char range)
{
    static unsigned char i, q, j;

    aim_n = 0;
    for (i = 0; i < foes; ++i) {
        if (!foe_hp[i]) {
            continue;
        }
        for (q = 0; q < size * size; ++q) {
            if (trace(foe_x[i] + (q & 1), foe_y[i] + (q >> 1), range) == i) {
                /* (into place by x, then y: a handful, at most) */
                for (j = aim_n; j && (aim_x[j - 1] > lx
                                      || (aim_x[j - 1] == lx
                                          && aim_y[j - 1] > ly)); --j) {
                    aim_t[j] = aim_t[j - 1];
                    aim_x[j] = aim_x[j - 1];
                    aim_y[j] = aim_y[j - 1];
                }
                aim_t[j] = i;
                aim_x[j] = lx;
                aim_y[j] = ly;
                ++aim_n;
                break;
            }
        }
    }
}

/* The player picks what to shoot at, within `range`: the cursor, yellow,
   starts on the nearest, the direction keys (or the stick) go through the
   rest, and SPACE, ^ (the stick's button) or * looses; any other key puts
   it down, with `put_down` said. The foe (target), with the line to it
   traced for loose(), or NONE. */
static unsigned char choose(unsigned char range, const char *put_down)
{
    static unsigned char c, i, d, best, key;

    targets(range);
    if (!aim_n) {
        say("Nothing is in reach.");
        return NONE;
    }
    best = 0xFF;
    for (i = 0; i < aim_n; ++i) {
        d = aim_x[i] < hx ? hx - aim_x[i] : aim_x[i] - hx;
        key = aim_y[i] < hy ? hy - aim_y[i] : aim_y[i] - hy;
        if (key > d) {
            d = key;
        }
        if (d < best) {
            best = d;
            c = i;
        }
    }
    say(aim_n > 1 ? "Aim: keys pick, SPACE or fire looses."
                  : "Aim: SPACE or fire looses.");
    aiming = 1;
    poses();
    for (;;) {
        cx = aim_x[c];
        cy = aim_y[c];
        key = next_key();
        if (key == K_SPACE || key == K_FIRE || key == K_CAST) {
            break;
        }
        d = key_dir(key);
        if (d == NONE) {
            say(put_down);
            c = NONE;
            break;
        }
        if (dir_dx[d] ? dir_dx[d] > 0 : dir_dy[d] > 0) {
            c = c + 1 == aim_n ? 0 : c + 1;
        } else {
            c = c ? c - 1 : aim_n - 1;
        }
    }
    aiming = 0;
    poses();
    if (c == NONE) {
        return NONE;
    }
    return target = trace(aim_x[c], aim_y[c], range);
}

/* The shot flies straight from the one whose turn it is to where the line
   meets the foe picked (choose()), two frames a step, and lands: battle.c's
   loose(), aimed. The arrow points the nearest of the eight ways. */
static unsigned char loose(unsigned char atk, unsigned char kind,
                           const char *what)
{
    static int dx, dy;
    static unsigned int px;
    static unsigned char k, n, ax, ay, py;
    static signed char sx, sy;

    dx = ((int)lx - hx) << 4;
    dy = ((int)ly - hy) << 4;
    ax = lx < hx ? hx - lx : lx - hx;
    ay = ly < hy ? hy - ly : ly - hy;
    sx = (ax << 1) < ay ? 0 : lx < hx ? -1 : 1;
    sy = (ay << 1) < ax ? 0 : ly < hy ? -1 : 1;
    v_put(V_SHOT, hx, hy);
    if (kind == SHOT_ARROW) {
        arrow_to(dir_of(sx, sy));
        vs_ptr[V_SHOT] = SPR_ARROW;
    } else {
        vs_ptr[V_SHOT] = SPR_FIRE - 1 + kind;
    }
    vs_col[V_SHOT] = shot_colour[kind];
    py = vs_y[V_SHOT];
    px = 24 + ((unsigned int)hx << 4);
    n = path_n << 1;
    for (k = 1; k <= n; ++k) {
        vs_x[V_SHOT]  = (unsigned char)(px + dx * k / n);
        vs_xh[V_SHOT] = (unsigned char)((px + dx * k / n) >> 8) & 1;
        vs_y[V_SHOT]  = py + (signed char)(dy * k / n);
        frame();
    }
    vs_y[V_SHOT] = 0;

    strcpy(msg, what);
    sfx_play(SFX_BOW);
    strcat(msg, " hits for ");
    k = strike(foe_hp[target], atk);
    foe_sprites(target);
    flash(fv, fn, lx, ly);
    return k;
}
#endif


/* The monster's own shot: battle.c's aim_at() and aim(). */
static unsigned char sx, sy, flew, victim;

static unsigned char aim_at(unsigned char tx, unsigned char ty)
{
    signed char dx, dy;
    unsigned char dir, x, y, ax, ay;

    /* Which way they are from the body, and the square of it the shot
       leaves from: level with them, or the corner toward them. */
    dx = tx < mx ? -1 : tx >= mx + size;
    dy = ty < my ? -1 : ty >= my + size;
    sx = dx < 0 ? mx : dx ? mx + size - 1 : tx;
    sy = dy < 0 ? my : dy ? my + size - 1 : ty;
    if (dx && dy) {                     /* a diagonal, or not in line */
        ax = dx > 0 ? tx - sx : sx - tx;
        ay = dy > 0 ? ty - sy : sy - ty;
        if (ax != ay) {
            return NONE;
        }
    }
    dir = dir_of(dx, dy);

    x = sx;
    y = sy;
    for (flew = 1; flew <= fq_reach; ++flew) {
        x += dir_dx[dir];
        y += dir_dy[dir];
        if (x >= FW || y >= FH
            || pinched(x - dir_dx[dir], y - dir_dy[dir], x, y)) {
            break;
        }
        if ((victim = member_at(x, y)) != NONE) {
            return dir;
        }
        if ((flag_at(x, y) & FF_KIND) == FF_BLOCK || foe_at(x, y) != NONE) {
            break;
        }
    }
    return NONE;                             /* out of reach, or blocked */
}

static unsigned char aim(void)
{
    unsigned char i, dir;

    if (!fq_reach) {
        return NONE;
    }
    for (i = 0; i < party_count; ++i) {
        if (here(i) && (dir = aim_at(mem_x[i], mem_y[i])) != NONE) {
            return dir;
        }
    }
    return NONE;
}

/* --- the ground ----------------------------------------------------------- */

/* Is the square at `w` in wall[], and all eight round it, open? */
static unsigned char open9(const unsigned char *w)
{
    w -= RIM_W + 1;
    return !(w[0] | w[1] | w[2]
           | w[RIM_W] | w[RIM_W + 1] | w[RIM_W + 2]
           | w[2 * RIM_W] | w[2 * RIM_W + 1] | w[2 * RIM_W + 2]);
}

/* No two fights are fought on the same ground: two to four things that
   block go down -- boulders, or what else the field may scatter that blocks
   (a tree) -- where the ground is open all round (so nothing is cut off),
   never on the leader's square; then, where the field has some, one to
   three patches of slow ground (a puddle, a thicket) on open squares that
   are quicker to cross than they would be. The field's file says what it
   may scatter (FI_SCAT, from assets/fields.txt). */
static unsigned char rock5[5];

static void put(unsigned char x, unsigned char y, const unsigned char *e)
{
    unsigned char *s = F_SCREEN + (unsigned int)y * 80 + (x << 1);

    s[0]  = e[0];
    s[1]  = e[1];
    s[40] = e[2];
    s[41] = e[3];
    flag_at(x, y) = e[4];
}

static void scatter(void)
{
    const unsigned char *blk[1 + 3], *slw[3], *e;
    unsigned char n, nb = 0, ns = 0, tries, x, y, f;
    unsigned char *w;

    memcpy(rock5, F_INFO + FI_ROCK, 4);
    rock5[4] = FF_BLOCK;
    blk[nb++] = rock5;
    for (n = 0; n < F_INFO[FI_NSCAT]; ++n) {
        e = F_INFO + FI_SCAT + n * 5;
        if (e[4] == FF_BLOCK) {
            blk[nb++] = e;
        } else {
            slw[ns++] = e;
        }
    }

    n = 2 + (unsigned char)(rand() % 3);
    for (tries = 0; tries < 40; ++tries) {
        x = 2 + ((unsigned char)rand() & 15);
        y = 1 + (unsigned char)(rand() % 7);
        w = &wall_at(x, y);
        if ((unsigned char)(x - F_INFO[FI_HX] + 2) > 4
            || (unsigned char)(y - F_INFO[FI_HY] + 2) > 2) {
            /* (not in the party's starting box: party_start()) */
            if (open9(w)) {
                *w = 1;
                put(x, y, blk[(unsigned char)rand() % nb]);
                if (!--n) {
                    break;
                }
            }
        }
    }

    if (!ns) {
        return;
    }
    n = 1 + (unsigned char)(rand() % 3);
    for (tries = 0; tries < 40; ++tries) {
        x = 1 + (unsigned char)(rand() % (FW - 2));
        y = (unsigned char)(rand() % FH);
        e = slw[(unsigned char)rand() % ns];
        f = flag_at(x, y);
        if ((f & FF_KIND) == FF_OPEN && (f & FF_SLOW) < (e[4] & FF_SLOW)
            && (x != F_INFO[FI_HX] || y != F_INFO[FI_HY])) {
            put(x, y, e);
            if (!--n) {
                break;
            }
        }
    }
}

/* --- spells --------------------------------------------------------------- */

static unsigned char hurt_all;

static unsigned char cast(unsigned char *spent)
{
    Actor *a = &party[act];
    unsigned char s, who, amount, one;
#ifdef FIGHT_BOT
    unsigned char dir;
#endif

    *spent = 0;
    hurt_all = 0;
    if (a->cls != CL_WIZARD) {
        say("Only a wizard can cast.");
        return 0;
    }
    if (!a->ready) {
        say("No spell is prepared.");
        return 0;
    }
    strcpy(msg, "Cast");
    for (s = 0; s < NUM_SPELLS; ++s) {
        if (a->ready & (1 << s)) {
            one = s;
            strcat(msg, " ");
            cat_num(s + 1);
            strcat(msg, " ");
            strcat(msg, spell_names[s]);
        }
    }
    if (a->ready & (a->ready - 1)) {
        say(msg);
        s = next_key() - 0x31;
    } else {
        s = one;                /* the one spell ready: no need to ask */
        strcpy(msg, "Cast ");
        strcat(msg, spell_names[s]);
        say(msg);
    }
    if (s >= NUM_SPELLS || !(a->ready & (1 << s))) {
        say("Never mind.");
        return 0;
    }
    if (a->mp < spell_cost[s]) {
        say("Not enough magic.");
        return 0;
    }

    switch (s) {
        case SP_SPARK:
        case SP_BOLT:
#ifdef FIGHT_BOT
            dir = which_way("The spell fades.");
            if (dir == NONE) {
                return 0;
            }
#else
            if (choose(s == SP_SPARK ? 8 : 14, "The spell fades.") == NONE) {
                return 0;
            }
#endif
            break;
        case SP_HEAL:
            who = pick_member("Heal whom?");
            if (who == NONE) {
                return 0;
            }
            break;
        case SP_WARD:
            if (ward) {
                say("The ward already holds.");
                return 0;
            }
            break;
    }

    a->mp -= spell_cost[s];
    *spent = 1;
    switch (s) {
#ifdef FIGHT_BOT
        case SP_SPARK:
            return loose(dir, 8, 4 + party_level, SHOT_FIRE, "The spark");
        case SP_BOLT:
            return loose(dir, 14, 8 + party_level + (party_level >> 1),
                         SHOT_BOLT, "The bolt");
#else
        case SP_SPARK:
            return loose(4 + party_level, SHOT_FIRE, "The spark");
        case SP_BOLT:
            return loose(8 + party_level + (party_level >> 1), SHOT_BOLT,
                         "The bolt");
#endif
        case SP_HEAL:
            amount = 8 + 2 * party_level;
            a = &party[who];
            if (a->hp_max - a->hp < amount) {
                amount = a->hp_max - a->hp;
            }
            a->hp += amount;
            say_who(who, " is mended: ");
            cat_num(amount);
            strcat(msg, ".");
            say(msg);
            return 0;
        case SP_WARD:
            ward = WARD;
            say("A ward settles on the party.");
            return 0;
        case SP_SLEEP:
            sleep = lone ? party_count << 1 : 2;
            cat3("A drowse falls on ", p_obj[foes_pron()], ".");
            say(msg);
            return 0;
        default:                             /* SP_BLAZE */
            hurt_all = 1;
            sfx_play(SFX_BOW);
            cat3("Fire takes ", p_obj[foes_pron()], " for ");
            s = strike(0xFFFF, 4 + party_level);
            for (who = 0; who < foes; ++who) {
                if (foe_hp[who]) {
                    foe_sprites(who);
                    vs_col[fv] = C_WHITE;
                }
            }
            pause(6);
            for (who = 0; who < MAX_FOES; ++who) {
                vs_col[V_FOE + who] = fq_colour;
            }
            return s;
    }
}

/* --- the fight ------------------------------------------------------------ */

static void turn_to(unsigned char i)
{
    act = i;
    hx = mem_x[i];
    hy = mem_y[i];
}

/* Take `dmg` off foe `t`; returns 1 if that was the last of them. */
static unsigned char wound(unsigned char t, unsigned char dmg)
{
    if (dmg >= foe_hp[t]) {
        foe_hp[t] = 0;
        fell = t;
        --alive;
        show_foe();
        foe_sprites(t);
        vanish(fv, fn);
        if (!alive) {
            return 1;
        }
        say_did(foe_pron[t], " fall");
        return 0;
    }
    foe_hp[t] -= dmg;
    show_foe();
    return 0;
}

static const char *const shot_hits[] = {
    "An arrow hits ",
    "Flame sears ",
    "A wave hits ",
    "Lightning strikes ",
};

/* Their turn: battle.c's, with the blows and the shots shown. */
static unsigned char their_turn(void)
{
    unsigned char i, dir, pace;

    if (sleep) {
        --sleep;
        say_did(foes_pron(), sleep ? " sleep" : " stir");
        return 0;
    }
    for (cur = 0; cur < foes; ++cur) {
        if (!foe_hp[cur]) {
            continue;
        }
        mx = foe_x[cur];
        my = foe_y[cur];
        pace = foe_pace[cur] + rel;
        for (; pace >= 10; pace -= 10) {
            if ((i = next_to()) != NONE) {
                dir = foes > 1 ? PRON_HE : foe_pron[cur];   /* (agrees) */
                cat3(foes > 1 ? (const char *)fq_name : p_subj[dir],
                     dir ? " hits " : " hit ", party[i].name);
                strcat(msg, " for ");
                foe_sprites(cur);
                /* Toward them: by the nearest part of a big body. */
                dir = dir_of(mem_x[i] < mx ? -1 : mem_x[i] >= mx + size,
                             mem_y[i] < my ? -1 : mem_y[i] >= my + size);
                sfx_delay();         /* let the blow before it finish */
                sfx_play(SFX_MELEE);
                lunge(fv, fn, dir);
                if (bitten(i, fq_atk + fury)) {
                    return 1;
                }
            } else if ((dir = aim()) != NONE) {
                shoot(sx, sy, dir, flew, fq_kind);
                sfx_delay();
                sfx_play(SFX_BOW);
                strcpy(msg, shot_hits[fq_kind]);
                strcat(msg, party[victim].name);
                strcat(msg, " for ");
                if (bitten(victim, fq_shot + fury)) {
                    return 1;
                }
                pace %= 10;
                break;
            } else {
                /* Slow ground spends more of its pace: what is left of it
                   may not be enough for another move this round. */
                foe_step();
                if (pace >= 10 + slow) {
                    pace -= slow;
                } else {
                    pace = 10;          /* (and the loop takes that) */
                }
            }
        }
        foe_pace[cur] = pace;
    }
    return 0;
}

/* A level reached: everyone on the field flashes through the colours to a
   fanfare. (The caller raises them: ow_fight.c's award().) */
static void level_up(void)
{
    static const unsigned char glow[8] = { 1, 7, 13, 3, 14, 4, 10, 7 };
    unsigned char k, i;

    sfx_delay();
    sfx_play(SFX_LEVEL);
    say(party_count > 1 ? "The party grows stronger!" : "You grow stronger!");
    for (k = 0; k < 32; ++k) {
        for (i = 0; i < party_count; ++i) {
            vs_col[i] = glow[(k + i) & 7];
        }
        pause(2);
    }
    for (i = 0; i < party_count; ++i) {
        vs_col[i] = fq_tunic[i] & 15;
    }
}

static unsigned char won(void)
{
    unsigned char got;

    say_did(foe_pron[fell], " fall");
    get_up();
    if (fq_to_level && fq_xp_each * fq_foes >= fq_to_level) {
        level_up();
    }
    if (party_has_bow() && arrows < ARROW_MAX && !(rand() & 3)) {
        got = 1 + (unsigned char)(rand() % 4);
        arrows += got;
        if (arrows > ARROW_MAX) {
            arrows = ARROW_MAX;
        }
        strcpy(msg, "You gather ");
        cat_num(got);
        strcat(msg, " arrows.");
        say(msg);
        show_party();
    }
    pause(50);
    return BATTLE_WON;
}

/* The last of them got away: the fallen are carried off, and the monsters
   left are what the caller remembers (fq_foes). */
static unsigned char got_away(void)
{
    say(party_count > 1 ? "The party gets away." : "You get away.");
    get_up();
    fq_foes = alive;
    pause(40);
    return BATTLE_FLED;
}

static unsigned char lost(void)
{
    if (fled_n) {
        return got_away();      /* the rest are fallen, but some got out */
    }
    say("The party has fallen.");
#ifndef FIGHT_BOT
    say("Press a key.");
    while (!cbm_k_getin()) {
        frame();
    }
#endif
    return BATTLE_DEAD;
}

/* The field's walls, the boulders, and everyone in their places. */
static void set_up(void)
{
    unsigned char x, y, i, pron;
    unsigned int  n, hp;

    size = fq_size == 2 ? 2 : 1;
    memset(wall, 1, sizeof(wall));
    for (y = 0; y < RIM_H; ++y) {
        seen_row[y] = seen + y * RIM_W;
    }
    for (y = 0; y < FH; ++y) {
        for (x = 0; x < FW; ++x) {
            wall_at(x, y) = (flag_at(x, y) & FF_KIND) != FF_OPEN;
        }
    }
    if (size == 1) {
        scatter();
    } else {
        memset(roomy, 1, sizeof(roomy));
        for (n = 0; n < sizeof(roomy) - RIM_W - 1; ++n) {
            roomy[n] = wall[n] | wall[n + 1]
                     | wall[n + RIM_W] | wall[n + RIM_W + 1];
        }
    }
    act = NONE;
    party_start();

    /* One that comes alone is the tougher for each of the party past the
       first: battle.c's LONE_HP. */
    hp = fq_hp;
    x = size == 2 || !fq_count ? 1 : fq_count > MAX_FOES ? MAX_FOES : fq_count;
    if (x == 1) {
        hp += (hp * LONE_HP * (party_count - 1)) >> 2;
    }
    foes = x;
    for (cur = 0; cur < x && foe_start(); ++cur) {
        foe_x[cur] = mx;
        foe_y[cur] = my;
        foe_hp[cur] = hp;
        foe_place(cur);
    }
    foes = alive = fq_foes = cur;
    lone = foes == 1;
    /* Pronouns: a person's rolled for each, from the fight's seed rather
       than rand(), so that the dice of a fight stay as they were. */
    pron = (fq_kind >> 6) ^ 3;          /* (battle.h's MP_*) */
    fq_kind &= SHOT_KIND;
    y = (unsigned char)fq_seed % 3;
    for (cur = 0; cur < foes; ++cur) {
        foe_pron[cur] = pron ? pron : y;
        y = y == 2 ? 0 : y + 1;         /* (a band: each in turn) */
    }
    fury = lone ? LONE_ATK * (party_count - 1) : 0;

    n = 0;
    for (i = 0; i < party_count; ++i) {
        n += armour_slow[party[i].armour];
    }
    rel = fq_speed + (unsigned char)(n / party_count);
    poses();
}

static unsigned char fight(void)
{
    unsigned char key, dir, nx, ny, dmg, spent, i;

    strcpy(msg, fq_name);
    strcat(msg, party_count > 1 ? " comes at the party!" : " comes at you!");
    say(msg);
    if (foes > 1) {
        strcpy(msg, "And ");
        cat_num(foes - 1);
        strcat(msg, " more!");
        say(msg);
    }
    cat3("Walk into ", p_obj[foes_pron()],
         fq_flee ? " to strike. Flee by a side." : " to strike.");
    say(msg);

    for (;;) {
        /* A round. One the autoplayer cannot finish (shut out of reach) is
           called off: battle.c's, too. */
        if (++fq_rounds > 300 && FIGHT_BOT_ON) {
            return BATTLE_FLED;
        }
        for (i = 0; i < party_count; ++i) {
            if (!here(i)) {
                continue;
            }
            turn_to(i);
            guard[i] = 0;
            if (party[i].mp < party[i].mp_max) {
                ++party[i].mp;
            }
            if (mire[i] >= 10) {
                /* Slow ground: this turn goes on getting through it. */
                mire[i] -= 10;
                show_party();
                say_who(i, " is slowed by the ground.");
                say(msg);
                pause(20);
                goto turn_done;
            }
            for (;;) {
                show_party();
                show_foe();

                dmg = 0;
                hurt_all = 0;
                key = next_key();
                dir = key_dir(key);

                switch (key) {
                    case K_FIRE:
                        if (party[act].cls == CL_WIZARD) {
                            goto cast_it;    /* a wizard's fire is a spell */
                        }
                        if (!party[act].bow) {
                            say_who(act, " has no bow.");
                            say(msg);
                            continue;
                        }
                        if (!arrows) {
                            say("You have no arrows.");
                            continue;
                        }
#ifdef FIGHT_BOT
                        dir = which_way("You lower the bow.");
                        if (dir == NONE) {
                            continue;                /* no turn spent */
                        }
                        --arrows;
                        dmg = loose(dir, bow_range[party[act].bow],
#else
                        if (choose(bow_range[party[act].bow],
                                   "You lower the bow.") == NONE) {
                            continue;                /* no turn spent */
                        }
                        --arrows;
                        dmg = loose(
#endif
                                    bow_atk[party[act].bow]
                                    + (party[act].cls == CL_ARCHER
                                       ? ARCHER_EYE : 0), SHOT_ARROW,
                                    party[act].bow == NUM_BOWS - 1
                                        ? "The bolt" : "The arrow");
                        dir = NONE;                  /* shooting is not walking */
                        break;

                    case K_CAST:
                    cast_it:
                        dmg = cast(&spent);
                        if (!spent) {
                            continue;
                        }
                        dir = NONE;
                        break;

                    case K_SPACE:
                        guard[act] = 4;
                        say_who(act, " stands firm.");
                        say(msg);
                        break;

                    case K_POTION:
                        if (!quaff()) {
                            continue;
                        }
                        break;

                    case K_HELP:                 /* no turn spent */
                        help();
                        continue;

                    case K_M:                    /* no turn spent */
                        fq_quiet ^= 1;
                        if (fq_quiet & 1) {
                            ow_tune_stop();
                        } else if (fq_song) {
                            ow_tune_play(fq_song - 1);
                        }
                        say(fq_quiet & 1 ? "Music off." : "Music on.");
                        continue;

                    case K_SHIFT_M:
                        fq_quiet ^= 2;
                        say(sfx_toggle() ? "Effects on." : "Effects off.");
                        continue;

                    default:
                        if (dir == NONE) {
                            continue;
                        }
                        break;
                }

                if (dir != NONE) {
                    nx = hx + dir_dx[dir];
                    ny = hy + dir_dy[dir];

                    if ((target = foe_at(nx, ny)) != NONE) {
                        say_who(act, " strikes for ");
                        sfx_play(SFX_MELEE);
                        lunge(act, 1, dir);
                        dmg = strike(foe_hp[target],
                                     weapon_atk[party[act].weapon]);
                        foe_sprites(target);
                        flash(fv, fn, nx, ny);
                    } else if (nx >= FW || ny >= FH || wall_at(nx, ny)
                               || member_at(nx, ny) != NONE
                               || (dir > DIR_RIGHT && wall_at(nx, hy)
                                   && wall_at(hx, ny))) {
                        /* (Nor between two corners of wall.) */
                        /* Unsigned: off the top or the left wraps to 255.
                           The gaps in the middle of the sides are the way
                           out, for the whole party. */
                        if (fq_flee && ny == FH / 2 && nx >= FW) {
                            /* Out by a side: off the field until the
                               rest of the party is out too. */
                            for (key = 0; key < 8; ++key) {
                                v_nudge(act, 1, dir, 2);
                                frame();
                            }
                            vs_y[act] = 0;
                            gone[act] = 1;
                            ++fled_n;
                            for (key = 0; key < party_count; ++key) {
                                if (here(key)) {
                                    break;
                                }
                            }
                            if (key == party_count) {
                                return got_away();
                            }
                            say_who(act, " gets away.");
                            say(msg);
                            act = NONE;
                            goto turn_done;
                        }
                        continue;                    /* bumping costs no turn */
                    } else {
                        sfx_step(ground);
                        for (key = 0; key < 8; ++key) {
                            v_nudge(act, 1, dir, 2);
                            frame();
                        }
                        hx = mem_x[act] = nx;
                        hy = mem_y[act] = ny;
                        mire[act] += FF_COST(flag_at(nx, ny)) - 10;
                    }
                }
                break;
            }

            if (dmg) {
                if (hurt_all) {
                    for (target = 0; target < foes; ++target) {
                        if (foe_hp[target] && wound(target, dmg)) {
                            return won();
                        }
                    }
                } else if (wound(target, dmg)) {
                    return won();
                }
            }
turn_done:
            /* Alone, it has (n + 1) / 2 turns a round for a party of n,
               spread between theirs: battle.c. */
            if (lone) {
                lone_bank += party_count + 1;
                if (lone_bank >= party_count << 1) {
                    lone_bank -= party_count << 1;
                    show_party();
                    if (their_turn()) {
                        return lost();
                    }
                }
            }
        }

        if (!lone) {
            act = NONE;
            show_party();
            if (their_turn()) {
                return lost();
            }
        }
    }
}

/* A field, packed (tools/mkfields.py's pack(): a byte n < 128 and n + 1
   bytes as they are, n >= 128 and the next byte n - 125 times), from where
   it loads to its place at $4000. */
#define F_PACKED ((const unsigned char *)0xC000)
#define F_SIZE   0x0C00

static void unpack(void)
{
    const unsigned char *src = F_PACKED;
    unsigned char *dst = F_SCREEN;
    unsigned char n;

    while (dst < F_SCREEN + F_SIZE) {
        n = *src++;
        if (n < 128) {
            memcpy(dst, src, ++n);
            src += n;
        } else {
            n -= 125;
            memset(dst, *src++, n);
        }
        dst += n;
    }
}

/* The game's tile for the field's ground, by its name (f.grass, f.forest,
   f.shore, f.swamp, f.floor, f.lair: assets/fields.txt), for the sound of
   a step on it (sfx_step()). */
static unsigned char ground_of(void)
{
    switch (fq_field[2]) {
    case 0x47: return T_GRASS;                                  /* g */
    case 0x46: return fq_field[3] == 0x4F ? T_FOREST : T_FLOOR; /* fo */
    case 0x53: return fq_field[3] == 0x48 ? T_SHORE : T_SWAMP;  /* sh */
    }
    return T_FLOOR;
}

/* The module's way in (head.s): load what the block names, fight, and
   leave the machine as it was. The music is the caller's tune, carried on
   or started, and paused where it is at the end (silence first, which has
   to reach VICE's sound buffer before the next load: music.s), so that the
   caller can carry on with it in turn. */
unsigned char fight_run(void)
{
    unsigned char dev = *(unsigned char *)0xBA, r;

    if (dev < 8) {
        dev = 8;
    }
    if (fq_sprite[0] &&         /* (none: the caller has loaded them) */
        (!cbm_load(fq_field, dev, 0) || !cbm_load(fq_sprite, dev, 0))) {
        return FIGHT_NODISK;
    }
    unpack();
    srand(fq_seed);
    fq_rounds = 0;
    mx_font();
    set_up();
    mx_colour();
    memset(F_COLOR + ROW_FOE * 40, C_WHITE, 6 * 40);
    ground = ground_of();
    if (fq_quiet & 2) {
        sfx_toggle();                   /* (the BSS starts them on) */
    }
    mx_start();
    if (fq_song && !(fq_quiet & 1)) {
        if (fq_carry) {
            ow_tune_resume(fq_song - 1);
        } else {
            ow_tune_play(fq_song - 1);
        }
    }
    r = fight();
    sfx_delay();                        /* the last blow heard out */
    fq_carry = ow_tune_on;
    ow_tune_pause();
    pause(12);
    mx_stop();
    return r;
}
