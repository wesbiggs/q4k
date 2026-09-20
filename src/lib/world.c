#include <q4k/tiles.h>
#include <q4k/world.h>

unsigned char world_tile(unsigned char x, unsigned char y)
{
    return world_map[((unsigned int)(y & WORLD_MASK) << WORLD_SHIFT)
                     | (x & WORLD_MASK)];
}

unsigned char world_flag(unsigned char x, unsigned char y)
{
    return tile_flags[world_tile(x, y) - TILE_BASE];
}
