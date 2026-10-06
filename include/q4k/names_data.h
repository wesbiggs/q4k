/* GENERATED from assets/names.txt by tools/mknames.py -- do not edit. */
#ifndef Q4K_NAMES_DATA_H
#define Q4K_NAMES_DATA_H

#define LAND_NAMES { "The Green Downs", "The Briar Wood", "The Sallow Mire", "The Amber Waste", "The Iron Crags" }
#define TOWN_NAMES { "Harrowby", "Thistledown", "Marrow Ford", "Oldcombe", "Saltmere", "Greywick", "Fenhallow", "Stonebridge" }
#define INN_NAMES { "The Wayfarer", "The Salted Pig", "The Rusty Plough", "The Last Lantern" }

/* index = (lore_seed + STRIDE * land) & (COUNT - 1) */
#define TOWN_STRIDE 3
#define TOWN_COUNT 8

/* index = (lore_seed + STRIDE * land) & (COUNT - 1) */
#define INN_STRIDE 1
#define INN_COUNT 4

#endif
