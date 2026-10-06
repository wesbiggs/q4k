#include <q4k/isle.h>
#include <q4k/names_data.h>

/* The generator itself is in isle_gen.s. */

static const char *const names[BIOMES] = LAND_NAMES;

const char *isle_name(void)
{
    return names[isle_biome];
}
