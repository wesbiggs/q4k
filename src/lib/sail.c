#include <c64.h>
#include <cbm.h>

#include <q4k/actor.h>
#include <q4k/game.h>
#include <q4k/isle.h>
#include <q4k/music.h>
#include <q4k/sail.h>
#include <q4k/sprite.h>
#include <q4k/tiles.h>
#include <q4k/view.h>
#include <q4k/world.h>

/* Unsigned, so that -1 is 255 and adding it is a single add. */
static const unsigned char dxs[4] = { 0, 1, 0, 255 };
static const unsigned char dys[4] = { 255, 0, 1, 0 };

static const char *const outward[4] = {
    "You sail north.", "You sail east.", "You sail south.", "You sail west.",
};

/* The voyage is a camera, not a ship: the ship stays in the middle of the
 * viewport and the land is what moves. Leaving, the old land is slid off the
 * far edge until nothing is left but sea; arriving, the new land is slid on
 * from it. Both ends are the same view of open water, so there is no seam, and
 * each ends on the very frame the map is drawn in afterwards.
 *
 * Positions are in world_view()'s terms: biased by 64 so that a little off the
 * edge of a map is still a small positive number, and the sea beyond the
 * window that world_view() is given is drawn as water. */
#define BIAS 64

static unsigned char island;    /* flying over a continent, not home */
static unsigned char cx, cy;    /* the viewport's top-left */
static unsigned char ix, iy;    /* the ship */

static void put(unsigned char x, unsigned char y,
                unsigned char code, unsigned char color)
{
    unsigned int at = (unsigned int)y * SCR_W + x;

    SCREEN_RAM[at] = code;
    COLOR_RAM[at]  = color;
}

/* The picture, with the ship in it if it is on screen. */
static void frame(void)
{
    unsigned char vx = ix - cx, vy = iy - cy;

    world_view(cx, cy, island ? SRC_ISLE : SRC_HOME);
    if (vx < VIEW_W && vy < VIEW_H) {
        put(vx, vy, T_SHIP, TC_SHIP);
    }
    waitvsync();
    waitvsync();
}

/* Sail `n` squares in direction d. */
static void go(unsigned char d, unsigned char n)
{
    while (n--) {
        ix += dxs[d];
        iy += dys[d];
    }
}

/* Only a screenful about (x,y) is land; the rest is sea. */
static void around(unsigned char x, unsigned char y)
{
    world_x0 = x - VIEW_CX;
    world_x1 = x + VIEW_CX;
    world_y0 = y - VIEW_CY;
    world_y1 = y + VIEW_CY;
}

/* Sail in direction d to (ex,ey), with the viewport centred on the ship. With
   `work` set, pieces of the continent being built are done between frames, so
   the ship is not left waiting while the land is made. */
static void fly(unsigned char d, unsigned char ex, unsigned char ey,
                unsigned char work)
{
    for (;;) {
        cx = ix - VIEW_CX;
        cy = iy - VIEW_CY;
        frame();
        if (work) {
            isle_work();
            isle_work();
        }
        if (ix == ex && iy == ey) {
            return;
        }
        ix += dxs[d];
        iy += dys[d];
    }
}

/* Where the viewport sits on a continent (a position biased by BIAS) when the
   hero is at `at`: centred, unless that would show past the edge. */
static unsigned char camera(unsigned char at, unsigned char centre,
                            unsigned char most)
{
    at = (at < centre) ? 0 : at - centre;
    return (at > most ? most : at) + BIAS;
}

/* Slide the viewport, a square a frame, to (tx,ty). */
static void pan(unsigned char tx, unsigned char ty)
{
    while (cx != tx || cy != ty) {
        cx += (cx < tx) - (cx > tx);
        cy += (cy < ty) - (cy > ty);
        frame();
    }
}

/* Out to sea: a screenful and a square from a continent's edge along the lane
   the ship uses, so that none of it is in view. */
static void offing(unsigned char d)
{
    ix = isle_sx + BIAS;
    iy = isle_sy + BIAS;
    go(d ^ 2, (d & 1) ? VIEW_CX + 1 : VIEW_CY + 1);
}

/* The overworld's side of a voyage: from the port out to a screenful of sea
   with the home coast slipping off (going out), or in from there to the square
   beside the port with it sliding on (going home). The window about the start
   is what is land going out, and the window about the end is what is going
   home. */
static void sea(unsigned char d, unsigned char home, unsigned char work)
{
    unsigned char n = (d & 1) ? VIEW_W : VIEW_H, ex, ey;

    island = 0;
    ix = home_port_x[d] + BIAS;
    iy = home_port_y[d] + BIAS;
    if (home) {
        go(d, 1);
    }
    around(ix, iy);
    ex = ix;
    ey = iy;
    if (home) {
        go(d, n);
        fly(d ^ 2, ex, ey, 0);
    } else {
        go(d, n);
        ex = ix;
        ey = iy;
        ix = home_port_x[d] + BIAS;
        iy = home_port_y[d] + BIAS;
        fly(d, ex, ey, work);
    }
}

void sail_out(void)
{
    unsigned char d, ex, ey;

    for (d = 0; d < 3; ++d) {
        if (home_port_x[d] == hero.x && home_port_y[d] == hero.y) {
            break;
        }
    }

    /* The captains take you where the story does. */
    if (quest_stage < Q_EMBER + slot_of(d)) {
        view_message("The captain will not sail yet.");
        return;
    }

    view_message(outward[d]);
    music_scene(MUSIC_SEA);
    sprite_hide();

    /* The home coast slides off while the other shore is made behind it. */
    isle_story = slot_of(d) & 1;
    isle_begin(d);
    sea(d, 0, 1);
    while (isle_work()) {
    }
    scene = SCENE_ISLE;
    land  = d;
    depth = 0;
    local_w = ISLE_W;
    local_h = ISLE_H;
    view_draw_panel();

    /* Then the new one slides on, the ship keeping to the middle, until it is
       one square off the landing. */
    island = 1;
    world_x0 = world_y0 = BIAS;
    world_x1 = BIAS + ISLE_W - 1;
    world_y1 = BIAS + ISLE_H - 1;
    offing(d);
    ex = isle_dock_x + BIAS;
    ey = isle_dock_y + BIAS;
    ex -= dxs[d];
    ey -= dys[d];
    fly(d, ex, ey, 0);
    /* The map keeps the camera inside itself; the ship was past that. */
    pan(camera(isle_dock_x, VIEW_CX, ISLE_W - VIEW_W),
        camera(isle_dock_y, VIEW_CY, ISLE_H - VIEW_H));

    hero.x = isle_dock_x;
    hero.y = isle_dock_y;
    ++game_turn;
}

void sail_home(void)
{
    unsigned char d = land, ex, ey;

    home_ensure();      /* built long since, unless a restored game is only now back */
    music_scene(MUSIC_SEA);
    sprite_hide();

    /* From the landing: the view as the map draws it, then the ship to the
       middle of it, then out until the continent is gone. */
    island = 1;
    world_x0 = world_y0 = BIAS;
    world_x1 = BIAS + ISLE_W - 1;
    world_y1 = BIAS + ISLE_H - 1;
    ix = hero.x + BIAS;
    iy = hero.y + BIAS;
    cx = camera(hero.x, VIEW_CX, ISLE_W - VIEW_W);
    cy = camera(hero.y, VIEW_CY, ISLE_H - VIEW_H);
    frame();
    pan(ix - VIEW_CX, iy - VIEW_CY);
    ex = ix;
    ey = iy;
    offing(d);
    ix ^= ex;                   /* swap: the ship is at the landing, and the */
    ex ^= ix;                   /* offing is where it is bound */
    ix ^= ex;
    iy ^= ey;
    ey ^= iy;
    iy ^= ey;
    fly(d ^ 2, ex, ey, 0);

    /* Then the home coast slides on until the ship is beside the port. */
    scene = SCENE_WORLD;
    land  = ISLE_HOME;
    depth = 0;
    local_w = local_h = WORLD_SIZE;
    view_draw_panel();
    sea(d, 1, 0);

    hero.x = home_port_x[d];
    hero.y = home_port_y[d];
    ++game_turn;
}
