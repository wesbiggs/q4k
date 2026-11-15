#include <conio.h>
#include <c64.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/arena_data.h>
#include <q4k/battle.h>
#include <q4k/game.h>
#include <q4k/isle.h>
#include <q4k/loot.h>
#include <q4k/music.h>
#include <q4k/rle.h>
#include <q4k/shop.h>
#include <q4k/sprite.h>
#include <q4k/engine.h>
#include <q4k/stick.h>
#include <q4k/tiles.h>
#include <q4k/view.h>

/* The fight is an engine of its own here (engine.h), and the map calls it
   through fightcall.c, which has these names; so do they, inside it. */
#define battle_wander  fight_wander
#define battle_undead  fight_undead
#define battle_boss    fight_boss
#define battle_dragon  fight_dragon
static unsigned char fight_wander(void);
static unsigned char fight_undead(void);
static unsigned char fight_boss(unsigned char dungeon);
static unsigned char fight_dragon(void);

/* The letters are for walking, as they are on the map. */
#define K_W          0x57
#define K_A          0x41
#define K_S          0x53
#define K_D          0x44
#define K_POTION     0x50
#define K_F          0x46
#define K_C          0x43
#define K_M          0x4D
#define K_X          0x58
#define K_SPACE      0x20
#define K_CRSR_DOWN  0x11
#define K_CRSR_RIGHT 0x1D
#define K_CRSR_UP    0x91
#define K_CRSR_LEFT  0x9D


/* How each SHOT_* kind lands, by the same index. */
static const char *const shot_hits[] = {
    "An arrow hits ",
    "Flame sears ",
    "A wave hits ",
    "Lightning strikes ",
};

/* The bestiary -- wanderers[], natives[], keepers[] and maldrath -- is in
   bestiary.c, so that the multicolour arena (src/owarena.c) can have it
   without the fight. */
#define NUM_WANDERERS num_wanderers

/* Experience the party needs for each level, from level 1. */
static const unsigned int level_at[] = {
    0, 20, 50, 100, 200, 400, 700, 1200, 1800, 2500,
};
#define MAX_LEVEL (sizeof(level_at) / sizeof(level_at[0]))

#define NONE 0xFF

/* What a class adds, found by having the arena fight it out
   (tools/balance.py): an archer's arrows hit harder as the party rises, and
   a wizard's spells grow with it, or three of either could not carry a
   keeper the way three warriors can. */
#define ARCHER_EYE  (3 + (party_level >> 1))
#define LONE_HP     3
#define LONE_ATK    2
#define WARD        4
/* A wizard can wear nothing heavier than robes, so their own wards stand in
   for armour, more as the party rises. */
#define WIZ_GUARD   ((party_level << 1) / 3)

static char msg[SCR_W + 1];    /* PETSCII, for the message log */

static void cat_num(unsigned int value)
{
    utoa(value, msg + strlen(msg), 10);
}

/* For `n` of them: a group pays for every one it had. The experience is the
   party's, and so is a level: everyone grows with it, mended, and a wizard's
   magic comes back. */
static void award(const Monster *m, unsigned char n)
{
    unsigned int xp = m->xp * n, pay = m->gold * n;

    party_xp += xp;
    gold += pay;

    strcpy(msg, "You gain ");
    cat_num(xp);
    strcat(msg, " experience and ");
    cat_num(pay);
    strcat(msg, " marks.");
    view_message(msg);

    while (party_level < MAX_LEVEL && party_xp >= level_at[party_level]) {
        ++party_level;
        party_grow();

        strcpy(msg, "The party reaches level ");
        cat_num(party_level);
        strcat(msg, ".");
        view_message(msg);
    }
}

/* --- the field ----------------------------------------------------------
 *
 * Every fight happens on a map: the ground you were crossing, a dungeon
 * room, or Maldrath's lair. The party starts near the bottom, it starts near
 * the top, and each round every one of the party takes a turn, in order, and
 * then it moves towards whichever of them it can reach soonest. Walking into
 * it is how you hit it; it hits you by walking into you.
 *
 * Whoever's turn it is is the sprite (sprite_as()); the rest of the party
 * stand on the field as their class's glyph, in their tunic's colour.
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


/* The fight's scratch is not BSS: it is the end of the fight's own engine
   (overlay.s), where nothing else is while it runs. Nothing in it carries
   from one fight to the next; each is set up in battle_fight(). */
extern unsigned char field[FIELD_SIZE];
extern unsigned char wall[RIM_W * RIM_H];    /* 1 where nothing can stand */
extern unsigned char roomy[RIM_W * RIM_H];   /* 0 where a 2x2 body fits,
                                                by its top-left square */
extern unsigned char seen[RIM_W * RIM_H];
extern unsigned char queue_x[256], queue_y[256];
static unsigned char *seen_row[RIM_H];
static unsigned char hx, hy, mx, my;
static unsigned char size;      /* the monster's, squares a side */
static const Monster *foe;

/* The party on the field: where each stands, and how hard each is braced
   (SPACE) until their next turn. `act` is whose turn it is, standing at
   (hx,hy); `ward` is the Ward spell's, for everyone, for the fight; `sleep`
   how many rounds the foes have yet to sleep. */
/* Set up afresh in every fight, so they can go where nothing is cleared
   (BSS2). */
#pragma bss-name (push, "BSS2")
static unsigned char mem_x[MAX_PARTY], mem_y[MAX_PARTY], guard[MAX_PARTY];
#pragma bss-name (pop)
static unsigned char act, ward, sleep;
/* The monsters' speed against the party's (fight()), and whether one came
   alone (their_turn()). */
static unsigned char rel, lone, lone_bank;
/* ...and how much harder one that came alone hits, for a party: LONE_ATK for
   each of them past the first. */
static unsigned char fury;

#ifdef FIGHT_BOT
/* The arena's autoplayer (below): on, and what it counted. */
unsigned char bot_on;
unsigned int  bot_rounds, bot_falls;
#endif

/* The group: where each one stands, the health it has left (0 once it has
   fallen) and its banked moves. */
static unsigned char foes, alive, cur;
static unsigned char foe_x[MAX_FOES], foe_y[MAX_FOES];
static unsigned char foe_pace[MAX_FOES];
/* Health is more than a byte holds: a keeper or Maldrath met by a party has
   more for each of them (fight()). Set up afresh in every fight, so it can go
   where nothing is cleared. */
#pragma bss-name (push, "BSS2")
static unsigned int  foe_hp[MAX_FOES];
#pragma bss-name (pop)

/* Is (x,y) part of the monster whose turn it is, at (mx,my)? Unsigned, so
   a square above or to the left of it wraps round to a large number and
   fails the same test. foe_at() below asks the same of the whole group. */
static unsigned char on_foe(unsigned char x, unsigned char y)
{
    return (unsigned char)(x - mx) < size && (unsigned char)(y - my) < size;
}

/* Which of the group has any part of it on (x,y), or NONE. */
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

/* Which of the party still standing is on (x,y), or NONE. */
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

/* Draw the ground at (x,y), or the part of a monster or the one of the
   party standing there -- bar whoever's turn it is, who is the sprite. */
static void paint(unsigned char x, unsigned char y)
{
    unsigned int  at = (unsigned int)y * SCR_W + x;
    unsigned char code = field[(unsigned int)y * LOCAL_W + x];
    unsigned char i = foe_at(x, y);

    if (i != NONE) {
        SCREEN_RAM[at] = foe->glyph
                       + (unsigned char)((y - foe_y[i]) * size)
                       + (unsigned char)(x - foe_x[i]);
        COLOR_RAM[at]  = foe->color;
    } else if ((i = member_at(x, y)) != NONE && i != act) {
        SCREEN_RAM[at] = class_glyph[party[i].cls];
        COLOR_RAM[at]  = tunic_of(&party[i]);
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

/* The exchange of blows. Each returns the damage done and says so;
   strike()'s line begins with `how`, "Alaric strikes for " or a shot's,
   built in msg. */
static unsigned char strike(unsigned int m_hp, unsigned char atk)
{
    unsigned char hit;

    hit = atk + party_level + (unsigned char)(rand() % 3);
    hit = (hit > foe->def) ? hit - foe->def : 1;

    cat_num(hit);
    strcat(msg, ".");
    view_message(msg);
    return hit >= m_hp ? (unsigned char)m_hp : hit;
}

/* msg = the active member's name and `rest`. */
static void say_who(unsigned char who, const char *rest)
{
    strcpy(msg, party[who].name);
    strcat(msg, rest);
}

/* Member `who` takes a blow of `atk`; msg already holds the line up to the
   number. Returns 1 if that was the last of the party standing. */
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
    view_message(msg);

    if (hit < a->hp) {
        a->hp -= hit;
        return 0;
    }
    a->hp = 0;                /* fallen: nobody with no health stands */
#ifdef FIGHT_BOT
    ++bot_falls;
#endif
    paint(mem_x[who], mem_y[who]);
    if (who == act) {
        sprite_hide();
    }
    say_who(who, " falls!");
    view_message(msg);
    for (hit = 0; hit < party_count; ++hit) {
        if (party[hit].hp) {
            return 0;
        }
    }
    return 1;
}

/* Who, of the party still standing? Asked only when there is a choice:
   returns NONE if the player thought better of it. */
static unsigned char next_key(void);

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
    view_message(msg);
    i = next_key() - 0x31;
    if (i < party_count && party[i].hp) {
        return i;
    }
    view_message("Never mind.");
    return NONE;
}

/* P during a fight: whoever's turn it is gives a draught to one of the
   party, themselves or another. Returns 0 if nothing happened, so no turn
   is spent. */
static unsigned char quaff(void)
{
    unsigned char healed, who;

    if (!potions) {
        view_message("You have no draughts.");
        return 0;
    }
    who = pick_member("Who drinks?");
    if (who == NONE) {
        return 0;
    }
    healed = drink_potion(who);
    if (!healed) {
        say_who(who, " is unhurt.");
        view_message(msg);
        return 0;
    }
    say_who(who, " drinks. ");
    cat_num(healed);
    strcat(msg, " restored.");
    view_message(msg);
    return 1;
}

/* Whoever fell gets up again when it is over, won or fled, with a little
   health: only if every one of the party falls is the fight lost. */
static void get_up(void)
{
    unsigned char i;

    for (i = 0; i < party_count; ++i) {
        if (!party[i].hp) {
            party[i].hp = 1;
            say_who(i, " gets up again.");
            view_message(msg);
        }
    }
}

/* The log carries on into the map afterwards, so a win needs no pause --
   and a "Press a key." would stay in it. Dying moves you, so it does. */
static unsigned char won(const Monster *m, unsigned char n)
{
    unsigned char got;

    view_message("It falls.");
    award(m, n);
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
        view_message(msg);
    }
    /* The map shares these out once it is back (fightcall.c): a treasure
       map, and what goes armed -- goblins, bandits, skeletons and ogres,
       and whatever of the continents' own is drawn as one -- may drop arms. */
    fight_spoils = SPOIL_WON;
    if (m->glyph == T_GOBLIN
        || (unsigned char)(m->glyph - T_BANDIT) <= T_OGRE - T_BANDIT) {
        fight_spoils |= SPOIL_ARMED;
    }
    return BATTLE_WON;
}

static unsigned char lost(void)
{
    view_message("The party has fallen.");
#ifdef FIGHT_BOT
    if (bot_on) {
        return BATTLE_DEAD;
    }
#endif
    view_message("Press a key.");
    cgetc();
    return BATTLE_DEAD;
}

/* Is any of the party standing beside the monster whose turn it is -- up,
   down, left or right of any part of it? Which, or NONE. */
static unsigned char next_to(void)
{
    unsigned char i, x, y;

    for (i = 0; i < party_count; ++i) {
        if (!party[i].hp) {
            continue;
        }
        x = mem_x[i];
        y = mem_y[i];
        if (on_foe(x, y - 1) || on_foe(x, y + 1)
            || on_foe(x - 1, y) || on_foe(x + 1, y)) {
            return i;
        }
    }
    return NONE;
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

/* Find the monster's way to the party, the shortest way round whatever is
   in the way. Returns 1 if there is one, and leaves the first step of it
   in (step_x, step_y), in rim coordinates; 0 if none of the party can be
   reached from where it stands.
 *
 * A breadth-first flood out from every one of the party standing at once:
 * the square the flood reaches the monster from is one step nearer to
 * whichever of them is nearest than where it stands.
 *
 * A two-by-two monster is flooded as its top-left square, over `roomy`
 * (where its whole body fits) instead of `wall`. The flood starts from the
 * eight places that would put it beside each of the party, with the four
 * that would put it on top of any of them ruled out first.
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
    if (size == 1) {
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
                x = mem_x[i] + 1;
                y = mem_y[i] + 1;
                seed(x - 1, y + 1); seed(x, y + 1);
                seed(x - 1, y - 2); seed(x, y - 2);
                seed(x + 1, y - 1); seed(x + 1, y);
                seed(x - 2, y - 1); seed(x - 2, y);
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
   waits where it is. The flood's first step can be one of the party's own
   squares only when the monster is already beside them, and then it
   strikes instead of stepping; a 2x2 body is kept off them by the flood. */
static void foe_step(void)
{
    unsigned char ox = mx, oy = my;

    if (flood() && member_at(step_x - 1, step_y - 1) == NONE) {
        mx = foe_x[cur] = step_x - 1;
        my = foe_y[cur] = step_y - 1;
        paint_body(ox, oy);
        paint_body(mx, my);
    }
}

/* Where monster `cur` starts: anywhere in the top third of the field that
   its whole body can stand on, that the rest of the group has left free,
   and that has a way through to the party, so no fight begins with it shut
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
            && foe_at(mx, my) == NONE && member_at(mx, my) == NONE
            && flood()) {
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

/* The party's places: the leader on the field's own spot, the rest beside
   and behind, wherever is open. */
static const signed char beside_x[] = { -1, 1,  0, -1, 1, -2, 2, 0 };
static const signed char beside_y[] = {  0, 0,  1,  1, 1,  0, 0, -1 };

static void party_start(unsigned char a)
{
    unsigned char i, k = 0, x, y;

    for (i = 0; i < party_count; ++i) {
        mem_x[i] = mem_y[i] = NONE;
        guard[i] = 0;
    }
    mem_x[0] = arena_hx[a];
    mem_y[0] = arena_hy[a];
    for (i = 1; i < party_count; ++i) {
        for (; k < sizeof(beside_x); ++k) {
            x = arena_hx[a] + beside_x[k];
            y = arena_hy[a] + beside_y[k];
            if (x < LOCAL_W && y < LOCAL_H
                && !wall[(unsigned int)(y + 1) * RIM_W + x + 1]) {
                break;
            }
        }
        mem_x[i] = x;
        mem_y[i] = y;
        ++k;
    }
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

/* The next key, with SHIFTed letters folded onto unshifted ones; and the
   joystick too, its button as F. */
#ifdef FIGHT_BOT
/* --- the arena's autoplayer ----------------------------------------------
 *
 * Only in the arena (src/arena.c), which links this file compiled a second
 * time with FIGHT_BOT: it plays the party, so that a fight can be fought a
 * few hundred times and the outcome counted, the way the keepers were once
 * tuned by hand. It feeds next_key() the keys a careful player would press,
 * and knows nothing the player could not see. Nothing of it is in the game.
 *
 * Each turn: a draught for whoever is worst hurt, if they are badly hurt; a
 * wizard heals, wards against something big, burns a crowd, or bolts what
 * is in line; whoever has a bow shoots what is in line; whoever is beside
 * it strikes; otherwise each closes in -- the ranged ones to get in line,
 * the rest to get beside it. */
static unsigned char bot_q[4], bot_n, bot_i;

static const unsigned char dir_key[4] = { K_S, K_W, K_A, K_D };

static void bot_push(unsigned char k)
{
    bot_q[bot_n++] = k;
}

/* Is a foe in line `dir` of the active member, within `range`, with nothing
   solid between? (The shot flies over the party.) */
static unsigned char bot_line(unsigned char dir, unsigned char range)
{
    unsigned char x = hx, y = hy, n;

    for (n = 0; n < range; ++n) {
        switch (dir) {
            case DIR_UP:   --y; break;
            case DIR_DOWN: ++y; break;
            case DIR_LEFT: --x; break;
            default:       ++x; break;
        }
        if (x >= LOCAL_W || y >= LOCAL_H) {
            return 0;
        }
        if (foe_at(x, y) != NONE) {
            return 1;
        }
        if (tile_flags[field[(unsigned int)y * LOCAL_W + x] - TILE_BASE]
            == TF_BLOCK) {
            return 0;
        }
    }
    return 0;
}

static unsigned char bot_aim(unsigned char range)
{
    unsigned char d;

    for (d = 0; d < 4; ++d) {
        if (bot_line(d, range)) {
            return d;
        }
    }
    return NONE;
}

static unsigned char bot_open(unsigned char x, unsigned char y)
{
    return x < LOCAL_W && y < LOCAL_H
        && !wall[(unsigned int)(y + 1) * RIM_W + x + 1]
        && member_at(x, y) == NONE && foe_at(x, y) == NONE;
}

static unsigned char bot_near(unsigned char x, unsigned char y)
{
    return foe_at(x, y) != NONE;
}

static unsigned char bot_alive(void)
{
    unsigned char i, n = 0;

    for (i = 0; i < party_count; ++i) {
        n += party[i].hp != 0;
    }
    return n;
}

/* Cast spell `s` (its key) at `arg`: a direction, a member, or nothing. */
static void bot_cast(unsigned char s, unsigned char arg, unsigned char kind)
{
    bot_push(K_C);
    bot_push(0x31 + s);
    if (kind == 1) {
        bot_push(dir_key[arg]);
    } else if (kind == 2 && bot_alive() > 1) {
        bot_push(0x31 + arg);
    }
}

static void bot_plan(void)
{
    Actor *a = &party[act];
    unsigned char i, low = NONE, d, nx, ny, best = 255, dist, tx = 0, ty = 0;
    unsigned char ranged, dx, dy, adx, ady, first, second, danger;

    for (i = 0; i < party_count; ++i) {
        if (party[i].hp && (low == NONE
            || (unsigned int)party[i].hp * party[low].hp_max
               < (unsigned int)party[low].hp * party[i].hp_max)) {
            low = i;
        }
    }
    /* In danger: the hardest blow it has could fell them, or they are under
       a third of their health. */
    i = (foe->shot > foe->atk ? foe->shot : foe->atk) + 2;
    d = armour_def[party[low].armour] + ward
      + (party[low].cls == CL_WIZARD ? WIZ_GUARD : 0);
    danger = party[low].hp <= (i > d ? i - d : 1)
          || party[low].hp * 3 < party[low].hp_max;

    if (a->cls == CL_WIZARD) {
        if ((a->ready & (1 << SP_HEAL)) && a->mp >= spell_cost[SP_HEAL]
            && danger) {
            bot_cast(SP_HEAL, low, 2);
            return;
        }
    }
    if (potions && danger && party[low].hp < party[low].hp_max) {
        bot_push(K_POTION);
        if (bot_alive() > 1) {
            bot_push(0x31 + low);
        }
        return;
    }
    if (a->cls == CL_WIZARD) {
        if ((a->ready & (1 << SP_WARD)) && !ward && foe->hp >= 40
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
        bot_push(K_F);
        bot_push(dir_key[d]);
        return;
    }
    for (d = 0; d < 4; ++d) {
        nx = hx; ny = hy;
        switch (d) {
            case DIR_UP:   --ny; break;
            case DIR_DOWN: ++ny; break;
            case DIR_LEFT: --nx; break;
            default:       ++nx; break;
        }
        if (foe_at(nx, ny) != NONE) {
            bot_push(dir_key[d]);
            return;
        }
    }

    /* Close in on the nearest. */
    for (i = 0; i < foes; ++i) {
        if (!foe_hp[i]) {
            continue;
        }
        nx = foe_x[i] + (hx > foe_x[i] ? (hx - foe_x[i] < size ? hx - foe_x[i]
                                                                : size - 1) : 0);
        ny = foe_y[i] + (hy > foe_y[i] ? (hy - foe_y[i] < size ? hy - foe_y[i]
                                                                : size - 1) : 0);
        dist = (nx > hx ? nx - hx : hx - nx) + (ny > hy ? ny - hy : hy - ny);
        if (dist < best) {
            best = dist;
            tx = nx;
            ty = ny;
        }
    }
    ranged = (a->bow && arrows)
          || (a->cls == CL_WIZARD && a->mp >= spell_cost[SP_SPARK]
              && (a->ready & (1 << SP_SPARK)));
    adx = tx > hx ? tx - hx : hx - tx;
    ady = ty > hy ? ty - hy : hy - ty;
    dx = tx > hx ? DIR_RIGHT : DIR_LEFT;
    dy = ty > hy ? DIR_DOWN : DIR_UP;
    /* The ranged line up (close the smaller gap); the rest close the
       larger. */
    if (ranged ? (adx && (adx <= ady || !ady)) : adx >= ady) {
        first = dx; second = dy;
    } else {
        first = dy; second = dx;
    }
    if (!adx) { first = dy; second = dy; }
    if (!ady) { first = dx; second = dx; }
    for (i = 0; i < 2; ++i) {
        d = i ? second : first;
        nx = hx; ny = hy;
        switch (d) {
            case DIR_UP:   --ny; break;
            case DIR_DOWN: ++ny; break;
            case DIR_LEFT: --nx; break;
            default:       ++nx; break;
        }
        /* The ranged keep a square between them and it. */
        if (bot_open(nx, ny)
            && !(ranged && (bot_near(nx, ny - 1) || bot_near(nx, ny + 1)
                            || bot_near(nx - 1, ny) || bot_near(nx + 1, ny)))) {
            bot_push(dir_key[d]);
            return;
        }
    }
    bot_push(K_SPACE);
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

static unsigned char next_key(void)
{
    unsigned char key;

#ifdef FIGHT_BOT
    if (bot_on) {
        return bot_key();
    }
#endif
    key = stick_key();

    if (key == STICK_FIRE) {
        return K_F;
    }
    return (key >= 0xC1 && key <= 0xDA) ? key - 0x80 : key;
}

/* Something flies from (hx,hy) `dir`: an arrow, or a spell. It flies
   straight until it meets a monster, something solid, the edge of the field
   or the end of its `range`. Water does not stop it, nor do the party
   (it flies over them); trees, boulders and walls do. Returns the damage,
   at `atk`, and leaves which one it hit in `target`; `what` begins the
   line ("Your arrow"). */
static unsigned char target;

static unsigned char loose(unsigned char dir, unsigned char range,
                           unsigned char atk, unsigned char kind,
                           const char *what)
{
    unsigned char x = hx, y = hy, flew = 0, hit = 0;

    sprite_face(dir);
    while (flew < range) {
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
        if ((target = foe_at(x, y)) != NONE) {
            hit = 1;
            break;
        }
        if (tile_flags[field[(unsigned int)y * LOCAL_W + x] - TILE_BASE]
            == TF_BLOCK) {
            break;
        }
    }

    sprite_shot(hx, hy, dir, flew, kind);
    strcpy(msg, what);
    if (hit) {
        sfx_play(SFX_BOW);
        strcat(msg, " hits for ");
        return strike(foe_hp[target], atk);
    }
    strcat(msg, " goes wide.");
    view_message(msg);
    return 0;
}

/* Ask for a direction to shoot or cast; 0xFF if the player thought better
   of it. */
static unsigned char which_way(const char *put_down)
{
    unsigned char dir;

    view_message("Which way?");
    dir = key_dir(next_key());
    if (dir == 0xFF) {
        view_message(put_down);
    }
    return dir;
}

/* Can the monster shoot one of the party from where it stands? The same
   rules as the party's bows, turned round: straight up, down, left or
   right, within its reach, and nothing solid in between -- the rest of its
   group included, and the first of the party in the way takes it. Returns
   the direction, or 0xFF, and leaves where the shot sets off from in
   (sx,sy), how far it flies in `flew` and who it hits in `victim`. A body
   bigger than a square shoots from whichever part of it lines up. */
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
        if ((victim = member_at(x, y)) != NONE) {
            return dir;
        }
        if (tile_flags[field[(unsigned int)y * LOCAL_W + x] - TILE_BASE]
            == TF_BLOCK || foe_at(x, y) != NONE) {
            break;
        }
    }
    return 0xFF;                             /* out of reach, or blocked */
}

static unsigned char aim(void)
{
    unsigned char i, dir;

    if (!foe->reach) {
        return 0xFF;
    }
    for (i = 0; i < party_count; ++i) {
        if (party[i].hp && (dir = aim_at(mem_x[i], mem_y[i])) != 0xFF) {
            return dir;
        }
    }
    return 0xFF;
}

/* No two fights are fought on the same ground: some ten boulders go down where
   the ground is open all round (so nothing is cut off from anything), never
   on the leader's own square. Run once the walls are marked. */
unsigned char open9(const unsigned char *w);     /* open9.s */

static void scatter(unsigned char a)
{
    unsigned char n, tries, x, y;
    unsigned char *w;

    n = 8 + (rand() & 3);
    for (tries = 0; tries < 60; ++tries) {
        x = 2 + ((unsigned char)rand() & 15);
        y = 2 + ((unsigned char)rand() & 15);
        w = wall + (unsigned int)(y + 1) * RIM_W + x + 1;
        if ((x != arena_hx[a] || y != arena_hy[a]) && open9(w)) {
            *w = 1;
            field[(unsigned int)y * LOCAL_W + x] = T_BOULDER;
            if (!--n) {
                break;
            }
        }
    }
}

/* --- spells --------------------------------------------------------------- */

/* Damage dealt to target `t`, or to them all: 0 if nothing was hit. The
   caller takes it off and sees who falls. */
static unsigned char hurt_all;

/* C: the wizard whose turn it is casts one of their prepared spells. Returns
   the damage done to `target` (or, with hurt_all, to every foe) and sets
   *spent if a turn was spent. */
static unsigned char cast(unsigned char *spent)
{
    Actor *a = &party[act];
    unsigned char s, dir, who, amount;

    *spent = 0;
    hurt_all = 0;
    if (a->cls != CL_WIZARD) {
        view_message("Only a wizard can cast.");
        return 0;
    }
    if (!a->ready) {
        view_message("No spell is prepared.");
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
    view_message(msg);
    s = next_key() - 0x31;
    if (s >= NUM_SPELLS || !(a->ready & (1 << s))) {
        view_message("Never mind.");
        return 0;
    }
    if (a->mp < spell_cost[s]) {
        view_message("Not enough magic.");
        return 0;
    }

    switch (s) {
        case SP_SPARK:
        case SP_BOLT:
            dir = which_way("The spell fades.");
            if (dir == 0xFF) {
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
                view_message("The ward already holds.");
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
            return loose(dir, 14, 8 + party_level + (party_level >> 1), SHOT_BOLT,
                         "The bolt");
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
            view_message(msg);
            return 0;
        case SP_WARD:
            ward = WARD;
            view_message("A ward settles on the party.");
            return 0;
        case SP_SLEEP:
            /* Two rounds: a lone one's turns come one for each of the
               party. */
            sleep = lone ? party_count << 1 : 2;
            view_message("A drowse falls on them.");
            return 0;
        default:                             /* SP_BLAZE */
            hurt_all = 1;
            sfx_play(SFX_BOW);
            strcpy(msg, "Fire takes them all for ");
            return strike(0xFFFF, 4 + party_level);
    }
}

/* --- the fight ------------------------------------------------------------ */

/* Whose turn it is: the sprite goes to them, and the one before is drawn
   back as a glyph. */
static void turn_to(unsigned char i)
{
    unsigned char was = act;

    act = i;
    party_turn = i;
    if (was != NONE && party[was].hp) {
        paint(mem_x[was], mem_y[was]);
    }
    hx = mem_x[i];
    hy = mem_y[i];
    paint(hx, hy);
    sprite_as(i);
    sprite_show(hx, hy);
}

/* Take `dmg` off foe `t`; returns 1 if that was the last of them. */
static unsigned char wound(unsigned char t, unsigned char dmg)
{
    if (dmg >= foe_hp[t]) {
        foe_hp[t] = 0;
        paint_body(foe_x[t], foe_y[t]);      /* ground now */
        if (!--alive) {
            return 1;
        }
        view_message("It falls.");
        return 0;
    }
    foe_hp[t] -= dmg;
    return 0;
}

/* Their turn, one after another: as many moves each as it has banked whole
   ones. Beside one of the party it strikes; in line and in reach it shoots
   (or breathes), and that is the rest of its turn -- a fast archer nocks one
   arrow at a time. Asleep, they only stir. Returns 1 if that was the last of
   the party standing.
 *
 * A group takes its turn once a round, when the whole party has had theirs;
 * a monster that comes alone, a keeper or Maldrath or a lone wanderer, takes
 * one after each of the party's, so that three of them do not get three
 * blows to its one. */

static unsigned char their_turn(void)
{
    unsigned char i, dir, pace;

    if (sleep) {
        --sleep;
        view_message(sleep ? "They sleep." : "They stir.");
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
                /* Alone it is "it"; in a group, say which. */
                strcpy(msg, foes > 1 ? foe->name : (const char *)"It");
                strcat(msg, " hits ");
                strcat(msg, party[i].name);
                strcat(msg, " for ");
                sfx_delay();         /* let the blow before it finish */
                sfx_play(SFX_MELEE);
                if (bitten(i, foe->atk + fury)) {
                    view_draw_panel();
                    return 1;
                }
            } else if ((dir = aim()) != 0xFF) {
                sprite_shot(sx, sy, dir, flew, foe->kind);
                sfx_delay();
                sfx_play(SFX_BOW);
                strcpy(msg, shot_hits[foe->kind]);
                strcat(msg, party[victim].name);
                strcat(msg, " for ");
                if (bitten(victim, foe->shot + fury)) {
                    view_draw_panel();
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

static unsigned char fight(const Monster *m, unsigned char ground,
                           unsigned char can_flee, unsigned char count)
{
    unsigned char a, key, dir, nx, ny, dmg, spent;
    unsigned char i;
    unsigned int  hp;
    unsigned char *edge;
    unsigned int  n;

    /* The field for this kind of ground, or the first one if there is
       none. */
    for (a = NUM_ARENAS - 1; a && arena_tile[a] != ground; --a) {
    }
    rle_unpack(field, arena_map[a], FIELD_SIZE);
    memset(wall, 1, sizeof(wall));
    n = 0;
    for (ny = 0; ny < RIM_H; ++ny) {
        seen_row[ny] = seen + (unsigned int)ny * RIM_W;
        if (ny == 0 || ny > LOCAL_H) {
            continue;                        /* the rim stays wall */
        }
        edge = wall + (unsigned int)ny * RIM_W + 1;
        for (nx = 0; nx < LOCAL_W; ++nx, ++n) {
            edge[nx] = tile_flags[field[n] - TILE_BASE] != TF_PASS;
        }
    }
    if (m->size == 1) {
        scatter(a);
    }
    size = m->size;
    if (size == 2) {
        memset(roomy, 1, sizeof(roomy));
        for (n = 0; n < sizeof(roomy) - RIM_W - 1; ++n) {
            roomy[n] = wall[n] | wall[n + 1]
                     | wall[n + RIM_W] | wall[n + RIM_W + 1];
        }
    }
    foe = m;
    act = NONE;
    ward = sleep = 0;
    party_start(a);

    /* One that comes alone is the tougher for every one of the party past
       the first: three quarters as much again each (LONE_HP, in quarters),
       found by having the arena fight it out (tools/balance.py). */
    hp = m->hp;
    if (count == 1) {
        hp += (hp * LONE_HP * (party_count - 1)) >> 2;
    }

    /* Placed one at a time, each clear of the ones before it. */
    memset(foe_hp, 0, sizeof(foe_hp));
    memset(foe_pace, 0, sizeof(foe_pace));
    foes = count;
    for (cur = 0; cur < count && foe_start(a); ++cur) {
        foe_x[cur] = mx;
        foe_y[cur] = my;
        foe_hp[cur] = hp;
    }
    foes = alive = cur;
    lone = foes == 1;
    lone_bank = 0;
    fury = lone ? LONE_ATK * (party_count - 1) : 0;

    /* Speed is relative: the party's is 1.0, less what its armour costs on
       the average (armour_slow: 0.6 in plate), and whatever it loses the
       monster gains. Its moves are banked in tenths, so a relative 1.5 moves
       once, then twice, then once. */
    n = 0;
    for (i = 0; i < party_count; ++i) {
        n += armour_slow[party[i].armour];
    }
    rel = m->speed + (unsigned char)(n / party_count);

    view_restore();
    draw_field();
    sprite_face(DIR_UP);

    strcpy(msg, m->name);
    strcat(msg, party_count > 1 ? " comes at the party!" : " comes at you!");
    view_message(msg);
    if (foes > 1) {
        strcpy(msg, "And ");
        cat_num(foes - 1);
        strcat(msg, " more!");
        view_message(msg);
    }
    view_message(can_flee ? "Walk into it to strike. Flee by a side."
                          : "Walk into it to strike.");

    for (;;) {
#ifdef FIGHT_BOT
        /* A fight the bot cannot finish (shut out of reach) is called off. */
        if (bot_on && ++bot_rounds > 300) {
            return BATTLE_FLED;
        }
#endif
        /* The party's turns, one after another. */
        for (i = 0; i < party_count; ++i) {
            if (!party[i].hp) {
                continue;
            }
            turn_to(i);
            guard[i] = 0;
            /* A wizard draws a little magic back each round of a fight,
               or a long one would leave them with only a short sword. */
            if (party[i].mp < party[i].mp_max) {
                ++party[i].mp;
            }
            for (;;) {
                view_draw_panel();
                show_foe();

                dmg = 0;
                hurt_all = 0;
                key = next_key();
                dir = key_dir(key);

                switch (key) {
                    case K_F:
                        if (!party[act].bow) {
                            say_who(act, " has no bow.");
                            view_message(msg);
                            continue;
                        }
                        if (!arrows) {
                            view_message("You have no arrows.");
                            continue;
                        }
                        dir = which_way("You lower the bow.");
                        if (dir == 0xFF) {
                            continue;                /* no turn spent */
                        }
                        --arrows;
                        /* An archer's eye is worth more as the party
                           rises: ARCHER_EYE on top of the bow. */
                        dmg = loose(dir, bow_range[party[act].bow],
                                    bow_atk[party[act].bow]
                                    + (party[act].cls == CL_ARCHER
                                       ? ARCHER_EYE : 0), SHOT_ARROW,
                                    party[act].bow == NUM_BOWS - 1
                                        ? "The bolt" : "The arrow");
                        dir = 0xFF;                  /* shooting is not walking */
                        break;

                    case K_C:
                        dmg = cast(&spent);
                        if (!spent) {
                            continue;
                        }
                        dir = 0xFF;
                        break;

                    case K_SPACE:
                        guard[act] = 4;
                        say_who(act, " stands firm.");
                        view_message(msg);
                        break;

                    case K_POTION:
                        if (!quaff()) {
                            continue;
                        }
                        break;

                    case K_M:
                        view_message(music_toggle() ? "Music on."
                                                    : "Music off.");
                        continue;                    /* no turn spent */

                    case K_X:
                        view_message(sfx_toggle() ? "Effects on."
                                                  : "Effects off.");
                        continue;

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

                    if ((target = foe_at(nx, ny)) != NONE) {
                        sfx_play(SFX_MELEE);
                        say_who(act, " strikes for ");
                        dmg = strike(foe_hp[target],
                                     weapon_atk[party[act].weapon]);
                    } else if (nx >= LOCAL_W || ny >= LOCAL_H
                               || wall[(unsigned int)(ny + 1) * RIM_W
                                       + nx + 1]
                               || member_at(nx, ny) != NONE) {
                        /* Unsigned: stepping off the top or left wraps to
                           255. The gaps in the middle of the sides are the
                           way out, for the whole party. */
                        if (can_flee && ny == LOCAL_H / 2 && nx >= LOCAL_W) {
                            view_message("You get away.");
                            get_up();
                            return BATTLE_FLED;
                        }
                        continue;                    /* bumping costs no turn */
                    } else {
                        sprite_walk(dir);
                        hx = mem_x[act] = nx;
                        hy = mem_y[act] = ny;
                        sprite_show(hx, hy);
                    }
                }
                break;
            }

            /* strike() never does less than 1, so a blow that landed is
               never 0; a shot that went wide is. */
            if (dmg) {
                if (hurt_all) {
                    for (target = 0; target < foes; ++target) {
                        if (foe_hp[target] && wound(target, dmg)) {
                            show_foe();
                            return won(m, foes);
                        }
                    }
                } else if (wound(target, dmg)) {
                    show_foe();
                    return won(m, foes);
                }
            }
            /* Alone, it has (n + 1) / 2 turns a round for a party of n --
               one against one hero, two against three -- spread between
               theirs: banked in halves of a turn, a member's worth at a
               time. */
            if (lone) {
                lone_bank += party_count + 1;
                if (lone_bank >= party_count << 1) {
                    lone_bank -= party_count << 1;
                    if (their_turn()) {
                        return lost();
                    }
                }
            }
        }

        /* A group moves once the whole party has; one that comes alone
           has moved after each of them already. */
        if (!lone && their_turn()) {
            return lost();
        }
    }
}

/* The panel's rows for the place stay out of the way while a fight names
   its foe there: view_draw_panel() repainting them showed through. */
unsigned char battle_fight(const Monster *m, unsigned char ground,
                           unsigned char can_flee, unsigned char count)
{
    unsigned char r;

    view_fighting = 1;
    r = fight(m, ground, can_flee, count);
    view_fighting = 0;
    party_turn = NONE;
    sprite_as(0);
    return r;
}

/* Fight `n` of wanderer `i`. Above ground, on the ground you were on;
   below it, the one room. */
static unsigned char wander_fight(unsigned char i, unsigned char n)
{
    return battle_fight(&wanderers[i].m,
                       scene != SCENE_DUNGEON ? map_at(hero.x, hero.y)
                                              : T_FLOOR,
                       1, n);
}

/* One or two of the land's own, as strong as the story's order of the
   continents makes them. A copy, which only ever lives for the fight: written
   before it is read, so it can go where nothing is cleared (BSS2). */
#pragma bss-name (push, "BSS2")
static Monster native;
#pragma bss-name (pop)

static unsigned char native_fight(void)
{
    unsigned char s;

    native = natives[isle_biome * 2 + (rand() & 1)];
    for (s = slot_of(land); s; --s) {
        native.hp   += 10;
        native.atk  += 3;
        native.def  += 1;
        native.xp   += 10;
        native.gold += 8;
        if (native.reach) {
            native.shot += 3;
        }
    }
    return battle_fight(&native,
                        scene != SCENE_DUNGEON ? map_at(hero.x, hero.y)
                                               : T_FLOOR,
                        1, 1 + (rand() & 1));
}

/* Each level down counts as one more of the hero's own: the pool of what
   turns up is drawn from further along the list, and in bigger groups. */
unsigned char battle_wander(void)
{
    unsigned char top, i, most, lvl;

    /* Over the sea, a quarter of what turns up is the land's own. */
    if (land != ISLE_HOME && !(rand() & 3)) {
        return native_fight();
    }

    lvl = party_level + (scene == SCENE_DUNGEON ? depth - 1 : 0);

    /* The rat and the goblin are there from level 1, so `top` is never 0. */
    for (top = 0; top < NUM_WANDERERS && wanderers[top].from <= lvl;
         ++top) {
    }
    i = (unsigned char)(rand() % top);

    /* As many as the party has levels for each one's worth, and one more
       for each of them past the first: a level-6 party of one can meet up
       to six rats (from level 1) or three wasps (from 2). */
    most = (lvl + party_count - 1) / wanderers[i].from;
    if (most > MAX_FOES) {
        most = MAX_FOES;
    }
    return wander_fight(i, (unsigned char)(1 + rand() % most));
}

/* What a grave robber meets: the dead, whatever else is about. The skeleton
   and the wraith are fixed places in wanderers[]. */
unsigned char battle_undead(void)
{
    if (party_level >= 6) {
        return wander_fight(9, 1);
    }
    return wander_fight(6, (unsigned char)(1 + rand() % 2));
}

unsigned char battle_boss(unsigned char dungeon)
{
    return battle_fight(&keepers[dungeon], T_FLOOR, 0, 1);
}

unsigned char battle_dragon(void)
{
    return battle_fight(&maldrath, T_LAIR, 0, 1);
}

/* The fight's engine: engine.h. */
unsigned char fight_entry(void)
{
    switch (eng_a) {
        case FIGHT_UNDEAD: return battle_undead();
        case FIGHT_BOSS:   return battle_boss(eng_b);
        case FIGHT_DRAGON: return battle_dragon();
        default:           return battle_wander();
    }
}
