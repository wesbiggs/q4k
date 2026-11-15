/*
 * q4k -- which music plays: a scene's SID file, loaded when it is wanted.
 *
 * assets/music.txt maps each scene to a SID file and one of its songs; the
 * build makes that into tune_file[] and tune_song[] (music_map.c) and puts
 * every file on the disk as m.NAME (tools/mksid.py), with a small header in
 * front that music.s reads: where init and play are, the volume, how fast
 * each song is called. One file is in memory at a time, under the KERNAL;
 * a scene whose file is another loads it, then starts its song. A file that
 * will not load (no disk) leaves the music off until a scene wants another,
 * without asking for the disk: the music is not worth stopping the game for.
 *
 * Everything here starts in DATA, not BSS, which the networked build has no
 * room in.
 */

#include <cbm.h>

#include <q4k/music.h>
#include <q4k/savegame.h>
#include <q4k/sprite.h>

/* Top of BASIC memory: see savegame.c, which raises it for the same reason. */
#define MEMSIZ (*(unsigned int *)0x0283)

#define NONE 0xFF

static unsigned char scene_now = NONE;  /* the last scene asked for */
static unsigned char file_held = NONE;  /* which file is under the KERNAL */
static unsigned char song_held = NONE;  /* and which song of it was started */
static unsigned char music_on = 1;      /* the M key */

void music_scene(unsigned char scene)
{
    unsigned char file, song;

    if (scene == scene_now) {
        return;
    }
    scene_now = scene;
    file = tune_file[scene];
    song = tune_song[scene];
    if (file == file_held && song == song_held) {
        return;                     /* the same music: it carries on */
    }
    if (file != file_held) {
        /* Silence before the disk (music_pause() waits for it to reach
           VICE's sound buffer), and no calls into a half-loaded tune. Every
           enabled sprite takes cycles from the serial bus (CLAUDE.md); the
           hero comes back with the next draw. */
        music_pause();
        music_stop();
        file_held = song_held = NONE;
        sprite_hide();
        MEMSIZ = 0xCF00;
        if (cbm_load(tune_name[file], save_device, 0) == 0) {
            return;
        }
        file_held = file;
    }
    song_held = song;
    if (music_on) {
        music_play(song);
    }
}

unsigned char music_toggle(void)
{
    music_on ^= 1;
    if (!music_on) {
        music_stop();
    } else if (file_held != NONE) {
        music_play(song_held);      /* from the top */
    }
    return music_on;
}
