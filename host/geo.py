"""The lay of the land, worked out from the game's seed.

The C64 sends its lore_seed (a byte) with every request, and the land is a
pure function of that seed: tools/isleref.py is a bit-exact model of the
generator in src/lib/isle_gen.s. So the host needs nothing more on the wire to
know where everything is -- it builds the same lands the game does and turns
the coordinates into what a local would say: a compass point and a rough walk,
for the places on the same land and for the harbours and what lies across them.

Nothing here is stored; it is a few milliseconds of Python per seed, cached.
"""

import functools
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "tools"))
import isleref as ref  # noqa: E402

HOME = ref.HOME
COMPASS = ["north", "east", "south", "west"]    # the ports' own numbering
WIND = ["north", "north-east", "east", "south-east",
        "south", "south-west", "west", "north-west"]

# Every name comes from assets/geography.json, which tools/mkgeo.py writes
# from the asset files (towns.txt, places.txt, names.txt): the lands in the
# order of ref.NAMES, and the towns a land makes for itself, indexed as
# src/lib/town_gen.c does, by the stride in town_stride. Npcs loads it and hands it over with use().
DATA = {}

# slot_of() puts these on the four continents, in the order the quest wants
# them; the first and third are towns, the others dungeons.
STORY = ["wardlow", "drownedstair", "ashcombe", "blackpit"]
KIND = {"town": ref.G_TOWN, "dungeon": ref.G_DUNGEON}


def use(data):
    DATA.clear()
    DATA.update(data)
    world.cache_clear()


def name(pid):
    """A place's name as the sentence has it: at the head of one, "The Hollow
    Crown"; after "in" or "to", "the Hollow Crown"."""
    return DATA["places"][pid]["name"]


def mid(text):
    return "the " + text[4:] if text.startswith("The ") else text


def cap(text):
    return text[0].upper() + text[1:]


def slot_of(seed, d):
    """src/lib/slot.c's slot_of(), which tools/homepath.py has as well."""
    p = [0, 1, 2, 3]
    v = seed & 255
    for i in (3, 2, 1):
        v = (v * 5 + 3) & 255
        j = (v >> 5) % (i + 1)
        p[i], p[j] = p[j], p[i]
    return p[d]


def bearing(a, b):
    """Which way b lies from a, as a point of the compass. The map has north
    at the top."""
    import math
    dx, dy = b[0] - a[0], b[1] - a[1]
    deg = math.degrees(math.atan2(dx, -dy)) % 360
    return WIND[int((deg + 22.5) // 45) % 8]


def walk(a, b):
    """How far, in the terms of someone who has walked it."""
    d = max(abs(a[0] - b[0]), abs(a[1] - b[1]))
    if d <= 8:
        return "a short walk"
    if d <= 18:
        return "an hour or so"
    if d <= 32:
        return "half a day's walk"
    if d <= 48:
        return "a day's walk"
    return "more than a day's walk"


def where(a, b):
    return "%s to the %s" % (walk(a, b), bearing(a, b))


@functools.lru_cache(maxsize=8)
def world(seed):
    """Every place by id, as {id: (land, (x, y), name)}, plus the facts about
    the ports. land is HOME or a continent's compass number."""
    seed &= 255
    home = ref.build(seed, HOME)
    places = {}
    towns = DATA["towns"]

    def town_name(land):
        # src/lib/town_gen.c, with the stride from assets/names.txt.
        return towns[(seed + DATA["town_stride"] * land) % len(towns)]
    for i, pid in enumerate(("millford", "castle", "hollowcrown", "lair")):
        places[pid] = (HOME, home.places[i], name(pid))
    places["hometown"] = (HOME, home.places[4], town_name(HOME))

    lands = {HOME: (home, "Keldmarch")}
    ports = {}      # compass number -> (x, y of the harbour, what is across)
    for d in range(4):
        slot = slot_of(seed, d)
        pid = STORY[slot]
        land = ref.build(seed, d, KIND[DATA["places"][pid]["kind"]])
        lands[d] = (land, mid(DATA["lands"][land.dom]))
        places[pid] = (d, land.places[0], name(pid))
        places["town%d" % d] = (d, land.places[1],
                                town_name(d))
        places["dock%d" % d] = (d, land.docks[0], "the harbour")
        ports[d] = (home.docks[d], d, pid)
    return places, lands, ports


def lines_for(place_id, seed):
    """What someone who lives at place_id knows of the way to everywhere,
    one plain sentence each. [] if the place is not one of the world's."""
    places, lands, ports = world(seed)
    if place_id not in places:
        return []
    land, here, _ = places[place_id]
    out = []

    # The same land.
    for pid, (l, xy, called) in places.items():
        if l == land and pid != place_id and not pid.startswith("dock"):
            out.append("%s is %s." % (cap(mid(called)), where(here, xy)))

    if land == HOME:
        for d, (xy, cont, story) in ports.items():
            out.append("The %s harbour is %s; its ship sails to %s, where "
                       "%s lies." % (COMPASS[d], where(here, xy),
                                     lands[cont][1], mid(places[story][2])))
    else:
        dock = places["dock%d" % land][1]
        out.append("The harbour, where the ship home sails from, is %s."
                   % where(here, dock))
        out.append("The ship from here crosses to Keldmarch, where %s "
                   "and %s are; from there the other countries "
                   "are reached by Keldmarch's other harbours."
                   % (name("millford"), name("castle")))
        for d, (xy, cont, story) in ports.items():
            if cont != land:
                out.append("%s lies in %s, across the sea from Keldmarch's "
                           "%s coast." % (cap(mid(places[story][2])),
                                          lands[cont][1], COMPASS[d]))
        out.append("%s and %s are in Keldmarch."
                   % (name("hollowcrown"), name("lair")))
    return out
