/*
 * Open world: what the engine's three parts share.
 *
 * The engine is a core (engine.c and smooth.s: the main loop, the scrolling,
 * the music, what every area does) and two overlays that take turns in one
 * slot of memory (cfg/c64-openworld.cfg's SLOT): the world's (world.c: the
 * overworld, the ships and the continents), the town's (town.c: towns and
 * the castle, their roofs and their people, and the conversations) and the
 * dungeon's (dungeon.c, with the dungeon generator). Stepping into a town or
 * a dungeon loads its overlay over the world's, and its way out loads the
 * world's back; what each keeps in BSS stays put while another is in, so
 * the overworld comes back as it was left.
 *
 * The core reaches an overlay only through the jump table at the start of
 * the slot (ovl.s), and an overlay may call the core and itself, nothing
 * else: tools/owslots.py checks that after every link.
 */

#ifndef OW_ENGINE_H
#define OW_ENGINE_H

#include <q4k/game.h>
#include <q4k/owgame.h>

#define WORLD   ((unsigned char *)0xB000)
#define BIAS    32              /* squares: smooth.s's 64 characters */
#define NONE    0xFF

/* The hero's square on the screen, in pixels from the top left of the
   character matrix, when the camera is still (see place_camera()). */
#define HERO_PX 148
#define HERO_PY 84

/* --- smooth.s ---------------------------------------------------------- */

extern unsigned char sm_ox, sm_oy, sm_fx, sm_fy, sm_front, sm_colend;
void sm_quit(void);
extern volatile unsigned char sm_frame;
void sm_init(void);
void sm_show(void);
void sm_draw_all(void);
void sm_shores(void);
void sm_animate(void);
void __fastcall__ sm_shift(unsigned char dir);
void __fastcall__ sm_edge_row(unsigned char dir);
void __fastcall__ sm_edge_col(unsigned char dir);
void __fastcall__ sm_commit(unsigned char flip);
void __fastcall__ sm_sprite(unsigned char frame);
void __fastcall__ sm_tileset(const unsigned char *set);
void __fastcall__ sm_blank(unsigned char on);
void __fastcall__ sm_reu_keep(unsigned char out);
extern unsigned char sm_offtile;
extern unsigned char sm_npc_n, sm_npc_x[7], sm_npc_y[7];
void sm_npcs(void);
void sm_redraw_box(void);
void sm_shores_rect(void);
void sm_stack(void);
extern unsigned char sm_sh_x0, sm_sh_y0, sm_sh_x1, sm_sh_y1;
extern unsigned char sm_box_c0, sm_box_c1, sm_box_r0, sm_box_r1;
extern unsigned char sm_torus;
void sm_vcopy(void);
extern unsigned char *vc_src, *vc_dst;
extern unsigned char vc_n, vc_sstep, vc_dstep;

/* A continent built behind the scrolling (src/lib/ow_gen.s). */
void __fastcall__ gen_start(unsigned char d);
void __fastcall__ gen_run(unsigned char frame);
extern volatile unsigned char gen_state;    /* 1 building, 3 copying it into
                                               the slot, 2 there, 0 nothing */

/* The music (src/lib/ow_tune.s). */
void __fastcall__ ow_tune_play(unsigned char song);
void ow_tune_stop(void);

/* The tile sets (src/lib/ow_data.s), and which tiles can be walked onto. */
extern const unsigned char ts_overworld[], ts_town[], ts_castle[],
                           ts_dungeon[];
extern const unsigned char ow_pass[64];

/* --- The core (engine.c) ------------------------------------------------ */

/* AREA_HOME ... are owgame.h's. */
extern unsigned char area, area_w, area_h;

/* Eight ways, a square at a time: sprite.h's four, then the diagonals. */
#define DIR_DL 4
#define DIR_DR 5
#define DIR_UL 6
#define DIR_UR 7
extern const signed char dirdx[8], dirdy[8];
extern const unsigned char facing[8];

extern unsigned char hx, hy;    /* the hero's square, as the engine has it */
extern unsigned char dir, stride, pre_dir, frame, after;
extern unsigned int  steps;
extern unsigned char whole, switch_time, roof_time;

/* Where the hero went into a place from, in the world; the ships, and the
   one the hero is in (1 + its index, or 0). The core's, because a talk
   hands them to the next run of the engine (ow.h). */
extern unsigned int  out_wx;
extern unsigned char out_wy;
#define SHIPS 8
extern unsigned int  ship_x[SHIPS];
extern unsigned char ship_y[SHIPS];
extern unsigned char boat;

/* Which town to go into (TOWN_*), set before asking for the town's
   overlay; and, coming back from a talk, where the hero stood in it. */
extern unsigned char town_index, resuming, resume_x, resume_y;
extern unsigned char dun_index;         /* the dungeon to go into, or in */

/* The world overlay's: the hero's square in the world, on the overworld. */
extern unsigned int  wx;
extern unsigned char wy;

/* Where the party is, and what the engine keeps of the game, into the game
   block (owgame.h): before a save, or before another program takes over. */
void owg_where(void);
/* Another program of the open world's over the engine (the talk, a shop
   ...), with OWB filled in: the game block first. Never returns. */
void __fastcall__ ow_leave(const char *name);
extern const char owtown[];     /* "owtown": the screens of text */
/* A fight (OWF_* in ow.h; arg the ground or the dungeon), in owtown.
   Never returns. */
void __fastcall__ ow_fight_with(unsigned char kind, unsigned char arg);
/* After a step onto that tile: a wandering monster, now and then. */
void __fastcall__ wander(unsigned char tile);

void next_frame(void);
void play_for(unsigned char scene);
unsigned char tile_at(unsigned char x, unsigned char y);
unsigned char passable(unsigned char x, unsigned char y);
/* Whether a step that way can be taken in a place: onto open ground, and on
   a diagonal not squeezing between two corners. */
unsigned char __fastcall__ place_step(unsigned char d);
unsigned char hero_frame(unsigned char d, unsigned char st);
void place_camera(void);
void leave_area(void);
void build_area(const unsigned char *set, unsigned char w, unsigned char h);
void __fastcall__ show_area(unsigned char x, unsigned char y);

/* The overlays: which one is wanted next, and which is in the slot. An
   overlay asks for the other by setting ov_want and returning; the core
   loads it when control is back with it and calls its ov_enter(). */
#define OV_NONE  0
#define OV_WORLD 1
#define OV_TOWN  2
#define OV_DUNGEON 3
extern unsigned char ov_want;

/* --- The jump table at the start of the slot (ovl.s) -------------------- */

void ov_enter(void);            /* just loaded: take over from here */
unsigned char __fastcall__ ov_can_step(unsigned char d);
void ov_arrived(void);          /* a step has ended: hx, hy are past it */
void __fastcall__ ov_bump(unsigned char d);  /* a step that could not be */
void ov_after(void);            /* the frame after a step (after != 0) */
void ov_idle(void);             /* what is left of a frame */

#endif /* OW_ENGINE_H */
