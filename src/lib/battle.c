#include <conio.h>
#include <c64.h>
#include <stdlib.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/game.h>
#include <q4k/panel.h>
#include <q4k/sprite.h>
#include <q4k/text.h>
#include <q4k/tiles.h>
#include <q4k/view.h>

#define LOG_Y      3
#define LOG_H     13
#define RULE_A     1
#define RULE_B    17
#define STAT_Y    18
#define CMD_Y     20

#define K_ATTACK  0x41
#define K_DEFEND  0x44
#define K_POTION  0x50
#define K_RUN     0x52

typedef struct {
    const char   *name;
    unsigned char hp, atk, def, xp;
    unsigned int  gold;
} Monster;

/* Wandering monsters, weakest first; which one shows up scales with level. */
static const Monster wanderers[] = {
    { "A rat",        4,  2, 0,  2,   2 },
    { "A goblin",     7,  3, 1,  4,   6 },
    { "A wolf",       9,  4, 1,  6,   5 },
    { "A bandit",    12,  5, 2,  9,  15 },
    { "A skeleton",  15,  6, 3, 12,  12 },
    { "An ogre",     22,  8, 3, 18,  25 },
    { "A wraith",    26, 10, 4, 25,  30 },
    { "A drake",     32, 12, 5, 35,  45 },
};
#define NUM_WANDERERS (sizeof(wanderers) / sizeof(wanderers[0]))

/* One keeper per deep place, in the order the quest opens them. */
static const Monster keepers[NUM_DUNGEONS] = {
    { "The Gaoler",       45, 12, 5,  60,  80 },
    { "The Drowned King", 60, 15, 6,  90, 120 },
    { "The Pit Warden",   80, 18, 8, 140, 180 },
};

static const Monster maldrath = { "Maldrath", 120, 22, 10, 255, 0 };

const char *const weapon_names[NUM_WEAPONS] = {
    "Bare hands", "Dagger", "Short sword", "Long sword", "War axe",
};
const unsigned char weapon_atk[NUM_WEAPONS]  = { 1, 3, 6, 10, 15 };
const unsigned int  weapon_cost[NUM_WEAPONS] = { 0, 25, 80, 200, 420 };

const char *const armour_names[NUM_ARMOUR] = {
    "Travel clothes", "Leather", "Chain mail", "Scale mail", "Plate",
};
const unsigned char armour_def[NUM_ARMOUR]  = { 0, 2, 4, 7, 10 };
const unsigned int  armour_cost[NUM_ARMOUR] = { 0, 30, 110, 260, 500 };

/* Experience needed for each level, from level 1. */
static const unsigned int level_at[] = {
    0, 20, 50, 100, 180, 300, 460, 700, 1000, 1400,
};
#define MAX_LEVEL (sizeof(level_at) / sizeof(level_at[0]))

static char line[SCR_W + 1];

/* CIA 1's timer A free-runs off the system clock; by the time a person has
   pressed a key it is unpredictable. */
#define CIA1_TIMER (*(volatile unsigned int *)0xDC04)

unsigned int last_seed;
static unsigned int entropy;

void rng_stir(unsigned int spins)
{
    /* Mix rather than replace: a slow decision and a fast one should both
       leave a mark, and the timer catches what the spin count is too coarse
       to see. */
    entropy = (entropy << 3) ^ (entropy >> 13) ^ spins ^ CIA1_TIMER;
}

void reseed(void)
{
    last_seed = entropy;
    srand(last_seed);
}

#define POTION_SHARE 40   /* per cent of full health */
#define POTION_FLOOR 12   /* ...but never less than the old flat amount */

unsigned char potion_heal(void)
{
    unsigned int amount = ((unsigned int)hero.hp_max * POTION_SHARE) / 100;

    return (unsigned char)(amount < POTION_FLOOR ? POTION_FLOOR : amount);
}

unsigned char drink_potion(void)
{
    unsigned char amount;

    if (!potions || hero.hp == hero.hp_max) {
        return 0;
    }
    --potions;
    amount = potion_heal();
    if (hero.hp_max - hero.hp < amount) {
        amount = hero.hp_max - hero.hp;
    }
    hero.hp += amount;
    return amount;
}

void heal_full(void)
{
    hero.hp = hero.hp_max;
}

unsigned char encounter_roll(unsigned char tile)
{
    unsigned char chance;

    /* Open ground is safer than the places nothing watches. */
    switch (tile) {
        case T_GRASS:  chance = 8;  break;
        case T_SHORE:  chance = 7;  break;
        case T_FOREST: chance = 12; break;
        case T_SWAMP:  chance = 16; break;
        case T_FLOOR:  chance = 10; break;   /* dungeon corridors */
        default:       chance = 0;  break;
    }
    return chance && (unsigned char)(rand() % 100) < chance;
}

static void status(const Monster *m, unsigned char m_hp)
{
    char *p;

    p = str_lit(line, "You ");
    p = str_num(p, hero.hp);
    p = str_lit(p, "/");
    p = str_num(p, hero.hp_max);
    p = str_pad(p, 20, line);
    p = str_lit(p, m->name);
    p = str_lit(p, " ");
    p = str_num(p, m_hp);

    text_clear(0, STAT_Y, SCR_W);
    text_put(0, STAT_Y, line, m_hp ? COLOR_WHITE : COLOR_GRAY2);
}

static void award(const Monster *m)
{
    char *p;

    hero.xp += m->xp;
    gold += m->gold;

    p = str_lit(line, "You gain ");
    p = str_num(p, m->xp);
    p = str_lit(p, " experience and ");
    p = str_num(p, m->gold);
    p = str_lit(p, " marks.");
    panel_line(line, COLOR_YELLOW);

    while (hero.level < MAX_LEVEL && hero.xp >= level_at[hero.level]) {
        ++hero.level;
        hero.hp_max += 6;
        hero.hp = hero.hp_max;

        p = str_lit(line, "You reach level ");
        p = str_num(p, hero.level);
        p = str_lit(p, ".");
        panel_line(line, COLOR_LIGHTGREEN);
    }
}

static unsigned char fight(const Monster *m, unsigned char can_flee)
{
    unsigned char m_hp = m->hp;
    unsigned char guard, hit, key;
    char *p;

    sprite_hide();
    clrscr();
    panel_begin(LOG_Y, LOG_H);

    textcolor(COLOR_LIGHTRED);
    cputsxy(0, 0, m->name);
    textcolor(COLOR_GRAY1);
    chlinexy(0, RULE_A, SCR_W);
    chlinexy(0, RULE_B, SCR_W);

    str_lit(line, can_flee ? "A)ttack  D)efend  P)otion  R)un"
                           : "A)ttack  D)efend  P)otion");
    text_put(0, CMD_Y, line, COLOR_GRAY2);

    panel_lit("It comes at you.", COLOR_GRAY1);

    for (;;) {
        status(m, m_hp);
        guard = 0;
        key = cgetc();
        if (key >= 0xC1 && key <= 0xDA) {
            key -= 0x80;
        }

        switch (key) {
            case K_ATTACK:
                hit = weapon_atk[weapon_lv] + hero.level
                      + (unsigned char)(rand() % 3);
                hit = (hit > m->def) ? hit - m->def : 1;
                if (hit >= m_hp) {
                    m_hp = 0;
                } else {
                    m_hp -= hit;
                }
                p = str_lit(line, "You strike for ");
                p = str_num(p, hit);
                p = str_lit(p, ".");
                panel_line(line, COLOR_WHITE);
                break;

            case K_DEFEND:
                guard = 4;
                panel_lit("You set yourself.", COLOR_GRAY1);
                break;

            case K_POTION:
                if (!potions) {
                    panel_lit("You have no draughts.", COLOR_GRAY2);
                    continue;
                }
                hit = drink_potion();
                if (!hit) {
                    panel_lit("You are unhurt.", COLOR_GRAY2);
                    continue;
                }
                p = str_lit(line, "You drink. ");
                p = str_num(p, hit);
                p = str_lit(p, " restored.");
                panel_line(line, COLOR_LIGHTGREEN);
                break;

            case K_RUN:
                if (!can_flee) {
                    panel_lit("There is nowhere to run.", COLOR_LIGHTRED);
                    continue;
                }
                if ((rand() % 4) != 0) {
                    panel_lit("You get away.", COLOR_GRAY1);
                    return BATTLE_FLED;
                }
                panel_lit("It cuts you off.", COLOR_LIGHTRED);
                break;

            default:
                continue;
        }

        if (m_hp == 0) {
            status(m, 0);
            panel_lit("It falls.", COLOR_LIGHTGREEN);
            award(m);
            panel_lit("Press a key.", COLOR_GRAY2);
            cgetc();
            return BATTLE_WON;
        }

        /* Its turn. */
        hit = m->atk + (unsigned char)(rand() % 3);
        hit = hit > (armour_def[armour_lv] + guard)
              ? hit - armour_def[armour_lv] - guard : 1;

        p = str_lit(line, "It hits you for ");
        p = str_num(p, hit);
        p = str_lit(p, ".");
        panel_line(line, COLOR_LIGHTRED);

        if (hit >= hero.hp) {
            hero.hp = 0;
            status(m, m_hp);
            panel_lit("You fall.", COLOR_LIGHTRED);
            panel_lit("Press a key.", COLOR_GRAY2);
            cgetc();
            return BATTLE_DEAD;
        }
        hero.hp -= hit;
    }
}

unsigned char battle_wander(void)
{
    unsigned char top = hero.level + 1;
    unsigned char i;

    if (top > NUM_WANDERERS) {
        top = NUM_WANDERERS;
    }
    i = (unsigned char)(rand() % top);
    return fight(&wanderers[i], 1);
}

unsigned char battle_boss(unsigned char dungeon)
{
    return fight(&keepers[dungeon], 0);
}

unsigned char battle_dragon(void)
{
    return fight(&maldrath, 0);
}
