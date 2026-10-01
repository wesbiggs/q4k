#include <conio.h>
#include <c64.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/arena_data.h>
#include <q4k/battle.h>
#include <q4k/game.h>
#include <q4k/music.h>
#include <q4k/rle.h>
#include <q4k/sprite.h>
#include <q4k/tiles.h>
#include <q4k/view.h>

/* The letters are for walking, as they are on the map. */
#define K_W          0x57
#define K_A          0x41
#define K_S          0x53
#define K_D          0x44
#define K_POTION     0x50
#define K_RUN        0x52
#define K_F          0x46
#define K_SPACE      0x20
#define K_CRSR_DOWN  0x11
#define K_CRSR_RIGHT 0x1D
#define K_CRSR_UP    0x91
#define K_CRSR_LEFT  0x9D


/* How each SHOT_* kind lands, by the same index. */
static const char *const shot_hits[] = {
    "An arrow hits you for ",
    "Flame sears you for ",
    "A wave hits you for ",
};

/* Kept in order of `from`: a fight picks evenly from the ones you have
   reached, and battle_wander() stops at the first you have not. */
const Wanderer wanderers[] = {
    { 1, { "A rat",        4,  2, 0,  2,  2, T_RAT,      TC_RAT,      13, 1 } },
    { 1, { "A goblin",     7,  3, 1,  4,  6, T_GOBLIN,   TC_GOBLIN,   10, 1 } },
    { 2, { "A wolf",       9,  4, 1,  6,  5, T_WOLF,     TC_WOLF,     15, 1 } },
    { 2, { "A giant wasp", 5,  2, 0,  4,  0, T_WASP,     TC_WASP,     22, 1 } },
    { 3, { "A cutthroat", 12,  5, 2,  9, 15, T_BANDIT,   COLOR_RED,   10, 1 } },
    { 4, { "A bandit",    12,  5, 2,  9, 15, T_BANDIT,   TC_BANDIT,   10, 1,
           3, 6 } },
    { 4, { "A skeleton",  15,  6, 3, 12, 12, T_SKELETON, TC_SKELETON,  8, 1 } },
    { 5, { "A bone archer",
                          13,  4, 2, 14, 14, T_SKELETON, COLOR_GRAY3,  8, 1,
           5, 8 } },
    { 5, { "An ogre",     22,  8, 3, 18, 25, T_OGRE,     TC_OGRE,      7, 1 } },
    { 6, { "A wraith",    26, 10, 4, 25, 30, T_WRAITH,   TC_WRAITH,   12, 1 } },
    { 7, { "A drake",     32, 12, 5, 35, 45, T_DRAKE,    TC_DRAKE,    17, 1 } },
};
#define NUM_WANDERERS (sizeof(wanderers) / sizeof(wanderers[0]))
const unsigned char num_wanderers = NUM_WANDERERS;

/* One keeper per deep place, in the order the quest opens them. */
const Monster keepers[NUM_DUNGEONS] = {
    { "The Gaoler",       45, 12, 5,  60,  80, T_MONSTER, TC_MONSTER, 10, 1 },
    { "The Drowned King", 60, 15, 6,  90, 120, T_MONSTER, TC_MONSTER, 10, 1,
      12, 4, SHOT_WATER },
    { "The Pit Warden",   80, 18, 8, 140, 180, T_MONSTER, TC_MONSTER, 10, 1 },
};

const Monster maldrath = {
    "Maldrath", 120, 22, 10, 255, 0, T_DRAGONTL, TC_DRAGONTL, 10, 2,
    18, 4, SHOT_FIRE
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

/* A bow hits a little softer than a blade of the same price: what you are
   paying for is not having to be next to it. */
const char *const bow_names[NUM_BOWS] = {
    "No bow", "Short bow", "Longbow", "Crossbow",
};
const unsigned char bow_atk[NUM_BOWS]   = { 0, 3, 6, 10 };
const unsigned char bow_range[NUM_BOWS] = { 0, 6, 10, 15 };
const unsigned int  bow_cost[NUM_BOWS]  = { 0, 80, 150, 350 };

/* Experience needed for each level, from level 1. */
static const unsigned int level_at[] = {
    0, 20, 50, 100, 180, 300, 460, 700, 1000, 1400,
};
#define MAX_LEVEL (sizeof(level_at) / sizeof(level_at[0]))

static char msg[SCR_W + 1];    /* PETSCII, for the message log */

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

/* For `n` of them: a group pays for every one it had. */
static void award(const Monster *m, unsigned char n)
{
    unsigned int xp = m->xp * n, pay = m->gold * n;

    hero.xp += xp;
    gold += pay;

    strcpy(msg, "You gain ");
    cat_num(xp);
    strcat(msg, " experience and ");
    cat_num(pay);
    strcat(msg, " marks.");
    view_message(msg);

    while (hero.level < MAX_LEVEL && hero.xp >= level_at[hero.level]) {
        ++hero.level;
        hero.hp_max += 6;
        hero.hp = hero.hp_max;

        strcpy(msg, "You reach level ");
        cat_num(hero.level);
        strcat(msg, ".");
        view_message(msg);
    }
}

/* The exchange of blows. Each returns the damage done and says so;
   strike()'s line begins with `how`, "You strike for " or a shot's. */
static unsigned char strike(const Monster *m, unsigned char m_hp,
                            unsigned char atk, const char *how)
{
    unsigned char hit;

    hit = atk + hero.level + (unsigned char)(rand() % 3);
    hit = (hit > m->def) ? hit - m->def : 1;

    if (how != msg) {
        strcpy(msg, how);            /* a shot builds its own in msg */
    }
    cat_num(hit);
    strcat(msg, ".");
    view_message(msg);
    return hit >= m_hp ? m_hp : hit;
}

/* Returns 1 if that was the end of you. `how` begins the line, as it does
   for strike(): "It hits you for ", a group's "A rat hits you for " built
   in msg, or a shot's. */
static unsigned char bitten(unsigned char atk, unsigned char guard,
                            const char *how)
{
    unsigned char hit;

    hit = atk + (unsigned char)(rand() % 3);
    hit = hit > (armour_def[armour_lv] + guard)
          ? hit - armour_def[armour_lv] - guard : 1;

    if (how != msg) {
        strcpy(msg, how);
    }
    cat_num(hit);
    strcat(msg, ".");
    view_message(msg);

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
        view_message("You have no draughts.");
        return 0;
    }
    healed = drink_potion();
    if (!healed) {
        view_message("You are unhurt.");
        return 0;
    }
    strcpy(msg, "You drink. ");
    cat_num(healed);
    strcat(msg, " restored.");
    view_message(msg);
    return 1;
}

/* The log carries on into the map afterwards, so a win needs no pause --
   and a "Press a key." would stay in it. Dying moves you, so it does. */
static unsigned char won(const Monster *m, unsigned char n)
{
    view_message("It falls.");
    award(m, n);
    return BATTLE_WON;
}

static unsigned char lost(void)
{
    view_message("You fall.");
    view_message("Press a key.");
    cgetc();
    return BATTLE_DEAD;
}

/* Three in four, as it always was. */
static unsigned char got_away(void)
{
    if ((rand() % 4) != 0) {
        view_message("You get away.");
        return 1;
    }
    view_message("It cuts you off.");
    return 0;
}

/* --- the field ----------------------------------------------------------
 *
 * Every fight happens on a map: the ground you were crossing, a dungeon
 * room, or Maldrath's lair. You start near the bottom, it starts near the
 * top, and each turn you move and then it moves towards you. Walking into
 * it is how you hit it; it hits you by walking into you.
 *
 * A monster can be bigger than one square -- Maldrath is two by two. Its
 * position is its top-left square, and everything below asks "is this
 * square part of it" rather than "is this the square it is on".
 *
 * Wanderers can come in a group of up to MAX_FOES, all of one kind. Each
 * takes its turn in order, as `cur`, with its square copied into (mx,my),
 * so the pathfinding and aiming see one monster at a time and treat the
 * rest of the group as walls. Keepers and Maldrath always come alone,
 * which is what lets a two-by-two body ignore the group entirely. */

#define FIELD_SIZE (LOCAL_W * LOCAL_H)
#define VAL_X      (PANEL_X + 9)

/* The pathfinding works on a copy of the field with a wall all the way
   round it, so stepping off an edge needs no test of its own. */
#define RIM_W      (LOCAL_W + 2)
#define RIM_H      (LOCAL_H + 2)

/* The hero's speed in tenths, before armour takes its share. */
#define HERO_SPEED 10

static unsigned char field[FIELD_SIZE];
static unsigned char wall[RIM_W * RIM_H];    /* 1 where nothing can stand */
static unsigned char roomy[RIM_W * RIM_H];   /* 0 where a 2x2 body fits,
                                                by its top-left square */
static unsigned char seen[RIM_W * RIM_H];
static unsigned char *seen_row[RIM_H];
static unsigned char queue_x[256], queue_y[256];
static unsigned char hx, hy, mx, my;
static unsigned char size;      /* the monster's, squares a side */
static const Monster *foe;

/* The group: where each one stands, the health it has left (0 once it has
   fallen) and its banked moves. */
static unsigned char foes, alive, cur;
static unsigned char foe_x[MAX_FOES], foe_y[MAX_FOES];
static unsigned char foe_hp[MAX_FOES], foe_pace[MAX_FOES];

/* Is (x,y) part of the monster whose turn it is, at (mx,my)? Unsigned, so
   a square above or to the left of it wraps round to a large number and
   fails the same test. foe_at() below asks the same of the whole group. */
static unsigned char on_foe(unsigned char x, unsigned char y)
{
    return (unsigned char)(x - mx) < size && (unsigned char)(y - my) < size;
}

/* Which of the group has any part of it on (x,y), or 0xFF. */
static unsigned char foe_at(unsigned char x, unsigned char y)
{
    unsigned char i;

    for (i = 0; i < foes; ++i) {
        if (foe_hp[i] && (unsigned char)(x - foe_x[i]) < size
                      && (unsigned char)(y - foe_y[i]) < size) {
            return i;
        }
    }
    return 0xFF;
}

/* Draw the ground at (x,y), or the part of a monster standing there. */
static void paint(unsigned char x, unsigned char y)
{
    unsigned int  at = (unsigned int)y * SCR_W + x;
    unsigned char code = field[(unsigned int)y * LOCAL_W + x];
    unsigned char i = foe_at(x, y);

    if (i != 0xFF) {
        SCREEN_RAM[at] = foe->glyph
                       + (unsigned char)((y - foe_y[i]) * size)
                       + (unsigned char)(x - foe_x[i]);
        COLOR_RAM[at]  = foe->color;
    } else {
        SCREEN_RAM[at] = code;
        COLOR_RAM[at]  = tile_colors[code - TILE_BASE];
    }
}

/* Redraw the squares a monster at (x,y) would cover, wherever it is now. */
static void paint_body(unsigned char x, unsigned char y)
{
    unsigned char i, j;

    for (j = 0; j < size; ++j) {
        for (i = 0; i < size; ++i) {
            paint(x + i, y + j);
        }
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

/* What you are fighting goes where the panel would name the place, with
   " x 3" after it while more than one is standing. A group's health is
   listed three to a row, the living only; a lone monster's stays up at 0
   when it falls. The longest wanderer's name, "A bone archer", leaves room
   for the count. */
static void show_foe(void)
{
    char buf[6];
    unsigned char i, x = VAL_X, y = 11;

    cclearxy(PANEL_X, 10, PANEL_W);
    cclearxy(PANEL_X, 11, PANEL_W);
    cclearxy(PANEL_X, 12, PANEL_W);
    textcolor(foe->color);
    cputsxy(PANEL_X, 10, foe->name);
    if (alive > 1) {
        cputs(" x ");
        utoa(alive, buf, 10);
        cputs(buf);
    }
    textcolor(COLOR_GRAY1);
    cputsxy(PANEL_X, 11, "HP");
    textcolor(COLOR_WHITE);
    for (i = 0; i < foes; ++i) {
        if (foe_hp[i] || foes == 1) {
            utoa(foe_hp[i], buf, 10);
            cputsxy(x, y, buf);
            x += 3;
            if (x > VAL_X + 6) {
                x = VAL_X;
                ++y;
            }
        }
    }
}

/* Is the hero beside the monster whose turn it is -- up, down, left or
   right of any part of it? */
static unsigned char next_to(void)
{
    return on_foe(hx, hy - 1) || on_foe(hx, hy + 1)
        || on_foe(hx - 1, hy) || on_foe(hx + 1, hy);
}

/* Start the flood from (x,y), in rim coordinates, if it is open there. */
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

/* Find the monster's way to the hero, the shortest way round whatever is
   in the way. Returns 1 if there is one, and leaves the first step of it
   in (step_x, step_y), in rim coordinates; 0 if the hero cannot be reached
   from where it stands.
 *
 * A breadth-first flood out from the hero: the square the flood reaches
 * the monster from is one step nearer than where it stands.
 *
 * A two-by-two monster is flooded as its top-left square, over `roomy`
 * (where its whole body fits) instead of `wall`. The flood starts from the
 * eight places that would put it beside the hero, with the four that would
 * put it on top of the hero ruled out first.
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
 * the flood begins, and the rest of the group is marked the same way: a
 * monster goes round its fellows rather than through them. */
static unsigned char step_x, step_y;

static unsigned char flood(void)
{
    register unsigned char *row;
    unsigned char head = 0;
    unsigned char x, y, tx, ty, i;

    tail = 0;
    tx = mx + 1;
    ty = my + 1;
    x = hx + 1;
    y = hy + 1;
    if (size == 1) {
        memcpy(seen, wall, sizeof(seen));
        for (i = 0; i < foes; ++i) {
            if (i != cur && foe_hp[i]) {
                seen_row[foe_y[i] + 1][foe_x[i] + 1] = 1;
            }
        }
        seed(x, y);
    } else {
        memcpy(seen, roomy, sizeof(seen));
        seen_row[y - 1][x - 1] = seen_row[y - 1][x] = 1;
        seen_row[y][x - 1]     = seen_row[y][x]     = 1;
        seed(x - 1, y + 1); seed(x, y + 1);
        seed(x - 1, y - 2); seed(x, y - 2);
        seed(x + 1, y - 1); seed(x + 1, y);
        seed(x - 2, y - 1); seed(x - 2, y);
    }
    if (!tail) {
        return 0;
    }

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
    return 0;

found:
    step_x = x;
    step_y = y;
    return 1;
}

/* Move the monster one square nearer. If there is no way through, it
   waits where it is. */
static void foe_step(void)
{
    unsigned char ox = mx, oy = my;

    if (flood()) {
        mx = foe_x[cur] = step_x - 1;
        my = foe_y[cur] = step_y - 1;
        paint_body(ox, oy);
        paint_body(mx, my);
    }
}

/* Where monster `cur` starts: anywhere in the top third of the field that
   its whole body can stand on, that the rest of the group has left free,
   and that has a way through to the hero, so no fight begins with it shut
   in a pocket of trees. The field's own spot from assets/arenas.txt is
   kept for when a few tries find nowhere for the first; for any after it,
   returns 0 and the group is smaller. */
#define START_ROWS  (LOCAL_H / 3)
#define START_TRIES 16

static unsigned char foe_start(unsigned char a)
{
    unsigned char tries;
    const unsigned char *stand = size == 1 ? wall : roomy;

    for (tries = 0; tries < START_TRIES; ++tries) {
        mx = (unsigned char)(rand() % (LOCAL_W - size + 1));
        my = (unsigned char)(rand() % (START_ROWS - size + 1));
        if (!stand[(unsigned int)(my + 1) * RIM_W + mx + 1]
            && foe_at(mx, my) == 0xFF && flood()) {
            return 1;
        }
    }
    if (cur) {
        return 0;
    }
    mx = arena_mx[a];
    my = arena_my[a];
    return 1;
}

/* A movement key as a direction, or 0xFF. */
static unsigned char key_dir(unsigned char key)
{
    switch (key) {
        case K_W: case K_CRSR_UP:     return DIR_UP;
        case K_S: case K_CRSR_DOWN:   return DIR_DOWN;
        case K_A: case K_CRSR_LEFT:   return DIR_LEFT;
        case K_D: case K_CRSR_RIGHT:  return DIR_RIGHT;
    }
    return 0xFF;
}

static unsigned char fold(unsigned char key)
{
    return (key >= 0xC1 && key <= 0xDA) ? key - 0x80 : key;
}

/* Shoot the bow `dir`. The shot flies straight until it meets a monster,
   something solid, the edge of the field or the end of its range. Water
   does not stop it; trees, boulders and walls do. Returns the damage, and
   leaves which one it hit in `target`. */
static unsigned char target;

static unsigned char loose(unsigned char dir)
{
    unsigned char x = hx, y = hy, flew = 0, hit = 0;
    const char *what = bow_lv == NUM_BOWS - 1 ? "Your bolt" : "Your arrow";

    sprite_face(dir);
    while (flew < bow_range[bow_lv]) {
        switch (dir) {
            case DIR_UP:   --y; break;
            case DIR_DOWN: ++y; break;
            case DIR_LEFT: --x; break;
            default:       ++x; break;
        }
        if (x >= LOCAL_W || y >= LOCAL_H) {
            break;                           /* off the field */
        }
        ++flew;
        if ((target = foe_at(x, y)) != 0xFF) {
            hit = 1;
            break;
        }
        if (tile_flags[field[(unsigned int)y * LOCAL_W + x] - TILE_BASE]
            == TF_BLOCK) {
            break;
        }
    }

    sprite_shot(hx, hy, dir, flew, SHOT_ARROW);
    if (hit) {
        sfx_play(SFX_BOW);
        strcpy(msg, what);
        strcat(msg, " hits for ");
        return strike(foe, foe_hp[target], bow_atk[bow_lv], msg);
    }
    strcpy(msg, what);
    strcat(msg, " goes wide.");
    view_message(msg);
    return 0;
}

/* Can the monster shoot the hero from where it stands? The same rules as
   the hero's bow, turned round: straight up, down, left or right, within
   its reach, and nothing solid in between -- the rest of its group
   included. Returns the direction, or 0xFF,
   and leaves where the shot sets off from in (sx,sy) and how far it flies
   in `flew`. A body bigger than a square shoots from whichever part of it
   lines up. */
static unsigned char sx, sy, flew;

static unsigned char aim(void)
{
    unsigned char dir, x, y;

    if (!foe->reach) {
        return 0xFF;
    }
    if ((unsigned char)(hx - mx) < size) {
        sx = hx;
        if (hy < my) {
            sy = my;
            dir = DIR_UP;
        } else {
            sy = my + size - 1;
            dir = DIR_DOWN;
        }
    } else if ((unsigned char)(hy - my) < size) {
        sy = hy;
        if (hx < mx) {
            sx = mx;
            dir = DIR_LEFT;
        } else {
            sx = mx + size - 1;
            dir = DIR_RIGHT;
        }
    } else {
        return 0xFF;                         /* not in line */
    }

    x = sx;
    y = sy;
    for (flew = 1; flew <= foe->reach; ++flew) {
        switch (dir) {
            case DIR_UP:   --y; break;
            case DIR_DOWN: ++y; break;
            case DIR_LEFT: --x; break;
            default:       ++x; break;
        }
        if (x == hx && y == hy) {
            return dir;
        }
        if (tile_flags[field[(unsigned int)y * LOCAL_W + x] - TILE_BASE]
            == TF_BLOCK || foe_at(x, y) != 0xFF) {
            break;
        }
    }
    return 0xFF;                             /* out of reach, or blocked */
}

unsigned char battle_fight(const Monster *m, unsigned char ground,
                           unsigned char can_flee, unsigned char count)
{
    unsigned char a, key, guard, dir, nx, ny, dmg;
    unsigned char rel, pace;
    unsigned char *edge;
    unsigned int  i;

    /* The field for this kind of ground, or the first one if there is
       none. */
    for (a = NUM_ARENAS - 1; a && arena_tile[a] != ground; --a) {
    }
    rle_unpack(field, arena_map[a], FIELD_SIZE);
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
    size = m->size;
    if (size == 2) {
        memset(roomy, 1, sizeof(roomy));
        for (i = 0; i < sizeof(roomy) - RIM_W - 1; ++i) {
            roomy[i] = wall[i] | wall[i + 1]
                     | wall[i + RIM_W] | wall[i + RIM_W + 1];
        }
    }
    hx = arena_hx[a];
    hy = arena_hy[a];
    foe = m;

    /* Placed one at a time, each clear of the ones before it. */
    memset(foe_hp, 0, sizeof(foe_hp));
    memset(foe_pace, 0, sizeof(foe_pace));
    foes = count;
    for (cur = 0; cur < count && foe_start(a); ++cur) {
        foe_x[cur] = mx;
        foe_y[cur] = my;
        foe_hp[cur] = m->hp;
    }
    foes = alive = cur;

    /* Speed is relative: armour slows you a tenth a grade, from 1.0 in
       travel clothes to 0.6 in plate, and whatever you lose it gains.
       Its moves are banked in tenths, so a relative 1.5 moves once, then
       twice, then once. */
    rel = m->speed + 10 - (HERO_SPEED - armour_lv);

    view_restore();
    draw_field();
    sprite_face(DIR_UP);
    sprite_show(hx, hy);

    strcpy(msg, m->name);
    strcat(msg, " comes at you!");
    view_message(msg);
    if (foes > 1) {
        strcpy(msg, "And ");
        cat_num(foes - 1);
        strcat(msg, " more!");
        view_message(msg);
    }
    view_message(can_flee ? "Walk into it to strike. R runs."
                          : "Walk into it to strike.");

    for (;;) {
        view_draw_panel();
        show_foe();

        guard = 0;
        dmg = 0;
        key = fold(cgetc());
        dir = key_dir(key);

        switch (key) {
            case K_F:
                if (!bow_lv) {
                    view_message("You have no bow.");
                    continue;
                }
                view_message("Shoot which way?");
                dir = key_dir(fold(cgetc()));
                if (dir == 0xFF) {
                    view_message("You lower the bow.");
                    continue;                /* no turn spent */
                }
                dmg = loose(dir);
                dir = 0xFF;                  /* shooting is not walking */
                break;

            case K_SPACE:
                guard = 4;
                view_message("You stand your ground.");
                break;

            case K_POTION:
                if (!quaff()) {
                    continue;
                }
                break;

            case K_RUN:
                if (!can_flee) {
                    view_message("There is nowhere to run.");
                    continue;
                }
                if (got_away()) {
                    return BATTLE_FLED;
                }
                break;

            default:
                if (dir == 0xFF) {
                    continue;
                }
                break;
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

            if ((target = foe_at(nx, ny)) != 0xFF) {
                sfx_play(SFX_MELEE);
                dmg = strike(m, foe_hp[target], weapon_atk[weapon_lv],
                             "You strike for ");
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

        /* strike() never does less than 1, so a blow that landed is
           never 0; a shot that went wide is. */
        if (dmg) {
            foe_hp[target] -= dmg;
            if (!foe_hp[target]) {
                paint_body(foe_x[target], foe_y[target]);   /* ground now */
                if (!--alive) {
                    show_foe();
                    return won(m, foes);
                }
                view_message("It falls.");
            }
        }

        /* Their turn, one after another: as many moves each as it has
           banked whole ones. Beside you it strikes; in line and in reach it
           shoots (or breathes), and that is the rest of its turn -- a fast
           archer nocks one arrow at a time. */
        for (cur = 0; cur < foes; ++cur) {
            if (!foe_hp[cur]) {
                continue;
            }
            mx = foe_x[cur];
            my = foe_y[cur];
            pace = foe_pace[cur] + rel;
            for (; pace >= 10; pace -= 10) {
                if (next_to()) {
                    /* Alone it is "it"; in a group, say which. */
                    if (foes > 1) {
                        strcpy(msg, m->name);
                        strcat(msg, " hits you for ");
                    }
                    sfx_play(SFX_MELEE);
                    if (bitten(m->atk, guard,
                               foes > 1 ? msg : "It hits you for ")) {
                        view_draw_panel();
                        return lost();
                    }
                } else if ((dir = aim()) != 0xFF) {
                    sprite_shot(sx, sy, dir, flew, m->kind);
                    sfx_play(SFX_BOW);
                    if (bitten(m->shot, guard, shot_hits[m->kind])) {
                        view_draw_panel();
                        return lost();
                    }
                    pace %= 10;
                    break;
                } else {
                    foe_step();
                }
            }
            foe_pace[cur] = pace;
        }
    }
}

unsigned char battle_wander(void)
{
    unsigned char top, i, most;

    /* The rat and the goblin are there from level 1, so `top` is never 0. */
    for (top = 0; top < NUM_WANDERERS && wanderers[top].from <= hero.level;
         ++top) {
    }
    i = (unsigned char)(rand() % top);

    /* As many as the hero has levels for each one's worth: a level-6 hero
       can meet up to six rats (from level 1) or three wasps (from 2). */
    most = hero.level / wanderers[i].from;
    if (most > MAX_FOES) {
        most = MAX_FOES;
    }

    /* Above ground, the ground you were on; below it, the one room. */
    return battle_fight(&wanderers[i].m,
                       scene == SCENE_WORLD ? map_at(hero.x, hero.y)
                                            : T_FLOOR,
                       1, (unsigned char)(1 + rand() % most));
}

unsigned char battle_boss(unsigned char dungeon)
{
    return battle_fight(&keepers[dungeon], T_FLOOR, 0, 1);
}

unsigned char battle_dragon(void)
{
    return battle_fight(&maldrath, T_LAIR, 0, 1);
}
