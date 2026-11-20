/*
 * q4k -- the screen a conversation is on (src/lib/talk.c's dialog_talk()).
 *
 * talk.c says what to show and asks for keys through these; how it looks is
 * the program's. The game's is the text screen (src/lib/talkscr.c, the
 * tsc_* functions, which src/lib/talkts.s names ts_*). The open world's
 * owtalk defines ts_* itself (src/ow/talkpic.c): a portrait of the speaker
 * with the words beside it on a bitmap, and the text screen, through
 * tsc_*, for anyone without a portrait.
 *
 * Text is ASCII, as the panel's (panel.h); colours are COLOR_*.
 */

#ifndef Q4K_TALKSCR_H
#define Q4K_TALKSCR_H

#define TS_ALL  0xFF    /* ts_topic(TS_ALL, 0): clear the topics */

/* The screen for a talk with `name` (PETSCII, as npc_names[] are). */
void ts_open(const char *name);
/* The person's lines are loaded (talk.c's last use of the disk): the
   screen may now take the machine. */
void ts_ready(unsigned char npc);
/* The player's question, and what is said: one line, or wrapped. */
void ts_ask(const char *ascii);
void ts_say(const char *ascii, unsigned char color, unsigned char wrap);
/* Topic `i` of the menu (`ascii` its number and label), or TS_ALL. */
void ts_topic(unsigned char i, const char *ascii);
/* The next key, as cgetc() gives it. */
unsigned char ts_key(void);
/* The talk is over: the machine back as ts_open() found it. */
void ts_close(void);

/* The text screen's, which the ts_* above are in the game. */
void tsc_open(const char *name);
void tsc_ask(const char *ascii);
void tsc_say(const char *ascii, unsigned char color, unsigned char wrap);
void tsc_topic(unsigned char i, const char *ascii);
unsigned char tsc_key(void);

#endif /* Q4K_TALKSCR_H */
