/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * The open world's spoils and treasure maps, for owtown (src/owtown.c).
 *
 * A fight won (src/lib/ow_fight.c) may leave something behind, as the
 * game's won() had it: now and then arms, from what goes armed, and out on
 * the land now and then a treasure map. The fight module was loaded over
 * the shop's code that hands arms out, so owtown loads itself again for
 * them (OWB_SPOILS: ow_back()), and ow_spoils() is that screen.
 *
 * A map is a square of the land the fight was on (the game block's tmap[]:
 * owgame.h), anywhere plain the party could walk, which L there digs up
 * (the engine's: src/ow/world.c's w_look()). The square is chosen from the
 * land itself: home is in the RAM under the I/O area and the continent the
 * party is on in the RAM under the KERNAL, where the engine keeps it
 * (vault.s), and both are still there in owtown. The party screen's M lists
 * the maps carried, each by the place of its land nearest the square, as
 * the game's treasure_maps() did ("12 leagues west, 3 north of Millford").
 *
 * In HIGH (the Makefile's --code-name), which a fight overwrites: none of
 * this runs after one in the same load.
 */

#include <conio.h>
#include <c64.h>
#include <stdlib.h>
#include <string.h>

#include <q4k/actor.h>
#include <q4k/game.h>
#include <q4k/isle.h>
#include <q4k/ow.h>
#include <q4k/owgame.h>
#include <q4k/shop.h>
#include <q4k/tiles.h>
#include <q4k/town_data.h>
#include <q4k/town_gen.h>
#include <q4k/vault.h>

#define NONE      0xFF
#define ISLE_TOP  16            /* a continent's first row in its cell
                                   (src/ow/world.c) */
#define HOME_AT   0xD000        /* vault.s: home, 64 by 64 */
#define KEPT_AT   0xE000        /* and the continent kept, 64 by 31 */

static char msg[41];

/* --- a map ---------------------------------------------------------------- */

/* The hero's square on their land, from the world's. */
static unsigned char here_x, here_y;

static void here(void)
{
    here_x = (unsigned char)G->wx & 63;
    here_y = (G->wy & 63) - (G->land == ISLE_HOME ? 0 : ISLE_TOP);
}

/* A map of the land the party is on, into the first free hand: a square
   the party could walk onto and that is nothing else (no place, no road,
   not the hero's own), tried at random as the game's treasure_drop() did.
   Returns 0 if there was no room or no square came up. */
static unsigned char map_drop(void)
{
    unsigned char i, n, x, y, t, l = G->land;
    unsigned char h = l == ISLE_HOME ? 64 : ISLE_H;

    for (i = 0; i < OWG_MAPS && G->tmap[i]; ++i) {
    }
    if (i == OWG_MAPS) {
        return 0;
    }
    here();
    for (n = 0; n < 40; ++n) {
        x = (unsigned char)rand() & 63;
        y = (unsigned char)(rand() % h);
        t = vault_peek((l == ISLE_HOME ? HOME_AT : KEPT_AT)
                       + ((unsigned int)y << 6) + x);
        if ((t == T_GRASS || t == T_FOREST || t == T_SHORE || t == T_SWAMP)
            && (x != here_x || y != here_y)) {
            G->tmap[i] = OWG_TMAP(l, x, y);
            return 1;
        }
    }
    return 0;
}

/* What the foe left (OWB->arg's OWS_*), with OWB->npc the roll for the
   arms: a screen of its own, after the fight's. */
void ow_spoils(void)
{
    unsigned char y = 4;

    textcolor(COLOR_YELLOW);
    cputsxy(1, 2, "Among what it left:");
    if (OWB->arg & OWS_GEAR) {
        y = gear_drop(OWB->npc, y);
    }
    if ((OWB->arg & OWS_MAP) && map_drop()) {
        textcolor(COLOR_LIGHTGREEN);
        cputsxy(1, y, "A treasure map!");
        textcolor(COLOR_GRAY3);
        cputsxy(1, y + 1, "I, then M, to read it; L to dig.");
    }
    textcolor(COLOR_GRAY2);
    cputsxy(1, 22, "Press a key.");
    cgetc();
}

/* --- the maps carried -------------------------------------------------- */

static const char *const compass[4] = { "north", "east", "south", "west" };

/* Land l's place nearest (x, y), by the longer way across or down, and
   its square; and its name. Home's are Millford, the castle, the Hollow
   Crown, Maldrath's lair and a town (isle.h's order); a continent's its
   story place and a town. */
static unsigned char near_x, near_y;

static const char *nearest(unsigned char l, unsigned char x, unsigned char y)
{
    unsigned char i, n, d, dx, dy, px, py, best = 255, at = 0, was;
    const char *name;

    n = l == ISLE_HOME ? OW_PLACES : 2;
    for (i = 0; i < n; ++i) {
        px = l == ISLE_HOME ? OW->home_fx[i] : OW->ipx[l][i];
        py = l == ISLE_HOME ? OW->home_fy[i] : OW->ipy[l][i];
        dx = px > x ? px - x : x - px;
        dy = py > y ? py - y : y - py;
        d = dx > dy ? dx : dy;
        if (d < best) {
            best = d;
            at = i;
            near_x = px;
            near_y = py;
        }
    }
    if (l == ISLE_HOME) {
        switch (at) {
        case 0: return town_names[TOWN_MILLFORD];
        case 1: return town_names[TOWN_CASTLE];
        case 2: return "the Hollow Crown";
        case 3: return "Maldrath's lair";
        }
    } else if (!at) {
        d = OW->slot[l];        /* which story place (slot.c) */
        if (d & 1) {
            return d >> 1 ? "Blackpit" : "the Drowned Stair";
        }
        return town_names[TOWN_WARDLOW + (d >> 1)];
    }
    was = land;
    land = l;                   /* town_gen_name() reads it */
    name = town_gen_name();
    land = was;
    return name;
}

/* " 18 leagues west" and the like, onto the end of msg; `more` leaves out
   "leagues" after the first. */
static void leg(unsigned char from, unsigned char to, unsigned char neg,
                unsigned char pos, unsigned char more)
{
    unsigned char n = to < from ? from - to : to - from;

    if (!n) {
        return;
    }
    if (msg[0]) {
        strcat(msg, ", ");
    }
    utoa(n, msg + strlen(msg), 10);
    if (!more) {
        strcat(msg, n == 1 ? " league" : " leagues");
    }
    strcat(msg, " ");
    strcat(msg, compass[to < from ? neg : pos]);
}

void tmap_list(void)
{
    unsigned char i, row = 3, l, x, y;
    const char *name;
    unsigned int t;

    VIC.spr_ena = 0;
    clrscr();
    textcolor(COLOR_YELLOW);
    cputsxy(1, 0, "Treasure maps");
    textcolor(COLOR_GRAY1);
    chlinexy(0, 1, 40);
    for (i = 0; i < OWG_MAPS; ++i) {
        t = G->tmap[i];
        if (!t) {
            continue;
        }
        l = OWG_TMAP_LAND(t);
        x = OWG_TMAP_X(t);
        y = OWG_TMAP_Y(t);
        textcolor(COLOR_LIGHTBLUE);
        if (l == ISLE_HOME) {
            strcpy(msg, "Keldmarch");
        } else {
            strcpy(msg, "Over the ");
            strcat(msg, compass[l]);
            strcat(msg, " sea");
        }
        cputsxy(1, row, msg);
        name = nearest(l, x, y);
        msg[0] = '\0';
        leg(near_x, x, 3, 1, 0);
        leg(near_y, y, 0, 2, msg[0] != '\0');
        strcat(msg, " of");
        textcolor(COLOR_WHITE);
        cputsxy(3, row + 1, msg);
        if (strlen(msg) + strlen(name) < 37) {
            cputc(' ');         /* the place on the same line */
            --row;
        } else {
            gotoxy(3, row + 2);
        }
        cputs(name);
        row += 4;
    }
    textcolor(COLOR_WHITE);
    if (row == 3) {
        cputsxy(1, 3, "You carry no treasure maps.");
    }
    textcolor(COLOR_GRAY2);
    cputsxy(1, 24, "Stand on the spot and press L");
    cgetc();
}
