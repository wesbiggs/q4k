#include <q4k/tiles.h>
#include <q4k/world.h>

unsigned char world_tile(unsigned char x, unsigned char y)
{
    unsigned char b;

    x &= WORLD_MASK;
    b = world_map[((unsigned int)(y & WORLD_MASK) << (WORLD_SHIFT - 1))
                  | (x >> 1)];
    if (x & 1) {
        b >>= 4;
    }
    b &= 0x0F;
    return world_tiles[b];
}

unsigned char world_flag(unsigned char x, unsigned char y)
{
    return tile_flags[world_tile(x, y) - TILE_BASE];
}
