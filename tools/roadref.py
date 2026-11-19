#!/usr/bin/env python3
"""A model of src/lib/ow_roads.s: the open world's roads on home.

After home is built, the bootstrap (src/openworld.c) lays one network of
roads joining Millford, the castle, home's own town and the four ports. It
is Prim's algorithm on the grid: a breadth-first search from everything
already joined (Millford at first, then every road square and every place
joined so far) stops at the first place not yet joined that it meets, which
is therefore the nearest, and the way back to the network is laid as road;
then again, until every place is joined. A road is laid only on grass,
forest, sand and swamp -- never on a mountain or water, nor through another
place -- and four ways, never diagonally. On the way back it keeps going the
way it was going while it can, so a road runs straight and turns as few
times as it may.

The search keeps each square's distance mod 3, plus one (0: not seen), which
is all the way back needs: a neighbour one nearer is the one whose number is
one less, round the cycle. This is the C64's algorithm step for step, so the
same land gives the same roads.

  tools/roadref.py SEED            draw home with its roads
  tools/roadref.py --check N       every seed below N: all joined, no road on
                                   anything but grass, forest, sand or swamp
  tools/roadref.py --machine SEED  the open world's bootstrap on an emulated
                                   C64 (dist/openworld.d81): its home, roads
                                   and all, read out of the RAM under the I/O
                                   area and held up against the model's, for
                                   the tests' quick start (seed 1) and then
                                   SEED (N in the engine)
"""

import os
import sys
import time

import isleref as ir

W = H = 64
T_ROAD = 0x96           # assets/overworld.txt's `code road`
ROADABLE = {ir.T_GRASS, ir.T_FOREST, ir.T_SHORE, ir.T_SWAMP}
# Millford, the castle and home's town, in home's places (isle.h's order).
PLACES = (0, 1, 4)
STEPS = ((0, -1), (1, 0), (0, 1), (-1, 0))     # N E S W


BLOCK, ROAD, GOAL = 0x80, 0x40, 0x20     # ow_roads.s's grid's bits
JOINED = BLOCK | 1      # a place joined: a source, at distance 0


def lay(tiles, places, ports):
    """The roads, laid into `tiles` (a list of 64 * 64 tile codes). `places`
    are home's (x, y) in isle_fx's order, `ports` its four landings. The
    grid is a byte a square: BLOCK for what no road crosses, ROAD, GOAL for
    a place not yet joined, and the distance's number in the low two bits."""
    g = [0 if t in ROADABLE else BLOCK for t in tiles]
    for x, y in [places[p] for p in PLACES[1:]] + list(ports):
        g[y * W + x] = BLOCK | GOAL
    x, y = places[PLACES[0]]
    g[y * W + x] = JOINED
    while True:
        # The sources, in the order of the squares: every place joined and
        # every road, at distance 0 (1); the last search's numbers cleared.
        queue = []
        for i in range(W * H):
            v = g[i]
            if v & BLOCK:
                if v == JOINED:
                    queue.append(i)
            else:
                v &= ROAD
                if v:
                    v |= 1
                    queue.append(i)
                g[i] = v
        head, found = 0, None
        while head < len(queue) and found is None:
            i = queue[head]
            head += 1
            x, y = i % W, i // W
            nv = 1 + (g[i] & 3) % 3
            for dx, dy in STEPS:
                if not (0 <= x + dx < W and 0 <= y + dy < H):
                    continue
                j = i + dy * W + dx
                if g[j] & GOAL:
                    found = (i, j)
                    break
                if g[j] == 0:
                    g[j] = nv
                    queue.append(j)
        if found is None:
            break
        # Joined, once the way back is laid: until then it is not one of
        # the squares the way back may end on.
        i, j = found
        g[j] = BLOCK
        trace(g, i)
        g[j] = JOINED
    for i in range(W * H):
        if g[i] & ROAD:
            tiles[i] = T_ROAD


def trace(g, i):
    """From square i back to distance 0 (a road or a place joined), laying
    road on the way: each step to the neighbour one nearer, the way the last
    one went if it can, else the first of north, east, south and west."""
    d = 0
    while not g[i] & (ROAD | BLOCK):
        g[i] |= ROAD
        v = g[i] & 3
        want = 3 if v == 1 else v - 1
        x, y = i % W, i // W
        for k in [d] + [k for k in range(4) if k != d]:
            dx, dy = STEPS[k]
            if not (0 <= x + dx < W and 0 <= y + dy < H):
                continue
            j = i + dy * W + dx
            if g[j] & 3 == want:
                d = k
                i = j
                break
        else:
            raise AssertionError("no way back from %d" % i)


def build(seed):
    land = ir.build(seed, ir.HOME)
    tiles = list(land.tiles)
    lay(tiles, land.places, land.docks)
    return land, tiles


def check(tiles, land):
    """The places wanted that roads do not join to Millford: roads and those
    places, four ways, are one piece. (Roads on nothing but open ground is
    main()'s to check.)"""
    want = [land.places[p] for p in PLACES] + list(land.docks)
    net = set(want) | {(i % W, i // W) for i, t in enumerate(tiles)
                       if t == T_ROAD}
    seen, todo = {want[0]}, [want[0]]
    while todo:
        x, y = todo.pop()
        for dx, dy in STEPS:
            n = (x + dx, y + dy)
            if n in net and n not in seen:
                seen.add(n)
                todo.append(n)
    return [p for p in want if p not in seen]


GLYPH = dict(ir.GLYPH)
GLYPH[T_ROAD] = "#"


def machine(seed):
    """The C64's home against the model's, for seed 1 and then `seed`: 0 if
    they are the same, square for square."""
    import screendump as sd
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    prg = os.path.join(root, "dist", "openworld.prg")
    port = sd.free_port()
    vice = sd.launch(prg, port, disk=os.path.join(root, "dist",
                                                  "openworld.d81"),
                     fastdisk=True)
    bad = 0
    try:
        mon = sd.Monitor(sd.connect(port))
        mon.resume()
        sd.press_splash(mon, "quick", 120)
        sd.wait_for_stage(mon, 1, 180)
        # VICE's bank 1 of the C64's is its RAM, whatever $01 shows.
        ram_bank = 1
        for n, want in enumerate((1, seed)):
            if n:
                # The next world is seed + 1 (the engine's N).
                syms = sd.load_symbols(prg, os.path.join(root, "build",
                                                         "ow-cpu.lbl"))
                sd.apply_pokes(mon, syms, ["seed=%d" % ((want - 1) & 255)])
                mon.resume()
                mon.type_keys("n")
            land, tiles = build(want)
            deadline = time.time() + 120
            while True:
                mon.resume()
                time.sleep(2)
                home = mon.mem_get(0xD000, 0xDFFF, bank=ram_bank)
                diff = [i for i in range(W * H) if home[i] != tiles[i]]
                if not diff or time.time() > deadline:
                    break
            print("seed %d: %d roads on the C64, %d in the model, %d squares "
                  "differ" % (want, home.count(T_ROAD), tiles.count(T_ROAD),
                              len(diff)))
            for i in diff[:8]:
                print("  %d,%d: C64 $%02X, model $%02X"
                      % (i % W, i // W, home[i], tiles[i]))
            bad += bool(diff)
        mon.quit()
    finally:
        vice.kill()
    return bad


def main():
    if sys.argv[1] == "--machine":
        sys.exit(1 if machine(int(sys.argv[2], 0)) else 0)
    if sys.argv[1] == "--check":
        bad = 0
        for seed in range(int(sys.argv[2])):
            land, tiles = build(seed)
            missing = check(tiles, land)
            for i, (a, b) in enumerate(zip(land.tiles, tiles)):
                if a != b and a not in ROADABLE:
                    print("seed %d: road on %02x at %d,%d" % (seed, a, i % W, i // W))
                    bad += 1
            if missing:
                print("seed %d: not joined: %s" % (seed, missing))
                bad += 1
        print("%d bad" % bad)
        sys.exit(1 if bad else 0)
    land, tiles = build(int(sys.argv[1], 0))
    for y in range(H):
        print("".join(GLYPH[t] for t in tiles[y * W:(y + 1) * W]))
    print("roads: %d squares" % tiles.count(T_ROAD))


if __name__ == "__main__":
    main()
