#!/usr/bin/env python3
"""Work out where everywhere actually is, and write it down for the NPCs.

Left to itself a model invents geography: it will send you north to a town
that is south, across a mountain range that is not there. This measures the
real map -- compass bearing on the wrapping grid, and walking distance by
flood fill over passable ground -- and emits assets/geography.json for the
dialogue daemon to put in front of whoever is speaking.

Walking distance matters more than the straight line: the Sundering runs
down the middle of Keldmarch, so two places a short hop apart as the crow
flies can be a long way round on foot, and an NPC ought to know that.
"""

import json
import math
import os
import sys
from collections import deque

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mktiles
import mktowns

COMPASS = ["east", "north-east", "north", "north-west",
           "west", "south-west", "south", "south-east"]


def bearing(dx, dy):
    # Screen coordinates: y grows southward, so north is -dy.
    angle = math.degrees(math.atan2(-dy, dx))
    return COMPASS[round(angle / 45) % 8]


def walk_distances(rows, walkable, start, w, h):
    """Steps to every reachable square, and the net (dx, dy) travelled to
    get there along that shortest walk.

    The map wraps, so the straight-line bearing can point across the edge
    while the only road runs the other way: the castle and Wardlow are 23
    squares apart southward over the wrap, 70 steps apart northward on
    foot. The direction worth telling a traveller is the one they walk.
    """
    dist = {start: 0}
    offset = {start: (0, 0)}
    queue = deque([start])
    while queue:
        x, y = queue.popleft()
        ox, oy = offset[(x, y)]
        for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            n = ((x + dx) % w, (y + dy) % h)
            if n not in dist and rows[n[1]][n[0]] in walkable:
                dist[n] = dist[(x, y)] + 1
                offset[n] = (ox + dx, oy + dy)
                queue.append(n)
    return dist, offset


def main():
    tiles = mktiles.parse("assets/tiles.txt")
    by_char = {t.mapchar: t for t in tiles}
    walkable = {c for c, t in by_char.items()
                if t.flag not in ("TF_BLOCK", "TF_WATER", "actor")}

    rows = [l.rstrip("\n") for l in open("assets/overworld.txt")
            if l.strip() and not l.startswith("#") and not l.startswith("start")]
    h, w = len(rows), len(rows[0])

    places = {}
    towns = mktowns.parse(by_char)
    mktowns.resolve(towns, by_char)   # fills in where each entity stands
    for t in towns:
        places[t.id] = {"id": t.id, "name": t.name, "x": t.wx, "y": t.wy,
                        "kind": "castle" if "CASTLE" in t.name else "town"}

    for line in open("assets/places.txt"):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        _, pid, x, y, marker, name = line.split(None, 5)
        x, y = int(x), int(y)
        if rows[y][x] != marker:
            sys.exit("places.txt: %s says %d,%d is %r, but the map has %r"
                     % (pid, x, y, marker, rows[y][x]))
        places[pid] = {"id": pid, "name": name, "x": x, "y": y,
                       "kind": "lair" if marker == "L" else "dungeon"}

    # Which place each NPC stands in, taken from where the towns put them.
    npc_place = {}
    for t in towns:
        for _, _, kind, param in t.placed:
            if kind == "npc":
                npc_place[param] = t.id

    lines = {}
    for origin in places.values():
        reach, offset = walk_distances(rows, walkable,
                                       (origin["x"], origin["y"]), w, h)
        here = []
        for other in places.values():
            if other["id"] == origin["id"]:
                continue
            steps = reach.get((other["x"], other["y"]))
            if steps is None:
                here.append("%s cannot be reached on foot from here."
                            % other["name"])
                continue

            # Bearing and straight line both follow the walk, so the two
            # never disagree about which way round the world you are going.
            dx, dy = offset[(other["x"], other["y"])]
            crow = max(abs(dx), abs(dy))

            note = ""
            # A walk half again as long as the straight line means the
            # mountains or the coast are in the way, and saying so is the
            # difference between a useful direction and a wrong one.
            if steps > crow * 1.5 + 4:
                note = ", though the road bends a long way round"
            here.append("%s lies %s, about %d leagues on foot%s."
                        % (other["name"], bearing(dx, dy), steps, note))
        lines[origin["id"]] = here

    out = {"places": places, "npc_place": npc_place, "from": lines}
    with open("assets/geography.json", "w") as f:
        json.dump(out, f, indent=1, sort_keys=True)
        f.write("\n")

    print("mkgeo: %d places, %d npcs located" % (len(places), len(npc_place)))
    for line in lines["millford"]:
        print("  from Millford: %s" % line)


if __name__ == "__main__":
    main()
