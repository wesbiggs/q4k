#!/usr/bin/env python3
"""Write down who stands where, for the NPCs.

Left to itself a model invents geography: it will send you north to a town
that is south, across a mountain range that is not there. The land is made
fresh for every game, so this file can only carry who lives where, and the
names of the places and lands: assets/geography.json. Where everything is the host works
out for itself, per game, from the lore_seed the C64 sends with every request
(host/geo.py, over tools/isleref.py's model of the generator).
"""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mktiles
import mknames
import mktowns


def main():
    tiles = mktiles.parse("assets/tiles.txt")
    by_char = {t.mapchar: t for t in tiles}

    places = {}
    towns = mktowns.parse(by_char)
    mktowns.resolve(towns, by_char)   # fills in where each entity stands
    for t in towns:
        places[t.id] = {"id": t.id, "name": t.name,
                        "kind": "castle" if "CASTLE" in t.name else "town"}

    for line in open("assets/places.txt"):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        _, pid, marker, name = line.split(None, 3)
        if marker not in ("D", "L"):
            sys.exit("places.txt: %s is a %r, which is neither D nor L"
                     % (pid, marker))
        places[pid] = {"id": pid, "name": name,
                       "kind": "lair" if marker == "L" else "dungeon"}

    # Which place each NPC stands in, taken from where the towns put them.
    npc_place = {}
    for t in towns:
        for _, _, kind, param in t.placed:
            if kind == "npc":
                npc_place[param] = t.id

    # What the game picks its own names from (assets/names.txt).
    names = mknames.parse()
    out = {"places": places, "npc_place": npc_place,
           "lands": names["land"], "towns": names["town"],
           "town_stride": names["stride"]["town"]}
    with open("assets/geography.json", "w") as f:
        json.dump(out, f, indent=1, sort_keys=True)
        f.write("\n")

    print("mkgeo: %d places, %d npcs located" % (len(places), len(npc_place)))


if __name__ == "__main__":
    main()
