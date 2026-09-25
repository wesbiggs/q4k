#include <conio.h>
#include <c64.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/arena_data.h>
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

/* On a field the letters are for walking, as they are on the map. */
#define K_W          0x57
#define K_S          0x53
#define K_SPACE      0x20
#define K_CRSR_DOWN  0x11
#define K_CRSR_RIGHT 0x1D
#define K_CRSR_UP    0x91
#define K_CRSR_LEFT  0x9D

typedef struct {
    const char   *name;
    unsigned char hp, atk, def, xp;
    unsigned int  gold;
    unsigned char glyph, color;   /* how it looks on a field */
} Monster;

/* Wandering monsters, weakest first; which one shows up scales with level. */
static const Monster wanderers[] = {
    { "A rat",        4,  2, 0,  2,   2, T_RAT,      TC_RAT      },
    { "A goblin",     7,  3, 1,  4,   6, T_GOBLIN,   TC_GOBLIN   },
    { "A wolf",       9,  4, 1,  6,   5, T_WOLF,     TC_WOLF     },
    { "A bandit",    12,  5, 2,  9,  15, T_BANDIT,   TC_BANDIT   },
    { "A skeleton",  15,  6, 3, 12,  12, T_SKELETON, TC_SKELETON },
    { "An ogre",     22,  8, 3, 18,  25, T_OGRE,     TC_OGRE     },
    { "A wraith",    26, 10, 4, 25,  30, T_WRAITH,   TC_WRAITH   },
    { "A drake",     32, 12, 5, 35,  45, T_DRAKE,    TC_DRAKE    },
};
#define NUM_WANDERERS (sizeof(wanderers) / sizeof(wanderers[0]))

/* One keeper per deep place, in the order the quest opens them. */
static const Monster keepers[NUM_DUNGEONS] = {
    { "The Gaoler",       45, 12, 5,  60,  80, T_MONSTER, TC_MONSTER },
    { "The Drowned King", 60, 15, 6,  90, 120, T_MONSTER, TC_MONSTER },
    { "The Pit Warden",   80, 18, 8, 140, 180, T_MONSTER, TC_MONSTER },
};

static const Monster maldrath = {
    "Maldrath", 120, 22, 10, 255, 0, T_DRAGON, TC_DRAGON
};

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

static char line[SCR_W + 1];   /* ASCII, for text_put */
static char msg[SCR_W + 1];    /* PETSCII, for say()  */

/* Where a fight's running commentary goes: the battle screen's own panel,
   or the map's message log when the fight is out on a field. */
static unsigned char on_field;

static void say(const char *petscii, unsigned char color)
{
    if (on_field) {
        view_message(petscii);
    } else {
        panel_lit(petscii, color);
    }
}

static void cat_num(unsigned int value)
{
    utoa(value, msg + strlen(msg), 10);
}

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
    hero.xp += m->xp;
    gold += m->gold;

    strcpy(msg, "You gain ");
    cat_num(m->xp);
    strcat(msg, " experience and ");
    cat_num(m->gold);
    strcat(msg, " marks.");
    say(msg, COLOR_YELLOW);

    while (hero.level < MAX_LEVEL && hero.xp >= level_at[hero.level]) {
        ++hero.level;
        hero.hp_max += 6;
        hero.hp = hero.hp_max;

        strcpy(msg, "You reach level ");
        cat_num(hero.level);
        strcat(msg, ".");
        say(msg, COLOR_LIGHTGREEN);
    }
}

/* The exchange of blows, shared by both kinds of fight so they cannot
   drift apart. Each returns the damage done and says so. */
static unsigned char strike(const Monster *m, unsigned char m_hp)
{
    unsigned char hit;

    hit = weapon_atk[weapon_lv] + hero.level + (unsigned char)(rand() % 3);
    hit = (hit > m->def) ? hit - m->def : 1;

    strcpy(msg, "You strike for ");
    cat_num(hit);
    strcat(msg, ".");
    say(msg, COLOR_WHITE);
    return hit >= m_hp ? m_hp : hit;
}

/* Returns 1 if that was the end of you. */
static unsigned char bitten(const Monster *m, unsigned char guard)
{
    unsigned char hit;

    hit = m->atk + (unsigned char)(rand() % 3);
    hit = hit > (armour_def[armour_lv] + guard)
          ? hit - armour_def[armour_lv] - guard : 1;

    strcpy(msg, "It hits you for ");
    cat_num(hit);
    strcat(msg, ".");
    say(msg, COLOR_LIGHTRED);

    if (hit >= hero.hp) {
        hero.hp = 0;
        return 1;
    }
    hero.hp -= hit;
    return 0;
}

/* P during a fight. Returns 0 if nothing happened, so no turn is spent. */
static unsigned char quaff(void)
{
    unsigned char healed;

    if (!potions) {
        say("You have no draughts.", COLOR_GRAY2);
        return 0;
    }
    healed = drink_potion();
    if (!healed) {
        say("You are unhurt.", COLOR_GRAY2);
        return 0;
    }
    strcpy(msg, "You drink. ");
    cat_num(healed);
    strcat(msg, " restored.");
    say(msg, COLOR_LIGHTGREEN);
    return 1;
}

static unsigned char won(const Monster *m)
{
    say("It falls.", COLOR_LIGHTGREEN);
    award(m);
    /* On a field the log carries on into the map, so there is nothing
       to hold the screen for -- and a "Press a key." would stay in it. */
    if (!on_field) {
        say("Press a key.", COLOR_GRAY2);
        cgetc();
    }
    return BATTLE_WON;
}

static unsigned char lost(void)
{
    say("You fall.", COLOR_LIGHTRED);
    say("Press a key.", COLOR_GRAY2);
    cgetc();
    return BATTLE_DEAD;
}

/* Three in four: the same odds on a field as on the old screen. */
static unsigned char got_away(void)
{
    if ((rand() % 4) != 0) {
        say("You get away.", COLOR_GRAY1);
        return 1;
    }
    say("It cuts you off.", COLOR_LIGHTRED);
    return 0;
}

/* The keepers, Maldrath, and anything met underground: a screen of its
   own with the blows written out. */
static unsigned char fight(const Monster *m, unsigned char can_flee)
{
    unsigned char m_hp = m->hp;
    unsigned char guard, key;

    on_field = 0;
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

    say("It comes at you.", COLOR_GRAY1);

    for (;;) {
        status(m, m_hp);
        guard = 0;
        key = cgetc();
        if (key >= 0xC1 && key <= 0xDA) {
            key -= 0x80;
        }

        switch (key) {
            case K_ATTACK:
                m_hp -= strike(m, m_hp);
                break;

            case K_DEFEND:
                guard = 4;
                say("You set yourself.", COLOR_GRAY1);
                break;

            case K_POTION:
                if (!quaff()) {
                    continue;
                }
                break;

            case K_RUN:
                if (!can_flee) {
                    say("There is nowhere to run.", COLOR_LIGHTRED);
                    continue;
                }
                if (got_away()) {
                    return BATTLE_FLED;
                }
                break;

            default:
                continue;
        }

        if (m_hp == 0) {
            status(m, 0);
            return won(m);
        }

        /* Its turn. */
        if (bitten(m, guard)) {
            status(m, m_hp);
            return lost();
        }
    }
}

/* --- fighting on a field ------------------------------------------------
 *
 * Out on the overworld a fight happens on a map of the ground you were
 * crossing: you start near the bottom, it starts near the top, and each
 * turn you move and then it moves towards you. Walking into it is how you
 * hit it; it hits you by walking into you. */

#define FIELD_SIZE (LOCAL_W * LOCAL_H)
#define VAL_X      (PANEL_X + 9)

/* The pathfinding works on a copy of the field with a wall all the way
   round it, so stepping off an edge needs no test of its own. */
#define RIM_W      (LOCAL_W + 2)
#define RIM_H      (LOCAL_H + 2)

static unsigned char field[FIELD_SIZE];
static unsigned char wall[RIM_W * RIM_H];    /* 1 where nothing can stand */
static unsigned char seen[RIM_W * RIM_H];
static unsigned char *seen_row[RIM_H];
static unsigned char queue_x[256], queue_y[256];
static unsigned char hx, hy, mx, my;
static const Monster *foe;

/* Draw the ground at (x,y), or the monster if it is standing there. */
static void paint(unsigned char x, unsigned char y)
{
    unsigned int  at = (unsigned int)y * SCR_W + x;
    unsigned char code = field[(unsigned int)y * LOCAL_W + x];

    if (x == mx && y == my) {
        SCREEN_RAM[at] = foe->glyph;
        COLOR_RAM[at]  = foe->color;
    } else {
        SCREEN_RAM[at] = code;
        COLOR_RAM[at]  = tile_colors[code - TILE_BASE];
    }
}

static void draw_field(void)
{
    unsigned char x, y;

    for (y = 0; y < LOCAL_H; ++y) {
        for (x = 0; x < LOCAL_W; ++x) {
            paint(x, y);
        }
    }
}

/* What you are fighting goes where the panel would name the place. */
static void show_foe(const Monster *m, unsigned char m_hp)
{
    char buf[6];

    cclearxy(PANEL_X, 10, PANEL_W);
    cclearxy(PANEL_X, 11, PANEL_W);
    textcolor(m->color);
    cputsxy(PANEL_X, 10, m->name);
    textcolor(COLOR_GRAY1);
    cputsxy(PANEL_X, 11, "HP");
    textcolor(COLOR_WHITE);
    utoa(m_hp, buf, 10);
    cputsxy(VAL_X, 11, buf);
}

static unsigned char next_to(void)
{
    return (hx == mx && (hy == my + 1 || my == hy + 1))
        || (hy == my && (hx == mx + 1 || mx == hx + 1));
}

/* Move the monster one square nearer, the shortest way round whatever is
   in the way.
 *
 * A breadth-first flood out from the hero: the square the flood reaches
 * the monster from is one step nearer than where it stands. If nothing
 * reaches it, it waits where it is.
 *
 * This runs every turn, and written the obvious way -- 16-bit indices into
 * the field, a bounds test per neighbour -- a flood of the whole grass
 * field took 270ms. What it does instead, which brings that to 160ms, and
 * a real turn (the flood stops at the monster) to 35ms on average and 67ms
 * at worst:
 *
 *   - coordinates are one-based inside a rim of wall, so the rim stops the
 *     flood at the edges and there is no bounds test at all;
 *   - a row is found through a table of pointers, so there is no multiply
 *     and no 16-bit index;
 *   - the queue is a 256-entry ring with byte indices. The flood's frontier
 *     on a 21x19 field never comes near that many squares at once.
 *
 * `seen` starts as a copy of the walls, which marks them visited before
 * the flood begins. */
static void foe_step(void)
{
    register unsigned char *row;
    unsigned char head = 0, tail = 1;
    unsigned char x, y, tx, ty;

    memcpy(seen, wall, sizeof(seen));
    tx = mx + 1;
    ty = my + 1;
    queue_x[0] = hx + 1;
    queue_y[0] = hy + 1;
    seen_row[hy + 1][hx + 1] = 1;

    do {
        x = queue_x[head];
        y = queue_y[head];
        ++head;

        row = seen_row[y - 1];
        if (!row[x]) {
            if (x == tx && y - 1 == ty) goto found;
            row[x] = 1;
            queue_x[tail] = x; queue_y[tail] = y - 1; ++tail;
        }
        row = seen_row[y + 1];
        if (!row[x]) {
            if (x == tx && y + 1 == ty) goto found;
            row[x] = 1;
            queue_x[tail] = x; queue_y[tail] = y + 1; ++tail;
        }
        row = seen_row[y];
        if (!row[x - 1]) {
            if (x - 1 == tx && y == ty) goto found;
            row[x - 1] = 1;
            queue_x[tail] = x - 1; queue_y[tail] = y; ++tail;
        }
        if (!row[x + 1]) {
            if (x + 1 == tx && y == ty) goto found;
            row[x + 1] = 1;
            queue_x[tail] = x + 1; queue_y[tail] = y; ++tail;
        }
    } while (head != tail);
    return;

found:
    tx = mx;
    ty = my;
    mx = x - 1;
    my = y - 1;
    paint(tx, ty);
    paint(mx, my);
}

static unsigned char field_fight(const Monster *m, unsigned char ground)
{
    unsigned char m_hp = m->hp;
    unsigned char a, key, guard, dir, nx, ny;
    unsigned char *edge;
    unsigned int  i;

    /* The field for this kind of ground, or the first one if there is
       none -- a bridge, say. */
    for (a = NUM_ARENAS - 1; a && arena_tile[a] != ground; --a) {
    }
    memcpy(field, arena_map[a], FIELD_SIZE);
    memset(wall, 1, sizeof(wall));
    i = 0;
    for (ny = 0; ny < RIM_H; ++ny) {
        seen_row[ny] = seen + (unsigned int)ny * RIM_W;
        if (ny == 0 || ny > LOCAL_H) {
            continue;                        /* the rim stays wall */
        }
        edge = wall + (unsigned int)ny * RIM_W + 1;
        for (nx = 0; nx < LOCAL_W; ++nx, ++i) {
            edge[nx] = tile_flags[field[i] - TILE_BASE] != TF_PASS;
        }
    }
    hx = arena_hx[a];
    hy = arena_hy[a];
    mx = arena_mx[a];
    my = arena_my[a];
    foe = m;

    on_field = 1;
    view_restore();
    draw_field();
    sprite_face(DIR_UP);
    sprite_show(hx, hy);

    strcpy(msg, m->name);
    strcat(msg, " comes at you!");
    say(msg, 0);
    say("Walk into it to strike. R runs.", 0);

    for (;;) {
        view_draw_panel();
        show_foe(m, m_hp);

        guard = 0;
        dir = 0xFF;
        key = cgetc();
        if (key >= 0xC1 && key <= 0xDA) {
            key -= 0x80;
        }

        switch (key) {
            case K_W: case K_CRSR_UP:     dir = DIR_UP;    break;
            case K_S: case K_CRSR_DOWN:   dir = DIR_DOWN;  break;
            case K_ATTACK: case K_CRSR_LEFT:  dir = DIR_LEFT;  break;
            case K_DEFEND: case K_CRSR_RIGHT: dir = DIR_RIGHT; break;

            case K_SPACE:
                guard = 4;
                say("You stand your ground.", 0);
                break;

            case K_POTION:
                if (!quaff()) {
                    continue;
                }
                break;

            case K_RUN:
                if (got_away()) {
                    return BATTLE_FLED;
                }
                break;

            default:
                continue;
        }

        if (dir != 0xFF) {
            nx = hx;
            ny = hy;
            switch (dir) {
                case DIR_UP:   --ny; break;
                case DIR_DOWN: ++ny; break;
                case DIR_LEFT: --nx; break;
                default:       ++nx; break;
            }
            sprite_face(dir);

            if (nx == mx && ny == my) {
                m_hp -= strike(m, m_hp);
                if (m_hp == 0) {
                    mx = my = 0xFF;          /* off the field */
                    paint(nx, ny);
                    show_foe(m, 0);
                    return won(m);
                }
            } else if (nx >= LOCAL_W || ny >= LOCAL_H
                       || wall[(unsigned int)(ny + 1) * RIM_W + nx + 1]) {
                /* Unsigned: stepping off the top or left wraps to 255. */
                continue;                    /* bumping costs no turn */
            } else {
                sprite_walk(dir);
                hx = nx;
                hy = ny;
                sprite_show(hx, hy);
            }
        }

        /* Its turn. */
        if (next_to()) {
            if (bitten(m, guard)) {
                view_draw_panel();
                return lost();
            }
        } else {
            foe_step();
        }
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

    /* Underground there is no field to fight on yet. */
    if (scene == SCENE_WORLD) {
        return field_fight(&wanderers[i], map_at(hero.x, hero.y));
    }
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
