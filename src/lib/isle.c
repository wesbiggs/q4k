#include <q4k/isle.h>

/* The generator itself is in isle_gen.s. */

static const char *const names[BIOMES] = {
    "The Green Downs", "The Briar Wood", "The Sallow Mire",
    "The Amber Waste", "The Iron Crags",
};

const char *isle_name(void)
{
    return names[isle_biome];
}
