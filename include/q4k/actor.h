/*
 * q4k -- actors.
 *
 * One hero for now, but the roster is an array from the outset so adding an
 * Ultima-style party later is a matter of raising party_count rather than
 * rewriting everything that touches the player.
 */

#ifndef Q4K_ACTOR_H
#define Q4K_ACTOR_H

#define MAX_PARTY   4
#define NAME_LEN    9   /* 8 characters plus a terminator */

typedef struct {
    unsigned char x, y;         /* world position       */
    unsigned char glyph;        /* screen code          */
    unsigned char color;
    unsigned char hp, hp_max;
    unsigned char level;
    unsigned int  xp;
    unsigned char alive;
    char          name[NAME_LEN];
} Actor;

extern Actor         party[MAX_PARTY];
extern unsigned char party_count;

/* The character the player is currently steering. */
#define hero (party[0])

void party_init(void);

#endif /* Q4K_ACTOR_H */
