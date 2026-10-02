/*
 * q4k -- the theme, on SID voice 1.
 *
 * Played from the frame interrupt, so it carries on whatever the game is
 * doing. The score is assets/theme.txt; see src/lib/music.s.
 *
 * The disk is the exception. Every interrupt takes cycles from the KERNAL's
 * serial-bus code, so savegame.c pauses the music around its transfers.
 * music_pause() and music_resume() do nothing unless the music is playing,
 * so they are safe to call before music_start() too.
 */

#ifndef Q4K_MUSIC_H
#define Q4K_MUSIC_H

void music_start(void);     /* from the top, and loops until stopped */
void music_stop(void);      /* silence; music_start() begins again */
void music_pause(void);     /* silence, keeping the place */
void music_resume(void);
unsigned char music_toggle(void);   /* the M key; returns 1 if now playing */


/*
 * Sound effects, on voice 3, played from the same interrupt. They sound
 * whether or not the music is on, and X turns them off separately.
 */
#define SFX_MELEE  7    /* a blow in melee, either way */
#define SFX_BOW    8    /* a shot that lands, either way */

void sfx_play(unsigned char id);
void sfx_step(unsigned char tile);  /* a step onto that tile, by terrain */
unsigned char sfx_toggle(void);     /* the X key; returns 1 if now on */
void sfx_delay(void);   /* wait out the last effect, if they are on */

#endif
