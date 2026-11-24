/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Open world: what the engine's three parts share.
 *
 * The engine is a core (engine.c and smooth.s: the main loop, the scrolling,
 * the music, what every area does) and three overlays that take turns in
 * one slot of memory (cfg/c64-openworld.cfg's SLOT): the world's (world.c:
 * the overworld, the ships and the continents), the town's (town.c: towns
 * and the castle, their roofs and their people, and who to talk to: the
 * conversation itself is a program of its own, src/owtalk.c) and the
 * dungeon's (dungeon.c, with the dungeon generator and its monsters). Stepping into a town or
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
#define SHORE   ((unsigned char *)0xA000)
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
extern unsigned char sm_tunic;          /* the hero's colour, on foot */
extern unsigned char sm_skin;           /* and their skin's (sprite 7) */
extern const unsigned char ow_skins[];  /* each class's skin, two frames */
extern unsigned char still;             /* a test's: nothing idles */
extern signed char sm_npc_ox[7], sm_npc_oy[7];   /* pixels off the square */
/* Each person's sprite block (as a sprite pointer) and colour: the
   multiplexer (smooth.s) gives them the VIC's sprites 3-6, a frame at a
   time, so nothing writes the VIC's own registers for them. */
extern unsigned char sm_npc_ptr[7], sm_npc_col[7];
extern unsigned char sm_sproff;         /* nonzero: no sprites at all */
extern unsigned char sm_mxwant;  /* the last plan left someone out */
extern unsigned char sm_mxdirty; /* a picture or colour changed */
void sm_npcs(void);
void sm_redraw_box(void);
void sm_shores_rect(void);
void sm_stack(void);
extern unsigned char sm_sh_x0, sm_sh_y0, sm_sh_x1, sm_sh_y1;
extern unsigned char sm_box_c0, sm_box_c1, sm_box_r0, sm_box_r1;
extern unsigned char sm_torus;
/* The text window: two lines under the map (smooth.s). sm_say() moves the
   bottom one up and writes s (PETSCII, up to 40) under it in sm_ink (0-7),
   taking the window from the party's status (stlow.s) until the next step;
   sm_guard() waits while the split that makes the window is near, for
   whatever is about to keep interrupts off for long. */
void __fastcall__ sm_say(const char *s);  /* (stlow.s's: below) */
void sm_text_clear(void);
extern unsigned char sm_rows;   /* the map's rows: 22 under the window, 25
                                   with it hidden (stlow.s) */
void sm_guard(void);
extern unsigned char sm_ink;
extern unsigned char sm_irqend, sm_splitline;
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
void __fastcall__ ow_tune_resume(unsigned char song);   /* where it paused */
void ow_tune_pause(void);
void ow_tune_stop(void);
extern unsigned char ow_tune_on;

/* The tile sets' files (t.overworld ..., tools/mkoverworld.py --sets),
   which tileset() loads into SHORE for sm_tileset(); and (src/lib/ow_data.s)
   which tiles can be walked onto. */
extern const char ts_overworld[], ts_town[], ts_castle[], ts_dungeon[];
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
extern unsigned char moving;    /* a step is under way (dir) */
extern unsigned char phase;     /* the back screen's build: PHASE_BUILT
                                   once it is ready for the next flip */
#define PHASE_BUILT 0
#define PHASE_SHIFT 1           /* (not yet shifted from the front) */
extern unsigned char speed;     /* pixels a frame: 1 or 2 */
extern unsigned char enc_always;    /* tests: 1 every roll a fight, and 1
                                       or 2 a dungeon's monsters even with
                                       OWG_CALM; 3 every roll at sea */
extern unsigned int  steps;
extern unsigned char whole, switch_time, roof_time;

/* Where the hero went into a place from, in the world; the ships, and the
   one the hero is in (1 + its index, or 0). The core's, because a talk
   hands them to the next run of the engine (ow.h). */
extern unsigned int  out_wx;
extern unsigned char out_wy;
#define SHIPS 1                 /* the king's, at the castle's port: the
                                   game block has room for eight */
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
extern const char owtalk[];     /* "owtalk": a conversation */
/* A fight (OWF_* in ow.h; arg the ground or the dungeon), in owtown.
   Never returns. */
void __fastcall__ ow_fight_with(unsigned char kind, unsigned char arg);

/* L, the overlays' (ov_look): what they found, said and taken -- gold or
   draughts -- and a looted[] bit taken (0, and said, if it was before); or
   nothing, when the dead may rise from `ground` (NONE: never, as in a
   town). Neither returns from a fight. */
void __fastcall__ loot_found(unsigned char draughts, unsigned int amount);
unsigned char __fastcall__ take_bit(unsigned char bit);
void __fastcall__ look_nothing(unsigned char ground);
extern unsigned char grave_odds;
/* A way that is shut: the refusal's sound, and why, in cyan (sm_ink is
   0-7 only: light grey would be a multicolour character). */
void __fastcall__ refuse(const char *why);
/* The place just gone into, from the world (not back from another
   program): its overlay's ov_enter() sets it, and the core says "Entering
   NAME." once the window is up (say_entering()), unless it is hidden. */
extern const char *entering;
void say_entering(void);

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
void __fastcall__ tileset(const char *set);
void build_area(const char *set, unsigned char w, unsigned char h);
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
void ov_idle(void);             /* what is left of a frame (every frame) */
void ov_look(void);             /* L: what is on the hero's square */
void __fastcall__ ov_use(unsigned char k);  /* + or -, or fire (1): in,
                                   down the stairs or up them */

/* --- stlow.s, in OWLO (the engine's file, below $0801) ------------------- */

void st_show(void);             /* the party under the map, unless hidden */
void st_start(void);            /* the screen is the engine's: a greeting,
                                   or the status */
void st_step(void);             /* a step begins: the status back */
void st_toggle(void);           /* SPACE */
void st_save(void);             /* F1, asked first */
void st_load(void);             /* F3, asked first */
void st_look(void);             /* L, and a chest drawn open */
void st_arrived(void);          /* a step ends: a chest is opened */
void st_chests(void);           /* a dungeon level built: the open ones */
void st_tchests(void);          /* a place's roofs put on or off: the same */
void st_bob(void);              /* the keeper idles */

#endif /* OW_ENGINE_H */
