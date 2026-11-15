/*
 * Quest for the King -- the multicolour fight: the block its caller fills
 * in (the Fight of include/q4k/fight.h), as the module sees it.
 *
 * The party and what it carries go by the game's own names, so that the
 * game's arms.c links against them unchanged; the rest is fq_*. They are
 * laid down in this order, right after the module's jump -- head.s checks
 * where they start and how long they are.
 */

#include <q4k/actor.h>
#include <q4k/fight.h>

#pragma bss-name (push, "FIGHTBLK")
Actor         party[MAX_PARTY];
unsigned char party_count, party_level;
unsigned char potions, arrows;
unsigned char fq_tunic[MAX_PARTY];
char          fq_name[FIGHT_NAME];
unsigned char fq_hp, fq_atk, fq_def, fq_colour, fq_speed, fq_size;
unsigned char fq_shot, fq_reach, fq_kind;
unsigned char fq_count, fq_flee;
unsigned int  fq_seed;
char          fq_field[FIGHT_FNAME];
char          fq_sprite[FIGHT_FNAME];
unsigned char fq_foes;
unsigned char fq_end;           /* only its address: the end of the block */
#pragma bss-name (pop)
