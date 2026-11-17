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
 * steps and strikes diagonally too (Q E Z C, or the stick's diagonals, so
 * K casts), and the monsters' flood and their blows go eight ways. Shots
 * fly four.
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
#define K_F          0x46
#define K_Q          0x51
#define K_E          0x45
#define K_Z          0x5A
#define K_C          0x43
#define K_CAST       0x4B       /* K: C is a diagonal, as in the open world */
#define K_M          0x4D
#define K_X          0x58
#define K_SPACE      0x20
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

/* battle.c's tuning, unchanged: see there. */
#define ARCHER_EYE  (3 + (party_level >> 1))
#define LONE_HP     3
#define LONE_ATK    2
#define WARD        4
#define WIZ_GUARD   ((party_level << 1) / 3)

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
static unsigned char act, ward, sleep;
static unsigned char rel, lone, lone_bank, fury;

static unsigned char foes, alive, cur;
static unsigned char foe_x[MAX_FOES], foe_y[MAX_FOES];
static unsigned char foe_pace[MAX_FOES];
static unsigned int  foe_hp[MAX_FOES];

#define flag_at(x, y) (F_INFO[FI_FLAGS + (unsigned char)((y) * FW + (x))])
#define wall_at(x, y) (wall[(unsigned char)(((y) + 1) * RIM_W + (x) + 1)])

/* --- sprites -------------------------------------------------------------- */

static unsigned char tick, anim;
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
    vs_col[V_CUR] = anim ? C_YELLOW : C_WHITE;
}

/* One frame: what moved is shown, and every sixteenth they shift their
   feet. */
static void frame(void)
{
    if (!(++tick & 15)) {
        anim ^= 1;
        poses();
    }
    mx_plan();
    mx_wait();
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

/* Something flies from square (x,y), `flew` squares the way of `dir`. */
static void shoot(unsigned char x, unsigned char y, unsigned char dir,
                  unsigned char flew, unsigned char kind)
{
    v_put(V_SHOT, x, y);
    vs_ptr[V_SHOT] = kind == SHOT_ARROW ? SPR_ARROW + dir
                                        : SPR_FIRE - 1 + kind;
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
    const Actor *a;

    for (i = 0, x = 0; i < party_count; ++i, x += 10) {
        a = &party[i];
        t_clear(x, ROW_NAMES, 10);
        t_clear(x, ROW_STATS, 10);
        t_at(x, ROW_NAMES, i == act ? C_YELLOW : C_WHITE);
        t_ch(i == act ? 0x3E : 0x20);           /* > */
        t_str(a->name);
        t_at(x + 1, ROW_STATS, a->hp ? C_GREEN : C_RED);
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
        if (party[i].hp && mem_x[i] == x && mem_y[i] == y) {
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
        if (party[hit].hp) {
            return 0;
        }
    }
    return 1;
}

/* --- keys ----------------------------------------------------------------- */

static unsigned char held;      /* frames the stick has been held */

/* The next key, with SHIFTed letters folded onto unshifted ones, or the
   stick in port 2: its directions as the cursor keys and its diagonals as
   Q, E, Z and C, repeating while held, and its button alone as F. Everyone
   goes on idling meanwhile, and the cursor marks whose turn it is. */
static unsigned char next_key(void)
{
    /* By the stick's four direction bits: up 1, down 2, left 4, right 8. */
    static const unsigned char stick[16] = {
        K_F, K_CRSR_UP, K_CRSR_DOWN, K_F,
        K_CRSR_LEFT, K_Q, K_Z, K_CRSR_LEFT,
        K_CRSR_RIGHT, K_E, K_C, K_CRSR_RIGHT,
        K_F, K_CRSR_UP, K_CRSR_DOWN, K_F,
    };
    unsigned char key, j;

    v_put(V_CUR, hx, hy);
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
    return (key >= 0xC1 && key <= 0xDA) ? key - 0x80 : key;
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
        case K_C:                     return DIR_DR;
    }
    return NONE;
}

static unsigned char pick_member(const char *question)
{
    unsigned char i, n = 0, one = 0;

    for (i = 0; i < party_count; ++i) {
        if (party[i].hp) {
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
    if (i < party_count && party[i].hp) {
        return i;
    }
    say("Never mind.");
    return NONE;
}

static unsigned char quaff(void)
{
    unsigned char healed, who;

    if (!potions) {
        say("You have no draughts.");
        return 0;
    }
    who = pick_member("Who drinks?");
    if (who == NONE) {
        return 0;
    }
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
        if (!party[i].hp) {
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
            if (party[i].hp) {
                seed(mem_x[i] + 1, mem_y[i] + 1);
            }
        }
    } else {
        /* A big one stands by its top left square: it is beside someone
           from any of the twelve places round them that it fits. */
        stand = roomy;
        memcpy(seen, roomy, sizeof(seen));
        for (i = 0; i < party_count; ++i) {
            if (party[i].hp) {
                x = mem_x[i] + 1;
                y = mem_y[i] + 1;
                seen_row[y - 1][x - 1] = seen_row[y - 1][x] = 1;
                seen_row[y][x - 1]     = seen_row[y][x]     = 1;
            }
        }
        for (i = 0; i < party_count; ++i) {
            if (party[i].hp) {
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

/* The monster one square nearer, gliding there. */
static void foe_step(void)
{
    unsigned char dir, k;

    if (flood() && member_at(step_x - 1, step_y - 1) == NONE) {
        --step_x;
        --step_y;
        dir = dir_of(step_x > mx ? 1 : step_x < mx ? -1 : 0,
                     step_y > my ? 1 : step_y < my ? -1 : 0);
        mx = foe_x[cur] = step_x;
        my = foe_y[cur] = step_y;
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

static const signed char beside_x[] = { -1, 1,  0, -1, 1, -2, 2, 0 };
static const signed char beside_y[] = {  0, 0,  1,  1, 1,  0, 0, -1 };

static void party_start(void)
{
    unsigned char i, k = 0, x, y;

    mem_x[0] = F_INFO[FI_HX];
    mem_y[0] = F_INFO[FI_HY];
    for (i = 1; i < party_count; ++i) {
        for (; k < sizeof(beside_x); ++k) {
            x = mem_x[0] + beside_x[k];
            y = mem_y[0] + beside_y[k];
            if (x < FW && y < FH && !wall_at(x, y)) {
                break;
            }
        }
        mem_x[i] = x;
        mem_y[i] = y;
        ++k;
    }
    for (i = 0; i < party_count; ++i) {
        guard[i] = 0;
        if (party[i].hp) {
            v_put(i, mem_x[i], mem_y[i]);
        }
        vs_col[i] = fq_tunic[i];
    }
}

/* --- shots ---------------------------------------------------------------- */

static unsigned char target;

/* Something flies from the member whose turn it is: battle.c's loose(). */
static unsigned char loose(unsigned char dir, unsigned char range,
                           unsigned char atk, unsigned char kind,
                           const char *what)
{
    unsigned char x = hx, y = hy, flew = 0, hit = 0;

    while (flew < range) {
        x += dir_dx[dir];
        y += dir_dy[dir];
        if (x >= FW || y >= FH) {
            break;                           /* off the field */
        }
        ++flew;
        if ((target = foe_at(x, y)) != NONE) {
            hit = 1;
            break;
        }
        if (flag_at(x, y) == FF_BLOCK) {
            break;
        }
    }

    shoot(hx, hy, dir, flew, kind);
    strcpy(msg, what);
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
    if (dir > DIR_RIGHT) {              /* NONE, or a diagonal: a shot */
        say(put_down);                  /* flies straight, and the arrow */
        return NONE;                    /* has four pictures */
    }
    return dir;
}

/* The monster's own shot: battle.c's aim_at() and aim(). */
static unsigned char sx, sy, flew, victim;

static unsigned char aim_at(unsigned char tx, unsigned char ty)
{
    unsigned char dir, x, y;

    if ((unsigned char)(tx - mx) < size) {
        sx = tx;
        if (ty < my) {
            sy = my;
            dir = DIR_UP;
        } else {
            sy = my + size - 1;
            dir = DIR_DOWN;
        }
    } else if ((unsigned char)(ty - my) < size) {
        sy = ty;
        if (tx < mx) {
            sx = mx;
            dir = DIR_LEFT;
        } else {
            sx = mx + size - 1;
            dir = DIR_RIGHT;
        }
    } else {
        return NONE;                         /* not in line */
    }

    x = sx;
    y = sy;
    for (flew = 1; flew <= fq_reach; ++flew) {
        x += dir_dx[dir];
        y += dir_dy[dir];
        if (x >= FW || y >= FH) {
            break;
        }
        if ((victim = member_at(x, y)) != NONE) {
            return dir;
        }
        if (flag_at(x, y) == FF_BLOCK || foe_at(x, y) != NONE) {
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
        if (party[i].hp && (dir = aim_at(mem_x[i], mem_y[i])) != NONE) {
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

/* No two fights are fought on the same ground: a few boulders go down
   where the ground is open all round (so nothing is cut off), never on the
   leader's square. On a field less than half the old one's size, two to
   four of them. */
static void scatter(void)
{
    unsigned char n, tries, x, y, q;
    unsigned char *w, *s;

    n = 2 + (unsigned char)(rand() % 3);
    for (tries = 0; tries < 40; ++tries) {
        x = 2 + ((unsigned char)rand() & 15);
        y = 1 + (unsigned char)(rand() % 7);
        w = &wall_at(x, y);
        if ((x != F_INFO[FI_HX] || y != F_INFO[FI_HY]) && open9(w)) {
            *w = 1;
            flag_at(x, y) = FF_BLOCK;
            s = F_SCREEN + (unsigned int)y * 80 + (x << 1);
            for (q = 0; q < 4; ++q) {
                s[(q >> 1) * 40 + (q & 1)] = F_INFO[FI_ROCK + q];
            }
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
    unsigned char s, dir, who, amount;

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
            strcat(msg, " ");
            cat_num(s + 1);
            strcat(msg, " ");
            strcat(msg, spell_names[s]);
        }
    }
    say(msg);
    s = next_key() - 0x31;
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
            dir = which_way("The spell fades.");
            if (dir == NONE) {
                return 0;
            }
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
        case SP_SPARK:
            return loose(dir, 8, 4 + party_level, SHOT_FIRE, "The spark");
        case SP_BOLT:
            return loose(dir, 14, 8 + party_level + (party_level >> 1),
                         SHOT_BOLT, "The bolt");
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
            say("A drowse falls on them.");
            return 0;
        default:                             /* SP_BLAZE */
            hurt_all = 1;
            sfx_play(SFX_BOW);
            strcpy(msg, "Fire takes them all for ");
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
        --alive;
        show_foe();
        foe_sprites(t);
        vanish(fv, fn);
        if (!alive) {
            return 1;
        }
        say("It falls.");
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
        say(sleep ? "They sleep." : "They stir.");
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
                strcpy(msg, foes > 1 ? (const char *)fq_name : (const char *)"It");
                strcat(msg, " hits ");
                strcat(msg, party[i].name);
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
                foe_step();
            }
        }
        foe_pace[cur] = pace;
    }
    return 0;
}

static unsigned char won(void)
{
    unsigned char got;

    say("It falls.");
    get_up();
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

static unsigned char lost(void)
{
    say("The party has fallen.");
    say("Press a key.");
    while (!cbm_k_getin()) {
        frame();
    }
    return BATTLE_DEAD;
}

/* The field's walls, the boulders, and everyone in their places. */
static void set_up(void)
{
    unsigned char x, y, i;
    unsigned int  n, hp;

    size = fq_size == 2 ? 2 : 1;
    memset(wall, 1, sizeof(wall));
    for (y = 0; y < RIM_H; ++y) {
        seen_row[y] = seen + y * RIM_W;
    }
    for (y = 0; y < FH; ++y) {
        for (x = 0; x < FW; ++x) {
            wall_at(x, y) = flag_at(x, y) != FF_OPEN;
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
    say(fq_flee ? "Walk into it to strike. Flee by a side."
                : "Walk into it to strike.");

    for (;;) {
        for (i = 0; i < party_count; ++i) {
            if (!party[i].hp) {
                continue;
            }
            turn_to(i);
            guard[i] = 0;
            if (party[i].mp < party[i].mp_max) {
                ++party[i].mp;
            }
            for (;;) {
                show_party();
                show_foe();

                dmg = 0;
                hurt_all = 0;
                key = next_key();
                dir = key_dir(key);

                switch (key) {
                    case K_F:
                        if (!party[act].bow) {
                            say_who(act, " has no bow.");
                            say(msg);
                            continue;
                        }
                        if (!arrows) {
                            say("You have no arrows.");
                            continue;
                        }
                        dir = which_way("You lower the bow.");
                        if (dir == NONE) {
                            continue;                /* no turn spent */
                        }
                        --arrows;
                        dmg = loose(dir, bow_range[party[act].bow],
                                    bow_atk[party[act].bow]
                                    + (party[act].cls == CL_ARCHER
                                       ? ARCHER_EYE : 0), SHOT_ARROW,
                                    party[act].bow == NUM_BOWS - 1
                                        ? "The bolt" : "The arrow");
                        dir = NONE;                  /* shooting is not walking */
                        break;

                    case K_CAST:
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

                    case K_M:                    /* no turn spent */
                        fq_quiet ^= 1;
                        if (fq_quiet & 1) {
                            ow_tune_stop();
                        } else if (fq_song) {
                            ow_tune_play(fq_song - 1);
                        }
                        say(fq_quiet & 1 ? "Music off." : "Music on.");
                        continue;

                    case K_X:
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
                            for (key = 0; key < 8; ++key) {
                                v_nudge(act, 1, dir, 2);
                                frame();
                            }
                            vs_y[act] = 0;
                            say("You get away.");
                            get_up();
                            pause(40);
                            return BATTLE_FLED;
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
    if (!cbm_load(fq_field, dev, 0) || !cbm_load(fq_sprite, dev, 0)) {
        return FIGHT_NODISK;
    }
    unpack();
    srand(fq_seed);
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
