#!/usr/bin/env python3
"""Write the .cmd files of the screen tests that have to walk somewhere.

Home is made from the game's seed, so these tests cannot poke a position: they
start in Millford, step out, and walk, with keys worked out from the model of
the generator (tools/homepath.py). Run this after changing the generator, or
the seed; the goldens then need blessing.

  tools/mkcmds.py
"""

import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import homepath
import mktiles
import mktowns

SEED = 0x5A
POKES = ["--poke", "lore_seed=0x%x" % SEED, "--poke", "no_encounters=1",
         "--timeout", "90"]
OUT = "tests/golden/%s.cmd"


def keys(*words, land=None):
    return subprocess.check_output(
        [sys.executable, "tools/homepath.py", str(SEED)] + list(words)
        + (["--land", str(land)] if land is not None else []),
        text=True).strip()


def town_keys(town, goals):
    """Keys that walk through a drawn town, from where you arrive to each of
    the squares in `goals` in turn, round the people (who block the way)."""
    import collections
    by_char = {t.mapchar: t for t in mktiles.parse("assets/tiles.txt")}
    t = [t for t in mktowns.parse(by_char) if t.id == town][0]
    people = {(x, y) for y, row in enumerate(t.rows)
              for x, c in enumerate(row) if c in t.ents}
    start = [(x, y) for y, row in enumerate(t.rows)
             for x, c in enumerate(row) if c == "$"][0]

    def passable(x, y):
        c = t.rows[y][x]
        return (x, y) not in people and (c in "$x" or (
            c in by_char and by_char[c].flag not in ("TF_BLOCK", "TF_WATER")))

    out, cur = "", start
    for goal in goals:
        prev, todo = {cur: None}, collections.deque([cur])
        while todo:
            c = todo.popleft()
            for (dx, dy), k in homepath.KEYS.items():
                n = (c[0] + dx, c[1] + dy)
                if (n not in prev and 0 <= n[0] < mktowns.W
                        and 0 <= n[1] < mktowns.H and passable(*n)):
                    prev[n] = c
                    todo.append(n)
        steps, c = "", goal
        while prev[c]:
            p = prev[c]
            steps = homepath.KEYS[(c[0] - p[0], c[1] - p[1])] + steps
            c = p
        out += steps + "l"
        cur = goal
    return out


def write(name, args):
    with open(OUT % name, "w") as f:
        f.write("dist/q4k.prg " + " ".join(args) + "\n")


def main():
    south = homepath.slot_of(SEED, 2)           # which story slot lies south
    assert south == 0, "the tests assume the south continent opens first"
    to_port2 = keys("millford", "port2")
    to_port0 = keys("millford", "port0")
    hop = keys("dock", "hop", land=2)

    # out of Millford, down to the south port, and out to sea
    write("sail", POKES + ["--poke", "quest_stage=3",
                           "--keys", "Alaric\\rs" + to_port2,
                           "--stage", str(len(to_port2) + 3)])
    # ... and back again, by stepping off the landing and on to it
    write("sailhome", POKES + ["--poke", "quest_stage=3",
                               "--keys", "Alaric\\rs" + to_port2 + hop,
                               "--stage", str(len(to_port2) + 6)])
    # the castle's treasures: walk to it, in, and look about
    to_castle = keys("millford", "castle")
    inside = town_keys("castle", [(6, 7), (14, 7)])
    write("loot", POKES + ["--poke", "quest_stage=1", "--keys", "Alaric\\rs" + to_castle + inside,
                           "--stage", str(len(to_castle) + len(inside) + 1)])
    # the first dungeon, with the Sigil in hand: the lit room round the stairs
    to_crown = keys("millford", "crown")
    write("fov", POKES + ["--poke", "quest_stage=2", "--poke", "items=1",
                          "--keys", "Alaric\\rs" + to_crown,
                          "--stage", str(len(to_crown) + 2)])
    # the north port is not open until the story has got further
    write("captain", POKES + ["--poke", "quest_stage=3",
                              "--keys", "Alaric\\rs" + to_port0,
                              "--stage", str(len(to_port0) + 2)])


if __name__ == "__main__":
    main()
