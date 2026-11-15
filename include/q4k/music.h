/*
 * q4k -- the music, a SID file a scene at a time, and the sound effects.
 *
 * Each scene the game knows (MUSIC_HOME, MUSIC_FIGHT ...: music_map.h, made
 * from assets/music.txt) plays a song of a SID file. music_scene() says
 * which scene the player is in; if its file is not the one in memory, it is
 * loaded from the disk (src/lib/tune.c) into the RAM under the KERNAL, and
 * the song starts from the top. The same file and song carries on. The tune
 * plays from the frame interrupt (src/lib/music.s), so it carries on
 * whatever the game is doing.
 *
 * The disk is the exception. Every interrupt takes cycles from the KERNAL's
 * serial-bus code, so the disk code pauses the music around its transfers.
 * music_pause() and music_resume() do nothing unless the music is playing,
 * so they are safe to call before anything has started too.
 */

#ifndef Q4K_MUSIC_H
#define Q4K_MUSIC_H

#include <q4k/music_map.h>

void music_scene(unsigned char scene);  /* MUSIC_*: load and start, if new */
unsigned char music_toggle(void);   /* the M key; returns 1 if now on */

void music_stop(void);      /* silence, and no more calls into the tune */
void music_pause(void);     /* silence, keeping the place */
void music_resume(void);
void music_play(unsigned char song);    /* the loaded tune's song, from 0 */

/* 0 stopped, 1 playing, 2 paused (music.s). */
extern unsigned char music_state;


/*
 * Sound effects, on voice 3, played from the same interrupt. They sound
 * whether or not the music is on, and X turns them off separately. A tune
 * may not use voice 3 (tools/mksid.py refuses one that does).
 */
#define SFX_MELEE  7    /* a blow in melee, either way */
#define SFX_BOW    8    /* a shot that lands, either way */

void sfx_play(unsigned char id);
void sfx_step(unsigned char tile);  /* a step onto that tile, by terrain */
unsigned char sfx_toggle(void);     /* the X key; returns 1 if now on */
void sfx_delay(void);   /* wait out the last effect, if they are on */

#endif
