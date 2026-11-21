/*
 * q4k -- the music's scenes, pausing it round the disk, and the sound
 * effects.
 *
 * Each scene (MUSIC_HOME, MUSIC_FIGHT ...: music_map.h, made from
 * assets/music.txt) plays a song of a SID file, which the open world's
 * engine loads into the RAM under the KERNAL (play_for(), src/ow/engine.c)
 * and plays from its frame interrupt (src/lib/ow_tune.s).
 *
 * Every interrupt takes cycles from the KERNAL's serial-bus code, so the
 * disk code pauses the music around its transfers: music_pause() and
 * music_resume(), which the screens of text have from src/ow/owstand.c.
 */

#ifndef Q4K_MUSIC_H
#define Q4K_MUSIC_H

#include <q4k/music_map.h>

void music_pause(void);     /* silence, keeping the place */
void music_resume(void);


/*
 * Sound effects, on voice 3, played from the same interrupt. They sound
 * whether or not the music is on, and X turns them off separately. A tune
 * may not use voice 3 (tools/mksid.py refuses one that does).
 */
#define SFX_MELEE  7    /* a blow in melee, either way */
#define SFX_BOW    8    /* a shot that lands, either way */
#define SFX_NO     9    /* not that way, or not yet (the open world's) */
#define SFX_DRINK 10    /* a draught drunk */
#define SFX_LEVEL 11    /* the party reaches a level */

void sfx_play(unsigned char id);
void sfx_step(unsigned char tile);  /* a step onto that tile, by terrain */
unsigned char sfx_toggle(void);     /* the X key; returns 1 if now on */
unsigned char sfx_on(void);         /* 1 if they are on */
void sfx_delay(void);   /* wait out the last effect, if they are on */

#endif
