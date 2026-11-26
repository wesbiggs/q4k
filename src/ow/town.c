/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Open world: the town's overlay.
 *
 * Towns and the castle: built the game's way (town_maps.c's maps, each its
 * own size up to TOWN_MAX_W by TOWN_MAX_H, or town_gen()'s, TGEN_W by
 * TGEN_H) into local[] and copied into WORLD, their buildings roofed
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

#include <q4k/isles.h>
#include "engine.h"

/* The villager's sprite frames, after the hero's six (assets/overworld.txt:
   it and its bob), and the robes' colours, one a person. */
#define NPC_FRAME  (64 + 6)
extern const unsigned char king_frame;  /* his own, with a crown, in this
                                           overlay (town_art.s) */
static const unsigned char robes[7] = {
    COLOR_CYAN, COLOR_YELLOW, COLOR_PURPLE, COLOR_BLUE, COLOR_ORANGE,
    COLOR_LIGHTBLUE, COLOR_WHITE
};


static unsigned char npc_ent[7];        /* which of here_ents each sprite is */
static unsigned char npc_frame[7];      /* and its first frame */
static unsigned char bob;               /* the people's frame: 0 or 1 */

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

/* Which region each square is in, 0 for a wall or a door: the biggest town
   with a square of 0 all round, so that a square's eight neighbours are
   always there to look at. It is the overlay's own scratch, in the slot
   after its code (OVTBSS), since BSS has no room for it: lost when another
   overlay loads, and found again by find_buildings() on the way in. */
#define RW       (TOWN_MAX_W + 2)
#define RH       (TOWN_MAX_H + 2)
#define R(y)     (((y) + 1) * RW + 1)
static const unsigned int rowoff[TOWN_MAX_H] = {
    R(0),  R(1),  R(2),  R(3),  R(4),  R(5),  R(6),  R(7),  R(8),  R(9),
    R(10), R(11), R(12), R(13), R(14), R(15), R(16), R(17), R(18), R(19),
    R(20), R(21), R(22), R(23), R(24)
};
#define AT(x, y) (rowoff[y] + (x))

#pragma bss-name (push, "OVTBSS")
static unsigned char region[RW * RH];
#pragma bss-name (pop)

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
   queue is in SHORE, which sm_shores() fills afresh afterwards: room for a
   square each of the biggest town. */
static void flood(unsigned char x, unsigned char y, unsigned char r)
{
    static const signed char fx[4] = { 1, -1, 0, 0 };
    static const signed char fy[4] = { 0, 0, 1, -1 };
    unsigned char *qx = (unsigned char *)0xA000;
    unsigned char *qy = qx + TOWN_MAX_W * TOWN_MAX_H;
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
            if (nx >= local_w || ny >= local_h || region[AT(nx, ny)] ||
                barrier(local[LOCAL_AT(nx, ny)])) {
                continue;
            }
            region[AT(nx, ny)] = r;
            qx[tail] = nx;
            qy[tail++] = ny;
        }
    }
}

static void find_buildings(void)
{
    unsigned char x, y, r = 2, t;
    unsigned int i;

    memset(region, 0, sizeof(region));
    memset(has_door, 0, sizeof(has_door));
    memset(top_y, 0xFF, sizeof(top_y));
    memset(bot_y, 0, sizeof(bot_y));
    memset(left_x, 0xFF, sizeof(left_x));
    memset(right_x, 0, sizeof(right_x));
    flood(hero.x, hero.y, 1);                   /* the outside */
    for (y = 0; y < local_h; ++y) {
        for (x = 0; x < local_w; ++x) {
            if (!region[AT(x, y)] && !barrier(local[LOCAL_AT(x, y)]) &&
                r < MAX_BLD) {
                flood(x, y, r++);
            }
        }
    }
    for (y = 0; y < local_h; ++y) {              /* which have a door */
        for (x = 0; x < local_w; ++x) {
            t = local[LOCAL_AT(x, y)];
            if (!is_door(t)) {
                continue;
            }
            i = AT(x, y);       /* (the border: region 0) */
            has_door[region[i - 1]] = 1;
            has_door[region[i + 1]] = 1;
            has_door[region[i - RW]] = 1;
            has_door[region[i + RW]] = 1;
        }
    }
    has_door[0] = has_door[1] = 0;              /* walls; the outside */
    for (r = 2; r < MAX_BLD; ++r) {
        ridge_y[r] = (top_y[r] + bot_y[r] + 1) / 2;
    }
    inside = 0;
}

static unsigned char roofed(unsigned char r)
{
    return has_door[r] && r != inside;
}

/* The roof over a wall at *p: the first building with a roof on it among
   the eight round it, or 0. A wall between two buildings is under whichever
   of them is roofed. This runs for every wall square a roof change redraws,
   so roofed() is spelled out. */
static unsigned char __fastcall__ wall_roof(const unsigned char *p)
{
    static const signed char round[8] = {
        -RW - 1, -RW, -RW + 1, -1, 1, RW - 1, RW, RW + 1
    };
    unsigned char d, r;

    for (d = 0; d < 8; ++d) {
        r = p[round[d]];
        if (has_door[r] && r != inside) {
            return r;
        }
    }
    return 0;
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
    const unsigned char *p;

    for (y = by0; y <= by1; ++y, w += 64, l += LOCAL_STRIDE) {
        p = region + AT(bx0, y);
        for (x = bx0; x <= bx1; ++x, ++p) {
            t = l[x];
            r = *p;
            if (!r && !is_door(t)) {    /* a wall: a building's roof */
                r = wall_roof(p);
            }
            if (!roofed(r)) {
                w[x] = t;
            } else {
                w[x] = y == ridge_y[r] ? T_RIDGE : T_ROOF;
            }
        }
    }
    st_tchests();               /* (a chest taken, uncovered: open) */
}

/* The people (sm_npcs places them), but for those under a roof: where they
   stand, and a robe each (by who they are, so it stays theirs); the king
   in his crown and gold. */
static void show_people(void)
{
    const TownEnt *e = here_ents;
    unsigned char i, f, n = 0;

    for (i = 0; i < here_n && n < 7; ++i, ++e) {
        if (roofed(region[AT(e->x, e->y)])) {
            continue;
        }
        npc_ent[n] = i;
        sm_npc_x[n] = e->x + BIAS;
        sm_npc_y[n] = e->y + BIAS;
        f = NPC_FRAME;
        sm_npc_col[n] = robes[i % 7];
        if (e->kind == ENT_NPC && e->param == NPC_KING) {
            f = king_frame;
            sm_npc_col[n] = COLOR_YELLOW;
        }
        npc_frame[n] = f;
        sm_npc_ptr[n] = f + bob;        /* (t_idle() bobs them) */
        ++n;
    }
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
    a = right_x[r] + 2 >= local_w ? local_w - 1 : right_x[r] + 2;
    if (a > bx1) bx1 = a;
    a = bot_y[r] + 2 >= local_h ? local_h - 1 : bot_y[r] + 2;
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
    if (r1 >= sm_rows) r1 = sm_rows - 1;  /* (the map's rows: the text
                                             window may be below) */
    if (c0 > c1 || r0 > r1) {
        c0 = c1 = r0 = r1 = 0;
    }
    if (whole) {
        c0 = r0 = 0;
        c1 = 39;
        r1 = sm_rows - 1;
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
        if (nx >= local_w || ny >= local_h) {
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
        bx1 = local_w - 1;
        by1 = local_h - 1;
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
    unsigned char *src;
    unsigned int n;
    unsigned char y, index = town_index;

    area = index == TOWN_CASTLE ? AREA_CASTLE : AREA_TOWN;
    play_for(area == AREA_CASTLE ? MUSIC_CASTLE : MUSIC_TOWN);
    if (index < NUM_TOWNS) {
        /* Unpacked into the end of local[] and moved up into its rows,
           which never overtake what is still to move: 64 * h + w <= 2112,
           true for anything up to 40 by 25. */
        local_w = town_w[index];
        local_h = town_h[index];
        n = (unsigned int)local_w * local_h;
        src = local + sizeof(local) - n;
        rle_unpack(src, town_map[index], n);
        for (y = 0; y < local_h; ++y, src += local_w) {
            memmove(local + LOCAL_AT(0, y), src, local_w);
        }
        here_ents = town_ents + town_ent_first[index];
        here_n    = town_ent_first[index + 1] - town_ent_first[index];
        hero.x    = town_sx[index];
        hero.y    = town_sy[index];
    } else {
        /* An island's (q4k/isles.h) is made as if its land were one of its
           own, ISL_LAND + its cell, of a ground of its own. */
        y = land;
        if (index > TOWN_PROC) {
            land = index - (TOWN_PROC + 1) + ISL_LAND;
            isle_biome = land % 5;
        }
        town_gen();             /* the people, the way out and where to stand */
        if (!resuming) {
            entering = town_gen_name();
        }
        land = y;
    }
    build_area(area == AREA_CASTLE ? ts_castle : ts_town, local_w, local_h);
    find_buildings();           /* (from the gate: the outside is region 1) */
    if (resuming) {             /* back from a talk: where they stood, ... */
        resuming = 0;
        hero.x = resume_x;
        hero.y = resume_y;
        inside = building_at(hero.x, hero.y);
        if (inside == NONE) {
            inside = 0;
        }                       /* ... and the roof off what they stand in */
    } else {                    /* in at the gate: said (the core's) */
        if (index < NUM_TOWNS) {
            entering = town_names[index];
        }
    }
    bx0 = by0 = 0;
    bx1 = local_w - 1;
    by1 = local_h - 1;
    put_roofs();
    show_people();
    show_area(hero.x + BIAS, hero.y + BIAS);
}

/* Whether a step that way can be taken: as anywhere in a place, except
   out of Millford, by any way out, before Garrett has armed the party (the
   game's rule: nobody leaves without a weapon). */
static unsigned char gate_shut(unsigned char d)
{
    return quest_stage == Q_START && town_index == TOWN_MILLFORD &&
        tile_at(hx + dirdx[d], hy + dirdy[d]) == T_EXIT;
}

unsigned char __fastcall__ t_can_step(unsigned char d)
{
    return !gate_shut(d) && place_step(d);  /* (t_bump says why not) */
}

/* At the end of a step: out by a way out (any exit tile), to the world's
   overlay, or in or out of a building. */
void t_arrived(void)
{
    if (tile_at(hx, hy) == T_EXIT) {
        leave_area();
        ov_want = OV_WORLD;
    } else {
        roof_check();
    }
}

/* Every frame (ov_idle): every sixteenth, the people shift their feet, as
   everyone does in a fight. */
void t_idle(void)
{
    unsigned char i;

    if ((frame & 15) || still) {
        return;
    }
    bob ^= 1;
    for (i = sm_npc_n; i--; ) {
        sm_npc_ptr[i] = npc_frame[i] + bob;
    }
    sm_mxdirty = 1;             /* (the interrupt writes them) */
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
   inn. The engine goes; src/owtalk.c has the conversation, src/owtown.c
   the shop's and the inn's screens,
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
    ow_leave(e->kind == ENT_NPC ? owtalk : owtown);
}

#pragma rodata-name (push, "OVTCODE")
static const char unarmed[] = "Garrett would see you armed first.";
#pragma rodata-name (pop)

/* A step into someone: a conversation, a shop or an inn (a villager is a
   wall); into Millford's gate too soon, a refusal. */
void __fastcall__ t_bump(unsigned char d)
{
    unsigned char i, x = hx + dirdx[d], y = hy + dirdy[d];

    if (gate_shut(d)) {         /* (not any way out that is merely blocked) */
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
