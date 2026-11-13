#include <string.h>

#include <q4k/actor.h>
#include <q4k/battle.h>
#include <q4k/game.h>
#include <q4k/tiles.h>

Actor         party[MAX_PARTY];
unsigned char party_count;
unsigned char party_turn = 0xFF;
unsigned char party_level;
unsigned int  party_xp;

const char *const class_names[NUM_CLASSES] = { "Warrior", "Archer", "Wizard" };
const unsigned char class_glyph[NUM_CLASSES] = {
    T_WARRIOR, T_ARCHER, T_WIZARD,
};

/* Health at level 1 and what each level adds, by class; a wizard's magic. */
static const unsigned char hp_base[NUM_CLASSES] = { 22, 18, 16 };
static const unsigned char hp_step[NUM_CLASSES] = {  7,  5,  5 };
#define MP_BASE  6
#define MP_STEP  3

unsigned char hp_for(unsigned char cls, unsigned char level)
{
    return hp_base[cls] + hp_step[cls] * (level - 1);
}

/* Nonzero and everyone the title screen makes starts on this much health:
   --poke hp_start=1 lets a test lose a fight at the first blow, which a
   poke of the health itself cannot, since the party is made after the
   pokes. Nothing in the game sets it. */
unsigned char hp_start;

void party_make(unsigned char i, const char *name, unsigned char cls)
{
    Actor *a = &party[i];

    memset(a, 0, sizeof(*a));
    strcpy(a->name, name);
    a->cls    = cls;
    a->x      = hero.x;
    a->y      = hero.y;
    if (cls == CL_WIZARD) {
        a->known = a->ready = 1 << SP_SPARK;   /* what the letter taught */
    }
    if (party_count <= i) {
        party_count = i + 1;
    }
    party_grow();
    if (hp_start) {
        a->hp = hp_start;
    }
}

void party_init(void)
{
    memset(party, 0, sizeof(party));
    party_count = 0;
    party_level = 1;
    party_xp    = 0;
    party_make(0, "Alaric", CL_WARRIOR);
}

void party_grow(void)
{
    unsigned char i;
    Actor *a;

    for (i = 0; i < party_count; ++i) {
        a = &party[i];
        a->hp_max = hp_for(a->cls, party_level);
        a->mp_max = a->cls == CL_WIZARD ? MP_BASE + MP_STEP * (party_level - 1)
                                        : 0;
    }
    party_rest();
}

void party_rest(void)
{
    unsigned char i;
    Actor *a;

    for (i = 0; i < party_count; ++i) {
        a = &party[i];
        a->hp    = a->hp_max;
        a->mp    = a->mp_max;
    }
}

/* A point of magic every eight turns. */
void party_tick(void)
{
    unsigned char i;

    if (game_turn & 7) {
        return;
    }
    for (i = 0; i < party_count; ++i) {
        if (party[i].mp < party[i].mp_max) {
            ++party[i].mp;
        }
    }
}
