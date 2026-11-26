/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdlib.h>
#include <string.h>

#include <q4k/dungeon.h>
#include <q4k/game.h>
#include <q4k/isles.h>
#include <q4k/tiles.h>

#define MIN_W     3
#define MIN_H     3

unsigned char dun_up_x, dun_up_y;
unsigned char dun_dn_x, dun_dn_y;

unsigned char dun_chests;
unsigned char dun_cx[DUN_CHESTS], dun_cy[DUN_CHESTS];

unsigned char dun_rooms;
unsigned char dun_rx[MAX_ROOMS], dun_ry[MAX_ROOMS];
unsigned char dun_rw[MAX_ROOMS], dun_rh[MAX_ROOMS];

#define rx dun_rx
#define ry dun_ry
#define rw dun_rw
#define rh dun_rh

static void carve(unsigned char x, unsigned char y)
{
    if (x < DUN_W && y < DUN_H) {
        local[LOCAL_AT(x, y)] = T_FLOOR;
    }
}

static void carve_room(unsigned char i)
{
    unsigned char x, y;

    for (y = 0; y < rh[i]; ++y) {
        for (x = 0; x < rw[i]; ++x) {
            carve(rx[i] + x, ry[i] + y);
        }
    }
}

/* An L-shaped corridor: along x, then along y. */
static void corridor(unsigned char x1, unsigned char y1,
                     unsigned char x2, unsigned char y2)
{
    while (x1 != x2) {
        carve(x1, y1);
        x1 += (x1 < x2) ? 1 : -1;
    }
    while (y1 != y2) {
        carve(x1, y1);
        y1 += (y1 < y2) ? 1 : -1;
    }
    carve(x1, y1);
}

static unsigned char overlaps(unsigned char n, unsigned char x, unsigned char y,
                              unsigned char w, unsigned char h)
{
    unsigned char i;

    /* One cell of slack on every side, so rooms never share a wall. */
    for (i = 0; i < n; ++i) {
        if (x < rx[i] + rw[i] + 1 && x + w + 1 > rx[i] &&
            y < ry[i] + rh[i] + 1 && y + h + 1 > ry[i]) {
            return 1;
        }
    }
    return 0;
}

/* How far apart two rooms' middles are, walking the corridors' way. */
static unsigned char apart(unsigned char i, unsigned char j)
{
    unsigned char ax = rx[i] + rw[i] / 2, bx = rx[j] + rw[j] / 2;
    unsigned char ay = ry[i] + rh[i] / 2, by = ry[j] + rh[j] / 2;

    return (ax > bx ? ax - bx : bx - ax) + (ay > by ? ay - by : by - ay);
}

void dungeon_build(unsigned char index, unsigned char level)
{
    unsigned char rooms = 0;
    unsigned char tries, x, y, w, h, i, j, n, best, d;

    /* Seeded from where you are and which game this is, not from the
       clock: leave a level and come back and it is the same level, and a
       restored game brings its lore_seed back with it -- but a new game
       gets new dungeons. */
    srand((unsigned int)index * 977 + (unsigned int)level * 131 + 17
          + (unsigned int)lore_seed * 7919);

    memset(local, T_WALL, sizeof(local));
    local_w = DUN_W;
    local_h = DUN_H;

    for (tries = 0; tries < 150 && rooms < MAX_ROOMS; ++tries) {
        w = MIN_W + (unsigned char)(rand() % 6);
        h = MIN_H + (unsigned char)(rand() % 4);
        x = 1 + (unsigned char)(rand() % (DUN_W - w - 2));
        y = 1 + (unsigned char)(rand() % (DUN_H - h - 2));

        if (overlaps(rooms, x, y, w, h)) {
            continue;
        }
        rx[rooms] = x;
        ry[rooms] = y;
        rw[rooms] = w;
        rh[rooms] = h;
        carve_room(rooms);
        ++rooms;
    }

    dun_rooms = rooms;

    /* Each room joins the nearest one placed before it. Chaining them in
       the order they were placed was fine on a single screen, but on a level
       this size it runs corridors back and forth across the whole map. */
    for (i = 1; i < rooms; ++i) {
        best = 0;
        for (j = 1; j < i; ++j) {
            if (apart(i, j) < apart(i, best)) {
                best = j;
            }
        }
        corridor(rx[best] + rw[best] / 2, ry[best] + rh[best] / 2,
                 rx[i] + rw[i] / 2, ry[i] + rh[i] / 2);
    }

    dun_up_x = rx[0] + rw[0] / 2;
    dun_up_y = ry[0] + rh[0] / 2;
    local[LOCAL_AT(dun_up_x, dun_up_y)] = T_STAIRUP;

    /* The way down is in the room furthest from the way up, so a level is
       crossed rather than stepped through. */
    i = rooms - 1;
    best = 0;
    for (j = 1; j < rooms; ++j) {
        d = apart(0, j);
        if (d > best) {
            best = d;
            i = j;
        }
    }
    dun_dn_x = rx[i] + rw[i] / 2;
    dun_dn_y = ry[i] + rh[i] / 2;

    if (index >= NUM_DUNGEONS) {
        /* A sea cave (q4k/isles.h): its levels, nobody at the bottom. */
        local[LOCAL_AT(dun_dn_x, dun_dn_y)] =
            level >= ISL_DEEP ? T_FLOOR : T_STAIRDN;
    } else if (level >= dungeon_deep[index]) {
        /* The bottom: the keeper stands where the stairs would have been --
           unless it has already been killed, which is what holding its stone
           means. Levels are rebuilt from scratch every visit, so without this
           the keeper comes back. */
        local[LOCAL_AT(dun_dn_x, dun_dn_y)] =
            has(dungeon_prize[index]) ? T_FLOOR : T_MONSTER;
    } else {
        local[LOCAL_AT(dun_dn_x, dun_dn_y)] = T_STAIRDN;
    }

    /* One to three chests, so a level is always worth exploring past its
       stairs. Each one is looted by index, so the draws must not depend on
       anything that changes between visits: the stairs are tested by
       position, not tile, because the keeper's square turns to floor once
       it is dead and a chest could otherwise land there on the next visit
       only. A chest placed earlier this build is the same on every visit,
       so testing for its tile is safe. */
    dun_chests = 0;
    if (index >= NUM_DUNGEONS) {
        return;                 /* (a sea cave's: no bits to keep them) */
    }
    n = 1 + (unsigned char)(rand() % DUN_CHESTS);
    for (tries = 0; tries < 24 && dun_chests < n; ++tries) {
        i = (unsigned char)(rand() % rooms);
        x = rx[i] + (unsigned char)(rand() % rw[i]);
        y = ry[i] + (unsigned char)(rand() % rh[i]);
        if ((x == dun_up_x && y == dun_up_y)
            || (x == dun_dn_x && y == dun_dn_y)
            || local[LOCAL_AT(x, y)] == T_CHEST) {
            continue;
        }
        dun_cx[dun_chests] = x;
        dun_cy[dun_chests] = y;
        local[LOCAL_AT(x, y)] = T_CHEST;
        ++dun_chests;
    }
}
