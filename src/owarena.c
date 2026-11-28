/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * The multicolour arena: pick a fight with anything, on any ground, and
 * have it fought by the fight module (include/q4k/fight.h, src/fight/) --
 * a field of multicolour metatiles, with the party, the monsters and the
 * shots all sprites.
 *
 * This program is only the menu, and the example of how a fight is called:
 * it loads the module once (FIGHT, at $5400), and for each fight fills in
 * the block at its head -- the party, the monster's numbers out of the
 * game's own bestiary, which field (f.*) and whose sprites (s.<n>) -- and
 * calls it. The module loads those two files itself, takes the screen, and
 * gives it back with the result. Everything is on dist/owarena.d64.
 *
 *   a-o      fight that monster
 *   1-6      the ground: wanderers anywhere, keepers always in a dungeon
 *            room and Maldrath in his lair
 *   +        how many wanderers come at once, 1 to MAX_FOES
 *   -        how many of the party: a warrior, then an archer, a wizard
 *   F1       the party's level, 1-10
 *   F3/F5/F7 weapon, armour, bow: each has the best of that or less that
 *            their class may carry
 *
 * In the fight: WASD or the cursor keys, and Q E Z X diagonally (or a
 * stick in port 2), walk and strike, the up arrow shoots, * casts, P
 * drinks, SPACE stands firm, SHIFT+M turns the effects off and on.
 *
 *   make run PROG=owarena
 *
 * For the tests, poked: auto_seed makes the next fight's dice the same
 * every time, and auto_hp starts everyone on that much health.
 *
 *   *        play by itself, for tools/balance.py: the fight poked
 *            (auto_mon, level, group, ground, and a party of auto_n with
 *            classes and arms auto_cls/w/a/b), auto_runs times, with the
 *            module's autoplayer (FIGHTBOT: the module built with
 *            FIGHT_BOT), counting what happened in res_*, then probe
 *            stage 5 for good.
 */

#include <cbm.h>
#include <conio.h>
#include <c64.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/fight.h>
#include <q4k/game.h>
#include <q4k/probe.h>
#include <q4k/tiles.h>

#define K_A    0x41     /* PETSCII a, unshifted */
#define K_1    0x31
#define K_PLUS 0x2B
#define K_MINUS 0x2D
#define K_F1   0x85
#define K_F3   0x86
#define K_F5   0x87
#define K_F7   0x88

#define MAX_LEVEL 10
#define LIST_Y    2
#define NUM_GROUNDS 6
#define G_LAIR    4
#define G_FLOOR   5

/* What arms.c and actor.c want of the game, which is not linked. */
unsigned int  gold;
unsigned char potions, arrows;
unsigned int  game_turn;

unsigned int  auto_seed;
unsigned char auto_hp;

/* The autoplayer's fight (all poked by tools/balance.py --ow). */
unsigned char auto_mon, auto_runs = 20, auto_n = 1, auto_draughts = 10;
unsigned char auto_cls[MAX_MADE] = { CL_WARRIOR, CL_ARCHER, CL_WIZARD };
unsigned char auto_w[MAX_MADE], auto_a[MAX_MADE], auto_b[MAX_MADE];
unsigned int  res_won, res_lost, res_stall, res_draughts, res_rounds;

#define FIGHT_BOT_FILE "fightbot"
#define STAGE_DONE     5

unsigned char level = 1, ground, group = 1;
static unsigned char members = 3;
static unsigned char tier_w = WPN_SHORT, tier_a = 2, tier_b = BOW_SHORT;
static unsigned char count;      /* monsters on the menu */
static unsigned char loaded;     /* which module is in memory: 1 the
                                    fight, 2 the autoplayer's */
static unsigned char fought;

static const char *const ground_names[NUM_GROUNDS] = {
    "grass", "forest", "shore", "swamp", "lair", "floor",
};
static const char *const made_names[MAX_MADE] = { "Alaric", "Mira", "Zed" };
static const unsigned char book_level[NUM_SPELLS] = { 1, 1, 2, 3, 5, 7 };
static const unsigned char book_pick[NUM_SPELLS] = {
    SP_SPARK, SP_HEAL, SP_BOLT, SP_BLAZE, SP_WARD, SP_SLEEP,
};

/* Menu entry n: the wanderers in order, then the keepers, then Maldrath. */
static const Monster *entry(unsigned char n)
{
    if (n < num_wanderers) {
        return &wanderers[n].m;
    }
    n -= num_wanderers;
    return n < NUM_DUNGEONS ? &keepers[n] : &maldrath;
}

/* Whose sprites a monster is drawn with, by the glyph the game draws it
   as: the number of their file, s.<n> (FIGHT_SPR_*, as ow_fight.c's
   sprite_of()); assets/monsters.txt has one for each. */
static unsigned char sprite_of(const Monster *m)
{
    unsigned char g = m->glyph;

    if (g >= T_RAT && g <= T_DRAGONTL) {
        return g - T_RAT + FIGHT_SPR_BASE;
    }
    return g == T_WASP ? FIGHT_SPR_WASP : FIGHT_SPR_KEEPER;
}

/* A member's colours from their name, as the game's sprite.c's tunic_of()
   picks the clothes: the same name always gets the same ones, clothes never
   black or white, and skin light red or brown, even odds, and never the
   clothes' (src/lib/ow_party.c's, which this cannot link). The high four
   bits are the skin, the low the clothes (OWG_SKIN, OWG_DRESS). The party
   screen changes them. */
static unsigned char tunic(const char *name)
{
    unsigned char h = 0, c, k;

    while (*name) {
        h = (unsigned char)((h << 1) | (h >> 7)) ^ *name++;
    }
    c = (unsigned char)(2 + h % 14);
    k = h & 0x10 ? COLOR_BROWN : COLOR_LIGHTRED;
    if (c == k) {
        ++c;                    /* (9 or 10: 10 or 11) */
    }
    return (unsigned char)((k << 4) | c);
}

/* The best of `kind` up to `tier` that member `who` may carry. */
static unsigned char best(unsigned char who, unsigned char kind,
                          unsigned char tier)
{
    for (; tier && !gear_allowed(who, kind, tier); --tier) {
    }
    return tier;
}

/* The party as the menu has it: this many, this level, full health, armed
   to the tiers chosen; a wizard knows every book the level allows and has
   prepared as many as it has room for. */
static void kit_out(void)
{
    unsigned char i, k, n;
    Actor *a;

    party_count = 0;
    party_level = level;
    for (i = 0; i < members; ++i) {
        party_make(i, made_names[i], i);
        a = &party[i];
        a->weapon = best(i, GEAR_WEAPON, tier_w);
        a->armour = best(i, GEAR_ARMOUR, tier_a);
        a->bow    = best(i, GEAR_BOW, tier_b);
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
    if (auto_hp) {
        for (i = 0; i < members; ++i) {
            party[i].hp = auto_hp;
        }
    }
    potions = 3;
    arrows  = ARROW_MAX;
}

static void row(unsigned char y, const char *label, const char *value)
{
    textcolor(COLOR_GRAY1);
    cputsxy(20, y, label);
    textcolor(COLOR_WHITE);
    cputs(value);
}

static void draw_menu(void)
{
    const Monster *m;
    unsigned char n;
    char buf[4];

    textcolor(COLOR_YELLOW);
    cputsxy(0, 0, "THE MULTICOLOUR ARENA");

    for (n = 0; n < count; ++n) {
        m = entry(n);
        textcolor(COLOR_GRAY1);
        cputcxy(0, LIST_Y + n, K_A + n);
        textcolor(m->color);
        cputsxy(2, LIST_Y + n, m->name);
    }

    for (n = 2; n < 12; ++n) {
        cclearxy(20, n, 20);
    }
    row(2, "1-6 on ", ground_names[ground]);
    utoa(group, buf, 10);
    row(3, "+   foes  ", buf);
    utoa(members, buf, 10);
    row(4, "-   party ", buf);
    utoa(level, buf, 10);
    row(5, "F1  level ", buf);
    row(7, "F3 ", weapon_names[tier_w]);
    row(8, "F5 ", armour_names[tier_a]);
    row(9, "F7 ", bow_names[tier_b]);
    for (n = 0; n < members; ++n) {
        textcolor(COLOR_GRAY1);
        cputsxy(20, 11 + n, party[n].name);
        cputc(' ');
        cputs(class_names[party[n].cls]);
    }
}

/* The bottom rows: what the last fight came to. */
static void say(const char *s)
{
    cclearxy(0, 22, 40);
    textcolor(COLOR_WHITE);
    cputsxy(0, 22, s);
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

static unsigned char device(void)
{
    unsigned char dev = *(unsigned char *)0xBA;

    return dev < 8 ? 8 : dev;
}

/* Fight menu entry n with the party as it is, and take back what the
   fight left of it. */
static unsigned char fight_with(unsigned char n)
{
    const Monster *m = entry(n);
    Fight *f = FIGHT;
    unsigned char i, g, r;

    /* Wanderers on the ground chosen; keepers in a dungeon room, Maldrath
       in his lair, as in the game. */
    g = n < num_wanderers ? ground
      : n < num_wanderers + NUM_DUNGEONS ? G_FLOOR : G_LAIR;

    memcpy(f->party, party, sizeof(party));
    f->party_count = party_count;
    f->party_level = party_level;
    f->potions     = potions;
    f->arrows      = arrows;
    for (i = 0; i < party_count; ++i) {
        f->tunic[i] = tunic(party[i].name);
    }
    strcpy(f->name, strlen(m->name) < FIGHT_NAME ? m->name
                    : m->name + 4);     /* without its "The " (ow_fight.c) */
    f->hp     = m->hp;
    f->atk    = m->atk;
    f->def    = m->def;
    f->colour = m->color;
    f->speed  = m->speed;
    f->size   = m->size;
    f->shot   = m->shot;
    f->reach  = m->reach;
    f->kind   = m->kind;
    f->count  = n < num_wanderers ? group : 1;
    f->flee   = n < num_wanderers;
    f->seed   = auto_seed ? auto_seed : rand();
    f->song   = 0;              /* no tune is loaded here */
    f->quiet  = 0;
    strcpy(f->field, "f.");
    strcat(f->field, ground_names[g]);
    strcpy(f->sprite, "s.");
    utoa(sprite_of(m), f->sprite + 2, 10);

    r = fight_call();

    memcpy(party, f->party, sizeof(party));
    potions = f->potions;
    arrows  = f->arrows;
    return r;
}

static void fight(unsigned char n)
{
    const Monster *m = entry(n);
    unsigned char i, r;
    char buf[6];

    if (loaded != 1) {
        say("Loading the fight ...");
        if (!cbm_load(FIGHT_FILE, device(), 0)) {
            say("No FIGHT on the disk in the drive.");
            loaded = 0;
            return;
        }
        loaded = 1;
    }
    say("To arms ...");
    r = fight_with(n);

    /* What came of it; the pay is the caller's to give. */
    cclearxy(0, 23, 40);
    switch (r) {
        case BATTLE_WON:
            say("Won: ");
            utoa(m->xp * FIGHT->foes, buf, 10);
            cputs(buf);
            cputs(" experience, ");
            utoa(m->gold * FIGHT->foes, buf, 10);
            cputs(buf);
            cputs(" marks.");
            break;
        case BATTLE_FLED:
            say("Fled.");
            break;
        case BATTLE_DEAD:
            say("The party fell.");
            break;
        default:
            say("Put the arena's disk back.");
            break;
    }
    gotoxy(0, 23);
    for (i = 0; i < party_count; ++i) {
        cputs(party[i].name);
        cputc(' ');
        utoa(party[i].hp, buf, 10);
        cputs(buf);
        cputs("  ");
    }
    fought = 1;
}

/* The poked party: auto_n of them, classes and arms as poked, the rest as
   kit_out() makes them. */
static void auto_kit(void)
{
    unsigned char i, k, n;
    Actor *a;

    party_count = 0;
    party_level = level;
    for (i = 0; i < auto_n; ++i) {
        party_make(i, made_names[i], auto_cls[i]);
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
}

static void autoplay(void)
{
    unsigned char r, run;

    say("Loading the autoplayer ...");
    if (!cbm_load(FIGHT_BOT_FILE, device(), 0)) {
        say("No FIGHTBOT on the disk in the drive.");
        loaded = 0;
        return;
    }
    loaded = 2;
    for (run = 0; run < auto_runs; ++run) {
        auto_kit();
        r = fight_with(auto_mon);
        if (r == BATTLE_WON) {
            ++res_won;
        } else if (r == BATTLE_DEAD) {
            ++res_lost;
        } else {
            ++res_stall;
        }
        res_draughts += auto_draughts - potions;
        res_rounds   += FIGHT->rounds;
    }
    probe_stage(STAGE_DONE);
    for (;;) {
    }
}

int main(void)
{
    unsigned char k;

    count = num_wanderers + NUM_DUNGEONS + 1;
    bgcolor(COLOR_BLACK);
    bordercolor(COLOR_BLACK);
    clrscr();
    VIC.addr = 0x17;    /* the lowercase set: conio writes for it */

    while (kbhit()) {   /* the launcher's RUN and RETURN are not input */
        cgetc();
    }

    textcolor(COLOR_GRAY1);
    cputsxy(0, 19, "A letter fights. In the fight: WASD or");
    cputsxy(0, 20, "the cursor keys, QEZX diagonally, ^");
    cputsxy(0, 21, "shoots, * casts, P drinks, SPACE stands");
    cputsxy(0, 22, "firm.");

#ifdef OWARENA_AUTO
    /* Straight into a fight, for a screenshot from VICE itself, which only
       takes one on its way out (-limitcycles, -exitscreenshot) and cannot
       be typed at: -DOWARENA_AUTO=7 is six bone archers on the shore. */
    group = MAX_FOES;
    ground = 2;
    kit_out();
    fight(OWARENA_AUTO);
#endif

    for (;;) {
        kit_out();
        draw_menu();
        probe_stage(fought ? 4 : 1);   /* 4: back from a fight */

        k = key();
        if (k >= 0xC1 && k <= 0xDA) {
            k -= 0x80;               /* fold SHIFTed letters onto unshifted */
        }

        if (k == 0x2A) {             /* * */
            autoplay();
        } else if (k >= K_A && k < K_A + count) {
            fight(k - K_A);
        } else if (k >= K_1 && k < K_1 + NUM_GROUNDS) {
            ground = k - K_1;
        } else if (k == K_PLUS) {
            group = group % MAX_FOES + 1;
        } else if (k == K_MINUS) {
            members = members % MAX_MADE + 1;
        } else if (k == K_F1) {
            level = level % MAX_LEVEL + 1;
        } else if (k == K_F3) {
            tier_w = (tier_w + 1) % NUM_WEAPONS;
        } else if (k == K_F5) {
            tier_a = (tier_a + 1) % NUM_ARMOUR;
        } else if (k == K_F7) {
            tier_b = (tier_b + 1) % NUM_BOWS;
        }
    }
    /* not reached; the loop is the program */
}
