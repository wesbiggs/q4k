#!/usr/bin/env python3
"""The keys that walk from one place on home to another, for screen tests.

Home is made from the game's seed, so a test that wants to be at the castle or
on a port cannot poke coordinates in advance: it walks, and this works out the
walk from the model of the generator (tools/isleref.py), which is bit-exact.

  tools/homepath.py SEED FROM TO       FROM and TO are millford, castle,
                                       crown, lair, town, port0..port3
                                       (0 north, 1 east, 2 south, 3 west), or
                                       x,y. Prints WASD keys; the last
                                       step is onto TO.
  --stop-before                        leave off that last step
  --land D                             walk on the continent across the sea from
                                       port D instead, where FROM and TO are
                                       story, town, or dock (the landing)
"""

import argparse
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import isleref as ref

NAMES = {"millford": 0, "castle": 1, "crown": 2, "lair": 3, "town": 4}
ISLE_NAMES = {"story": 0, "town": 1}


def slot_of(seed, d):
    """src/lib/game.c's slot_of(), in Python."""
    p = [0, 1, 2, 3]
    v = seed
    for i in (3, 2, 1):
        v = (v * 5 + 3) & 255
        j = (v >> 5) % (i + 1)
        p[i], p[j] = p[j], p[i]
    return p[d]
KEYS = {(0, -1): "w", (1, 0): "d", (0, 1): "s", (-1, 0): "a"}


def where(land, name, isle=False):
    if isle and name in ISLE_NAMES:
        return land.places[ISLE_NAMES[name]]
    if isle and name == "dock":
        return land.docks[0]
    if name.startswith("port"):
        return land.docks[int(name[4:])]
    if "," in name:
        x, y = name.split(",")
        return int(x), int(y)
    return land.places[NAMES[name]]


def path(land, start, goal):
    """Shortest walk from start to goal over plain walkable squares, the last
    step onto goal, which is a place and so is not walked through."""
    tiles = land.tiles
    prev = {start: None}
    todo = collections.deque([start])
    while todo:
        cur = todo.popleft()
        if cur == goal:
            break
        for (dx, dy) in KEYS:
            n = (cur[0] + dx, cur[1] + dy)
            if n in prev or not (0 <= n[0] < ref.W
                                 and 0 <= n[1] < len(tiles) // ref.W):
                continue
            t = tiles[n[1] * ref.W + n[0]]
            if t in ref.WALK or n == goal:
                prev[n] = cur
                todo.append(n)
    if goal not in prev:
        sys.exit("homepath: no way from %s to %s" % (start, goal))
    steps = []
    cur = goal
    while prev[cur]:
        p = prev[cur]
        steps.append(KEYS[(cur[0] - p[0], cur[1] - p[1])])
        cur = p
    return "".join(reversed(steps))


def hop(land):
    """Two keys: a step off the landing on to a square beside it, and back --
    which is how a test sails home."""
    dock = land.docks[0]
    for (dx, dy), key in KEYS.items():
        n = (dock[0] + dx, dock[1] + dy)
        if land.tiles[n[1] * ref.W + n[0]] in ref.WALK:
            return key + KEYS[(-dx, -dy)]
    sys.exit("homepath: nowhere to step from the landing")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("seed", type=lambda s: int(s, 0))
    ap.add_argument("start")
    ap.add_argument("goal")
    ap.add_argument("--stop-before", action="store_true")
    ap.add_argument("--land", type=int, default=ref.HOME)
    args = ap.parse_args()
    isle = args.land != ref.HOME
    if isle:
        story = ref.G_DUNGEON if slot_of(args.seed, args.land) & 1 else ref.G_TOWN
        land = ref.build(args.seed, args.land, story)
    else:
        land = ref.build(args.seed, ref.HOME)
    if args.goal == "hop":
        print(hop(land))
        return
    keys = path(land, where(land, args.start, isle),
                where(land, args.goal, isle))
    print(keys[:-1] if args.stop_before else keys)


if __name__ == "__main__":
    main()
