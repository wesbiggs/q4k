#include <string.h>

#include <q4k/actor.h>
#include <q4k/tiles.h>
#include <q4k/world.h>

Actor         party[MAX_PARTY];
unsigned char party_count;

void party_init(void)
{
    memset(party, 0, sizeof(party));
    party_count = 1;

    strcpy(hero.name, "Alaric");
    hero.x      = world_start_x;
    hero.y      = world_start_y;
    hero.glyph  = T_PLAYER;
    hero.color  = 1;           /* white */
    hero.hp     = 20;
    hero.hp_max = 20;
    hero.level  = 1;
    hero.xp     = 0;
    hero.alive  = 1;
}
