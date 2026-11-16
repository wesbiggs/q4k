/*
 * Open world: the town's overlay.
 *
 * Towns and the castle: built the game's way (town_data.c's maps, or
 * town_gen()) into local[] and copied into WORLD, their buildings roofed
 * until the hero goes in, their people sprites; and walking into one of them
 * who has lines is a conversation (src/owtown.c, loaded over the engine).
 * Loaded into the slot over the world's when the hero steps onto a town
 * (world.c's to_town()), or at the start when a talk is over; its way out
 * asks for the world's back (engine.h). Built with --code-name OVTCODE.
 */

#include <c64.h>
#include <peekpoke.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/isle.h>
#include <q4k/loot_data.h>
#include <q4k/music.h>
#include <q4k/music_map.h>
#include <q4k/ow.h>
#include <q4k/rle.h>
#include <q4k/tiles.h>
#include <q4k/town_data.h>
#include <q4k/town_gen.h>

#include "engine.h"

/* The villager's sprite frame, after the hero's eight (assets/overworld.txt),
   and the robes' colours, one a person. */
#define NPC_FRAME  (64 + 8)
static const unsigned char robes[7] = {
    COLOR_CYAN, COLOR_YELLOW, COLOR_PURPLE, COLOR_BLUE, COLOR_ORANGE,
    COLOR_LIGHTBLUE, COLOR_WHITE
};


static unsigned char ex, ey;            /* the way out */
static unsigned char npc_ent[7];        /* which of here_ents each sprite is */

/* --- Roofs -------------------------------------------------------------

   In a town or the castle, a building you are not in shows only its roof,
   as in Ultima VI. A building is a closed area with a door: found once on
   the way in, by filling from where you stand (the outside, region 1)
   through everything that is not a wall or a door, then again from each
   square not yet reached. Its roof covers it and the walls round it, with a
   ridge along its middle row, and hides the people in it; the doors stay.
   Stepping into its doorway (or onto its floor) takes its roof off and puts
   back whichever came off before, drawn into the hidden screen and flipped
   to, so it changes between one frame and the next. A door between two
   rooms keeps open the one already open. What you can walk on is read from local[],
   which has no roofs. */

#define T_ROOF   0x92           /* the tile sets' own codes (mkoverworld.py) */
#define T_RIDGE  0x93
#define MAX_BLD  32
#define AT(x, y) ((unsigned int)(y) * LOCAL_W + (x))

static unsigned char region[LOCAL_W * LOCAL_H];        /* 0 a wall or door */
/* For a wall, the buildings beside it (up to two, 0 for none): it is under
   a roof if either is. Worked out once, so that a roof changing is a pass
   over the town and no more. */
static unsigned char owner1[LOCAL_W * LOCAL_H], owner2[LOCAL_W * LOCAL_H];
static unsigned char has_door[MAX_BLD], ridge_y[MAX_BLD];
static unsigned char top_y[MAX_BLD], bot_y[MAX_BLD];
static unsigned char left_x[MAX_BLD], right_x[MAX_BLD];
static unsigned char inside;    /* the building you are in, or 0 */

static unsigned char is_door(unsigned char t)
{
    return t == T_DOOR || t == T_LOCKEDDOOR;
}

static unsigned char barrier(unsigned char t)
{
    return t == T_WALL || t == T_SECRETDOOR || is_door(t);
}

/* Everything joined to (x, y) that is not a wall or a door, as `r`. The
   queue is in SHORE, which sm_shores() fills afresh afterwards. */
static void flood(unsigned char x, unsigned char y, unsigned char r)
{
    static const signed char fx[4] = { 1, -1, 0, 0 };
    static const signed char fy[4] = { 0, 0, 1, -1 };
    unsigned char *qx = (unsigned char *)0xA000, *qy = qx + 512;
    unsigned int head = 0, tail = 0;
    unsigned char d, nx, ny;

    region[AT(x, y)] = r;
    qx[tail] = x;
    qy[tail++] = y;
    while (head < tail) {
        x = qx[head];
        y = qy[head++];
        if (y < top_y[r]) top_y[r] = y;
        if (y > bot_y[r]) bot_y[r] = y;
        if (x < left_x[r]) left_x[r] = x;
        if (x > right_x[r]) right_x[r] = x;
        for (d = 0; d < 4; ++d) {
            nx = x + fx[d];
            ny = y + fy[d];
            if (nx >= LOCAL_W || ny >= LOCAL_H || region[AT(nx, ny)] ||
                barrier(local[LOCAL_AT(nx, ny)])) {
                continue;
            }
            region[AT(nx, ny)] = r;
            qx[tail] = nx;
            qy[tail++] = ny;
        }
    }
}

/* Each wall's buildings: those with a door among the eight round it. */
static void find_owners(void)
{
    unsigned char x, y, dx, dy, nx, ny, r;
    unsigned int i = 0;

    for (y = 0; y < LOCAL_H; ++y) {
        for (x = 0; x < LOCAL_W; ++x, ++i) {
            owner1[i] = owner2[i] = 0;
            if (region[i] || is_door(local[LOCAL_AT(x, y)])) {
                continue;
            }
            for (dy = 0; dy < 3; ++dy) {
                for (dx = 0; dx < 3; ++dx) {
                    nx = x + dx - 1;
                    ny = y + dy - 1;
                    if (nx >= LOCAL_W || ny >= LOCAL_H) {
                        continue;
                    }
                    r = region[AT(nx, ny)];
                    if (!has_door[r] || r == owner1[i]) {
                        continue;
                    }
                    if (!owner1[i]) {
                        owner1[i] = r;
                    } else {
                        owner2[i] = r;
                    }
                }
            }
        }
    }
}

static void find_buildings(void)
{
    unsigned char x, y, r = 2, t;

    memset(region, 0, sizeof(region));
    memset(has_door, 0, sizeof(has_door));
    memset(top_y, 0xFF, sizeof(top_y));
    memset(bot_y, 0, sizeof(bot_y));
    memset(left_x, 0xFF, sizeof(left_x));
    memset(right_x, 0, sizeof(right_x));
    flood(hero.x, hero.y, 1);                   /* the outside */
    for (y = 0; y < LOCAL_H; ++y) {
        for (x = 0; x < LOCAL_W; ++x) {
            if (!region[AT(x, y)] && !barrier(local[LOCAL_AT(x, y)]) &&
                r < MAX_BLD) {
                flood(x, y, r++);
            }
        }
    }
    for (y = 0; y < LOCAL_H; ++y) {              /* which have a door */
        for (x = 0; x < LOCAL_W; ++x) {
            t = local[LOCAL_AT(x, y)];
            if (!is_door(t)) {
                continue;
            }
            if (x > 0)            has_door[region[AT(x - 1, y)]] = 1;
            if (x + 1 < LOCAL_W)  has_door[region[AT(x + 1, y)]] = 1;
            if (y > 0)            has_door[region[AT(x, y - 1)]] = 1;
            if (y + 1 < LOCAL_H)  has_door[region[AT(x, y + 1)]] = 1;
        }
    }
    has_door[0] = has_door[1] = 0;              /* walls; the outside */
    for (r = 2; r < MAX_BLD; ++r) {
        ridge_y[r] = (top_y[r] + bot_y[r] + 1) / 2;
    }
    inside = 0;
    find_owners();
}

static unsigned char roofed(unsigned char r)
{
    return has_door[r] && r != inside;
}

/* WORLD from local[], with a roof on every building you are not in: the
   squares bx0-bx1 by by0-by1 of it (the whole town, or where a roof came
   off and went on). */
static unsigned char bx0, by0, bx1, by1;

static void put_roofs(void)
{
    unsigned char x, y, t, r;
    unsigned char *w = WORLD + (unsigned int)by0 * 64;
    unsigned char *l = local + LOCAL_AT(0, by0);
    unsigned int i;

    for (y = by0; y <= by1; ++y, w += 64, l += LOCAL_STRIDE) {
        i = AT(bx0, y);
        for (x = bx0; x <= bx1; ++x, ++i) {
            t = l[x];
            r = region[i];
            if (!r) {                   /* a wall: either building's roof */
                r = owner1[i];
                if (!roofed(r)) {
                    r = owner2[i];
                }
            }
            if (!roofed(r)) {
                w[x] = t;
            } else {
                w[x] = y == ridge_y[r] ? T_RIDGE : T_ROOF;
            }
        }
    }
}

/* The people, as sprites 1 to 7, but for those under a roof: where they
   stand, and a robe each (by who they are, so it stays theirs). */
static void show_people(void)
{
    const TownEnt *e = here_ents;
    unsigned char i, n = 0;

    for (i = 0; i < here_n && n < 7; ++i, ++e) {
        if (roofed(region[AT(e->x, e->y)])) {
            continue;
        }
        npc_ent[n] = i;
        sm_npc_x[n] = e->x + BIAS;
        sm_npc_y[n] = e->y + BIAS;
        POKE(0x43F9 + n, NPC_FRAME);    /* both screens' sprite pointers */
        POKE(0x47F9 + n, NPC_FRAME);
        POKE(0xD028 + n, robes[i % 7]);
        ++n;
    }
    VIC.spr_mcolor = 0xFF;
    sm_npc_n = n;
}

/* The box of squares building r decides, grown to take it in: the
   building, its walls, and a square more. */
static void grow_box(unsigned char r)
{
    unsigned char a;

    if (!r) {
        return;
    }
    a = left_x[r] < 2 ? 0 : left_x[r] - 2;
    if (a < bx0) bx0 = a;
    a = top_y[r] < 2 ? 0 : top_y[r] - 2;
    if (a < by0) by0 = a;
    a = right_x[r] + 2 >= LOCAL_W ? LOCAL_W - 1 : right_x[r] + 2;
    if (a > bx1) bx1 = a;
    a = bot_y[r] + 2 >= LOCAL_H ? LOCAL_H - 1 : bot_y[r] + 2;
    if (a > by1) by1 = a;
}

/* That box in characters on the screen, clipped to it (if none of it is
   on the screen, a corner of the screen: the copy is all that matters). */
static void box_on_screen(void)
{
    int c0 = (bx0 + BIAS) * 2 - sm_ox, c1 = (bx1 + BIAS) * 2 + 1 - sm_ox;
    int r0 = (by0 + BIAS) * 2 - sm_oy, r1 = (by1 + BIAS) * 2 + 1 - sm_oy;

    if (c0 < 0) c0 = 0;
    if (r0 < 0) r0 = 0;
    if (c1 > 39) c1 = 39;
    if (r1 > 21) r1 = 21;       /* (the map's rows: the text is below) */
    if (c0 > c1 || r0 > r1) {
        c0 = c1 = r0 = r1 = 0;
    }
    if (whole) {
        c0 = r0 = 0;
        c1 = 39;
        r1 = 21;
    }
    sm_box_c0 = c0;
    sm_box_c1 = c1;
    sm_box_r0 = r0;
    sm_box_r1 = r1;
}

/* The building a door at (x, y) opens into: one beside it with a roof, or
   the one already open if it joins two (a door between rooms); 0 if none. */
static unsigned char door_opens(unsigned char x, unsigned char y)
{
    static const signed char dx[4] = { 1, -1, 0, 0 };
    static const signed char dy[4] = { 0, 0, 1, -1 };
    unsigned char d, nx, ny, r, found = 0;

    for (d = 0; d < 4; ++d) {
        nx = x + dx[d];
        ny = y + dy[d];
        if (nx >= LOCAL_W || ny >= LOCAL_H) {
            continue;
        }
        r = region[AT(nx, ny)];
        if (!has_door[r]) {
            continue;
        }
        if (r == inside) {
            return r;
        }
        if (!found) {
            found = r;
        }
    }
    return found;
}

/* The building the hero stands in at (x, y) -- on its floor or in its
   doorway -- or 0 outside; or NONE where that cannot be told (a wall, or a
   door to nothing with a roof), when whatever is open stays open. */
static unsigned char building_at(unsigned char x, unsigned char y)
{
    unsigned char r = region[AT(x, y)];

    if (!r) {
        if (!is_door(local[LOCAL_AT(x, y)])) {
            return NONE;        /* (a wall: never stood on) */
        }
        r = door_opens(x, y);   /* standing in a doorway opens the building */
        if (!r) {
            return NONE;
        }
    }
    return has_door[r] ? r : 0; /* 0: outside, or nowhere with a roof */
}

/* At the end of a step in a town: in or out of a building? */
static void roof_check(void)
{
    unsigned char r = building_at(hx - BIAS, hy - BIAS);

    if (r == NONE || r == inside) {
        return;
    }
    roof_time = PEEK(0xA2);
    /* What changes: the building you left and the one you went into, with
       their walls and a square more, whose edges they decide. */
    bx0 = by0 = 0xFF;
    bx1 = by1 = 0;
    grow_box(inside);
    grow_box(r);
    if (whole) {
        bx0 = by0 = 0;
        bx1 = LOCAL_W - 1;
        by1 = LOCAL_H - 1;
    }
    inside = r;
    put_roofs();
    sm_sh_x0 = bx0;             /* their edges ... */
    sm_sh_y0 = by0;
    sm_sh_x1 = bx1;
    sm_sh_y1 = by1;
    sm_shores_rect();
    box_on_screen();            /* ... and their characters, into the hidden */
    sm_redraw_box();            /* screen, the rest copied from the front */
    pre_dir = NONE;             /* (over any shift made for the next step) */
    show_people();
    sm_npcs();
    sm_commit(1);               /* ... and flipped to at the next frame */
    next_frame();
    roof_time = PEEK(0xA2) - roof_time;
    frame = sm_frame;
}

/* --- In and out ----------------------------------------------------------- */

/* Just loaded into the slot: town_index, with the hero at its gate, or, back
   from a talk, where they stood (resuming). The screen is off. */
void t_enter(void)
{
    unsigned char *src = local + sizeof(local) - LOCAL_W * LOCAL_H;
    unsigned char y, index = town_index;

    area = index == TOWN_CASTLE ? AREA_CASTLE : AREA_TOWN;
    play_for(area == AREA_CASTLE ? MUSIC_CASTLE : MUSIC_TOWN);
    if (index < NUM_TOWNS) {
        rle_unpack(src, town_map[index], LOCAL_W * LOCAL_H);
        for (y = 0; y < LOCAL_H; ++y, src += LOCAL_W) {
            memmove(local + LOCAL_AT(0, y), src, LOCAL_W);
        }
        here_ents = town_ents + town_ent_first[index];
        here_n    = town_ent_first[index + 1] - town_ent_first[index];
        here_ex   = town_ex[index];
        here_ey   = town_ey[index];
        hero.x    = town_sx[index];
        hero.y    = town_sy[index];
    } else {
        town_gen();             /* the people, the way out and where to stand */
    }
    build_area(area == AREA_CASTLE ? ts_castle : ts_town, LOCAL_W, LOCAL_H);
    find_buildings();           /* (from the gate: the outside is region 1) */
    ex = here_ex;
    ey = here_ey;
    if (resuming) {             /* back from a talk: where they stood, ... */
        resuming = 0;
        hero.x = resume_x;
        hero.y = resume_y;
        inside = building_at(hero.x, hero.y);
        if (inside == NONE) {
            inside = 0;
        }                       /* ... and the roof off what they stand in */
    }
    bx0 = by0 = 0;
    bx1 = LOCAL_W - 1;
    by1 = LOCAL_H - 1;
    put_roofs();
    show_people();
    show_area(hero.x + BIAS, hero.y + BIAS);
}

/* Whether a step that way can be taken: as anywhere in a place, except
   out of Millford before Garrett has armed the party (the game's rule:
   nobody leaves without a weapon). */
unsigned char __fastcall__ t_can_step(unsigned char d)
{
    if (quest_stage == Q_START && town_index == TOWN_MILLFORD &&
        (unsigned char)(hx + dirdx[d] - BIAS) == ex &&
        (unsigned char)(hy + dirdy[d] - BIAS) == ey) {
        return 0;               /* (t_bump says no) */
    }
    return place_step(d);
}

/* At the end of a step: out by the way out, to the world's overlay, or in
   or out of a building. */
void t_arrived(void)
{
    if (hx - BIAS == ex && hy - BIAS == ey) {
        leave_area();
        ov_want = OV_WORLD;
    } else {
        roof_check();
    }
}

/* L: the town's hidden gold (assets/loot.txt, the game's), on the hero's
   square, once; anywhere else nothing, and no dead rise in a town. */
void t_look(void)
{
    const Loot *l = loot_table;
    unsigned char i, x = hx - BIAS, y = hy - BIAS;

    for (i = 0; i < NUM_LOOT; ++i, ++l) {
        if (l->map == town_index && l->x == x && l->y == y) {
            if (take_bit(DUN_LOOT + i)) {
                loot_found(l->kind == LOOT_DRAUGHT, l->amount);
            }
            return;
        }
    }
    look_nothing(NONE);
}

/* Talk, or trade, with sprite n's person: a conversation, a shop or an
   inn. The engine goes; src/owtown.c has the screens,
   and loads the engine back when it ends, which comes straight back here
   (engine.c's main()). Never returns. */
static void talk_to(unsigned char n)
{
    const TownEnt *e = &here_ents[npc_ent[n]];

    switch (e->kind) {
    case ENT_NPC:  OWB->mode = OWB_TALK; break;
    case ENT_SHOP: OWB->mode = OWB_SHOP; break;
    case ENT_INN:  OWB->mode = OWB_INN;  break;
    default:       return;      /* a villager is a wall */
    }
    OWB->npc = OWB->arg = e->param;
    ow_leave(owtown);
}

#pragma rodata-name (push, "OVTCODE")
static const char unarmed[] = "Garrett would see you armed first.";
#pragma rodata-name (pop)

/* A step into someone: a conversation, a shop or an inn (a villager is a
   wall); into Millford's gate too soon, a refusal. */
void __fastcall__ t_bump(unsigned char d)
{
    unsigned char i, x = hx + dirdx[d], y = hy + dirdy[d];

    if (x - BIAS == ex && y - BIAS == ey) {
        refuse(unarmed);
        return;
    }
    for (i = 0; i < sm_npc_n; ++i) {
        if (sm_npc_x[i] == x && sm_npc_y[i] == y) {
            talk_to(i);
            return;
        }
    }
}
