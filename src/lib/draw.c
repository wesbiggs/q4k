/*
 * q4k -- drawing the map you are on, and saying where that is.
 *
 * Split out of view.c, which keeps the message log, the chrome and the panel
 * that the fight draws too, so that the offline build can keep this with the
 * map's engine, on the disk (ENGINES.md). Both builds link it; nothing here
 * changed in the move but view_place(), which was the middle of
 * view_draw_panel().
 */

#include <conio.h>
#include <c64.h>

#include <q4k/actor.h>
#include <q4k/fov.h>
#include <q4k/game.h>
#include <q4k/isle.h>
#include <q4k/charset.h>
#include <q4k/sprite.h>
#include <q4k/tiles.h>
#include <q4k/view.h>
#include <q4k/world.h>

/* Where the viewport's top-left corner is in local[]. A dungeon level is
   bigger than the screen, so the view follows you like the overworld's,
   but stops at the edges instead of wrapping: it keeps you centred until
   there is no more map that way, and then you walk off the centre. A town
   is exactly one viewport, so both come out 0 and it never moves. */
static unsigned char cam_x, cam_y;

static unsigned char camera(unsigned char at, unsigned char centre,
                            unsigned char size, unsigned char view)
{
    if (at <= centre) {
        return 0;
    }
    at -= centre;
    return (at + view > size) ? size - view : at;
}

static void draw_local(void)
{
    unsigned char vx, vy, x, y, i, code, row, b, m;
    unsigned char *scr, *col;

    cam_x = camera(hero.x, VIEW_CX, local_w, VIEW_W);
    cam_y = camera(hero.y, VIEW_CY, local_h, VIEW_H);

    for (vy = 0; vy < VIEW_H; ++vy) {
        y   = cam_y + vy;
        row = fov_row[y];
        scr = SCREEN_RAM + (unsigned int)vy * SCR_W;
        col = COLOR_RAM  + (unsigned int)vy * SCR_W;

        for (vx = 0, x = cam_x; vx < VIEW_W; ++vx, ++x, ++scr, ++col) {
            code = local[LOCAL_AT(x, y)];

            /* Underground you draw what you can see and what you remember
               seeing, and nothing else. Above ground the whole place is in
               front of you. */
            if (scene == SCENE_DUNGEON) {
                b = row + (x >> 3);
                m = fov_bit[x & 7];
                if (!(fov_seen[b] & m)) {
                    *scr = 0x20;
                    *col = COLOR_BLACK;
                    continue;
                }
                if (!(fov_lit[b] & m)) {
                    *scr = code;
                    *col = COLOR_GRAY1;
                    continue;
                }
            }

            *scr = code;
            *col = (code >= TILE_BASE + TILE_COUNT)
                 ? COLOR_LIGHTRED       /* the keeper */
                 : tile_colors[code - TILE_BASE];
        }
    }

    /* Only towns have people in them, and towns never scroll. */
    if (scene == SCENE_TOWN) {
        for (i = 0; i < here_n; ++i) {
            x = here_ents[i].x;
            y = here_ents[i].y;
            SCREEN_RAM[(unsigned int)y * SCR_W + x] = T_VILLAGER;
            COLOR_RAM[(unsigned int)y * SCR_W + x]  =
                (here_ents[i].kind == ENT_NPC || here_ents[i].kind == ENT_VILLAGER)
                ? COLOR_CYAN : COLOR_YELLOW;
        }
    }
}

/* The hero is not in the map at all: the square they stand on is drawn like
   any other, and the sprite goes over the top of it. */
void view_draw(void)
{
    if (scene == SCENE_WORLD) {
        cam_x = camera(hero.x, VIEW_CX, WORLD_SIZE, VIEW_W);
        cam_y = camera(hero.y, VIEW_CY, WORLD_SIZE, VIEW_H);
        world_draw(cam_x, cam_y);
    } else if (scene != SCENE_TOWN || !town_drawn) {
        /* A town is drawn once; anything else every turn, including the
           first one after walking out of a town onto a continent. */
        draw_local();
    }
    town_drawn = (scene == SCENE_TOWN);
    sprite_show(hero.x - cam_x, hero.y - cam_y);
}

/* The panel's rows for where you are: the land, a town by name, or how deep
   you have gone. view_draw_panel() calls it unless a fight has those rows. */
void view_place(void)
{
    cclearxy(PANEL_X, 10, PANEL_W);
    cclearxy(PANEL_X, 11, PANEL_W);
    cclearxy(PANEL_X, 12, PANEL_W);     /* a fight's second row of health */
    textcolor(COLOR_LIGHTBLUE);
    if (scene == SCENE_TOWN) {
        cputsxy(PANEL_X, 10, town_name());
    } else if (scene == SCENE_DUNGEON) {
        cputsxy(PANEL_X, 10, dungeon_names[place]);
        cputsxy(PANEL_X, 11, "Level");
        view_num(PANEL_X + 6, 11, depth);
    } else if (scene == SCENE_ISLE) {
        cputsxy(PANEL_X, 10, isle_name());
    } else {
        cputsxy(PANEL_X, 10, "Keldmarch");
    }
}
