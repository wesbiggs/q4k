#!/usr/bin/env python3
"""Write down who stands where, for the NPCs.

Left to itself a model invents geography: it will send you north to a town
that is south, across a mountain range that is not there. It used to be
measured here, off the hand-drawn map. The land is made fresh for every game
now, so there is nothing to measure until the game exists: assets/geography.json
carries the names of the places and which of them each NPC lives in, and no
directions, and the dialogue daemon tells whoever is speaking that they are
not sure of the distances. (The directions themselves are for the game to
send the host when it starts: a later step.)
"""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mktiles
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

    out = {"places": places, "npc_place": npc_place, "from": {}}
    with open("assets/geography.json", "w") as f:
        json.dump(out, f, indent=1, sort_keys=True)
        f.write("\n")

    print("mkgeo: %d places, %d npcs located" % (len(places), len(npc_place)))


if __name__ == "__main__":
    main()
