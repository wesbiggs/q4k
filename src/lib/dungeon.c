#include <stdlib.h>
#include <string.h>

#include <q4k/dungeon.h>
#include <q4k/game.h>
#include <q4k/tiles.h>

#define MIN_W     3
#define MIN_H     3

unsigned char dun_up_x, dun_up_y;
unsigned char dun_dn_x, dun_dn_y;

unsigned char dun_rooms;
unsigned char dun_rx[MAX_ROOMS], dun_ry[MAX_ROOMS];
unsigned char dun_rw[MAX_ROOMS], dun_rh[MAX_ROOMS];

#define rx dun_rx
#define ry dun_ry
#define rw dun_rw
#define rh dun_rh

static void carve(unsigned char x, unsigned char y)
{
    if (x < LOCAL_W && y < LOCAL_H) {
        local[(unsigned int)y * LOCAL_W + x] = T_FLOOR;
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

void dungeon_build(unsigned char index, unsigned char level)
{
    unsigned char rooms = 0;
    unsigned char tries, x, y, w, h, i;

    /* Seeded from where you are and which game this is, not from the
       clock: leave a level and come back and it is the same level, and a
       restored game brings its lore_seed back with it -- but a new game
       gets new dungeons. */
    srand((unsigned int)index * 977 + (unsigned int)level * 131 + 17
          + (unsigned int)lore_seed * 7919);

    memset(local, T_WALL, sizeof(local));

    for (tries = 0; tries < 60 && rooms < MAX_ROOMS; ++tries) {
        w = MIN_W + (unsigned char)(rand() % 4);
        h = MIN_H + (unsigned char)(rand() % 3);
        x = 1 + (unsigned char)(rand() % (LOCAL_W - w - 2));
        y = 1 + (unsigned char)(rand() % (LOCAL_H - h - 2));

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

    for (i = 1; i < rooms; ++i) {
        corridor(rx[i - 1] + rw[i - 1] / 2, ry[i - 1] + rh[i - 1] / 2,
                 rx[i] + rw[i] / 2, ry[i] + rh[i] / 2);
    }

    dun_up_x = rx[0] + rw[0] / 2;
    dun_up_y = ry[0] + rh[0] / 2;
    local[(unsigned int)dun_up_y * LOCAL_W + dun_up_x] = T_STAIRUP;

    i = rooms - 1;
    dun_dn_x = rx[i] + rw[i] / 2;
    dun_dn_y = ry[i] + rh[i] / 2;

    if (level >= dungeon_deep[index]) {
        /* The bottom: the keeper stands where the stairs would have been --
           unless it has already been killed, which is what holding its stone
           means. Levels are rebuilt from scratch every visit, so without this
           the keeper comes back. */
        local[(unsigned int)dun_dn_y * LOCAL_W + dun_dn_x] =
            has(dungeon_prize[index]) ? T_FLOOR : T_MONSTER;
    } else {
        local[(unsigned int)dun_dn_y * LOCAL_W + dun_dn_x] = T_STAIRDN;
    }
}
