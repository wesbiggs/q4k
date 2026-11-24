#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""A bit-exact model of src/lib/isle_gen.s, the land generator.

The C64 code has its own random numbers (a 16-bit LFSR, so building land
never touches cc65's rand()), which makes the whole thing small enough to
model here: the same seed gives the same map, byte for byte, and
tools/checkisle.py compares the two. Also handy for looking at the land
without starting an emulator:

  tools/isleref.py SEED [DIR]        draw one (DIR 0-3: a continent, N E S W;
                                     4: the home continent)

Home (4) and the four continents across the sea are the same generator. Home
has the four ports; a continent has the one landing. Both have places on
them -- towns, a castle, dungeons, a lair -- put where the model says.
"""

import collections
import sys

W, H = 64, 31       # a continent: the 32nd row of local[] is the row buffer
HOME_H = 64         # home is built (and kept) outside local[]
BIOMES = 5
HOME = 4
# The walker goes back to the middle every INNER steps, OUTER times, which
# keeps the land in one piece around it rather than strung out along wherever
# the walk wandered. Home is given more land to cover, so each of its rounds
# is longer.
OUTER, INNER = 17, 128
OUTER_HOME, INNER_HOME = 30, 256

# What a square of the map holds while it is being built. Odd is walkable, so
# "can I stand here" is one bit; a walkable square is G_LAND + 2 * its biome.
# The "specials" are even, so as far as connectivity goes they are not there:
# a landing or a town is stepped on, not through. PROT is added to the
# walkable squares beside a landing so no obstacle takes the last of them.
G_SEA, G_LAND, G_MTN, G_LAKE = 0, 1, 2, 4
G_DOCK, G_TOWN, G_CASTLE, G_DUNGEON, G_LAIR = 0x0A, 0x0C, 0x0E, 0x10, 0x12
# The shallows round the land (shoal()): the sea within a few squares of
# it, which a pass marks G_NEW and makes G_SHOAL after; the rest is deep.
G_SHOAL, G_NEW = 0x14, 0x16
SHOAL_PASSES = 5
PROT = 0x40

# Tile codes -- see assets/tiles.txt -- by G_ value. The odd ones above 9 and
# a few below are never held.
T_WATER, T_GRASS, T_MOUNTAIN, T_FOREST = 0x64, 0x60, 0x62, 0x61
T_SWAMP, T_SHORE = 0x66, 0x65
T_TOWN, T_CASTLE, T_DUNGEON, T_LAIR = 0x67, 0x68, 0x69, 0x71
T_DEEP = 0x95       # the overworld set's own (assets/overworld.txt's code)
FINAL = [T_DEEP, T_GRASS, T_MOUNTAIN, T_FOREST, T_WATER, T_SWAMP,
         T_GRASS, T_SHORE, T_GRASS, T_GRASS, T_SHORE, T_GRASS,
         T_TOWN, T_GRASS, T_CASTLE, T_GRASS, T_DUNGEON, T_GRASS, T_LAIR,
         T_GRASS, T_WATER]
WALK = {T_GRASS, T_FOREST, T_SWAMP, T_SHORE}
SEA = {T_WATER, T_DEEP}
SPECIAL = {T_TOWN, T_CASTLE, T_DUNGEON, T_LAIR}

STEP = [-W, 1, W, -1]                      # N E S W, as indexes into the map
RING = [0, -W, 1, W, -1, -W - 1, -W + 1, W + 1, W - 1]


def start(h):
    """Where a ship heading N E S W comes in: the middle of the opposite edge."""
    return [(W // 2, h - 1), (0, h // 2), (W // 2, 0), (W - 1, h // 2)]

# The places, as (what, how far from the others it would like to be). A
# newcomer must be at least this far (the larger of the two distances along
# either axis) from every place already put down, and three from a landing;
# the distance is given up two at a time every 128 tries, to four, so that a
# small continent cannot stop it. The first of a continent's two places is
# its story: a town or a dungeon.
HOME_PLACES = [(G_TOWN, 0), (G_CASTLE, 24), (G_DUNGEON, 12), (G_LAIR, 12),
               (G_TOWN, 10)]
LANDING_APART = 3

Land = collections.namedtuple(
    "Land", "tiles dom docks start places")


class Isle:
    def __init__(self, lore_seed, d):
        self.h = HOME_H if d == HOME else H
        # The seed: the game's byte over one for the compass point, then
        # eight draws thrown away so that nearby seeds part company.
        self.s = (lore_seed & 0xFF) << 8 | ((d << 3) | 5)
        for _ in range(8):
            self.rnd()
        self.m = [G_SEA] * (W * self.h)
        self.at = 0

    def rnd(self):
        """A Galois LFSR, taps $B400, advanced eight bits a draw."""
        s = self.s
        for _ in range(8):
            carry = s & 1
            s >>= 1
            if carry:
                s ^= 0xB400
        self.s = s
        return s & 0xFF

    def walk(self):
        r = self.rnd()
        to = self.at + STEP[r & 3]
        if 4 <= (to & 63) < W - 4 and 3 * W <= to < (self.h - 3) * W:
            self.at = to
        return r

    def grow(self, dom, outer, inner):
        self.at = (self.h // 2) * W + W // 2
        lay = G_LAND + 2 * dom
        for i in range(outer * inner):
            if i % inner == 0:
                self.at = (self.h // 2) * W + W // 2
            r = self.walk()
            if self.rnd() & 63 == 0:
                k = self.rnd() & 15
                lay = G_LAND + 2 * (k if k < BIOMES else dom)
            for k in range(9 if r & 0x0C else 5):
                self.m[self.at + RING[k]] = lay

    def removable(self, p):
        """Can the walkable square p stop being one without cutting any of
        the rest off? A purely local test: sufficient, not necessary."""
        b = [0] + [self.m[p + RING[k]] & 1 for k in range(1, 9)]
        n = b[1] + b[2] + b[3] + b[4]
        e = ((b[1] & b[2] & b[6]) + (b[2] & b[3] & b[7])
             + (b[3] & b[4] & b[8]) + (b[4] & b[1] & b[5]))
        return n != 0 and e + 1 >= n

    def pick(self):
        """A random square at least four from the sides, three from the ends."""
        while True:
            x = self.rnd() & 63
            if not 4 <= x < W - 4:
                continue
            y = self.rnd() & (63 if self.h > 32 else 31)
            if not 3 <= y < self.h - 3:
                continue
            return x, y

    def obstacle(self, code, length):
        while True:
            x, y = self.pick()
            if self.m[y * W + x] & 1:
                break
        self.at = y * W + x
        for _ in range(length):
            p = self.at
            m = self.m[p]
            if m & 1 and not m & PROT and self.removable(p):
                self.m[p] = code
            self.walk()

    def dock(self, d):
        """Where a ship heading d comes ashore: the first land along the
        middle row or column from the edge it comes in at."""
        sx, sy = start(self.h)[d]
        i = sy * W + sx
        while not self.m[i] & 1:
            i += STEP[d]
        self.m[i] = G_DOCK
        for k in (1, 2, 3, 4):
            if self.m[i + RING[k]] & 1:
                self.m[i + RING[k]] |= PROT
        return sx, sy, i % W, i // W

    def place(self, kind, apart, landings, placed):
        """Put a place down: on land that can be spared, far enough from the
        rest and, for a lair, with a mountain beside it."""
        tries = 0
        while True:
            tries += 1
            if tries & 127 == 0 and apart > 4:
                apart -= 2
            x, y = self.pick()
            p = y * W + x
            m = self.m[p]
            if not m & 1 or m >= G_DOCK:
                continue
            if not self.removable(p):
                continue
            if kind == G_LAIR and not any(self.m[p + STEP[k]] == G_MTN
                                          for k in range(4)):
                continue
            if any(max(abs(x - px), abs(y - py)) < LANDING_APART
                   for px, py in landings):
                continue
            if any(max(abs(x - px), abs(y - py)) < apart
                   for px, py in placed):
                continue
            self.m[p] = kind
            placed.append((x, y))
            return


    def shoal(self):
        """The shallows: SHOAL_PASSES passes, each over every square but the
        map's edges, row by row, which makes a square of sea shallow if any
        of its eight neighbours is not sea (land, a lake, or shallows from a
        pass before): the first pass always, the rest three times in four,
        so the shallows are one to five squares wide and ragged."""
        for p in range(SHOAL_PASSES):
            for y in range(1, self.h - 1):
                for x in range(1, W - 1):
                    i = y * W + x
                    if self.m[i] != G_SEA:
                        continue
                    if not any(self.m[i + RING[k]] not in (G_SEA, G_NEW)
                               for k in range(1, 9)):
                        continue
                    if p == 0 or self.rnd() & 3:
                        self.m[i] = G_NEW
            self.m = [G_SHOAL if c == G_NEW else c for c in self.m]
        # Then no deep square left alone between shallows: one with them
        # (or land) both west and east of it, or both north and south.
        def held(c):
            return c not in (G_SEA, G_NEW)
        for y in range(1, self.h - 1):
            for x in range(1, W - 1):
                i = y * W + x
                if self.m[i] == G_SEA and (
                        held(self.m[i - 1]) and held(self.m[i + 1]) or
                        held(self.m[i - W]) and held(self.m[i + W])):
                    self.m[i] = G_NEW
        self.m = [G_SHOAL if c == G_NEW else c for c in self.m]


def build(lore_seed, d, story=G_TOWN):
    """Returns a Land: the W*H tile codes; the dominant biome; the landings
    (a continent's one, or home's four in the order of the ports they serve)
    as (x, y); the ship's starting square as (x, y), for a continent; and the
    places, in the order the list above gives them. `story` is the kind of the
    first place on a continent."""
    home = d == HOME
    g = Isle(lore_seed, d)
    while True:
        dom = g.rnd() & 7
        if dom < (2 if home else BIOMES):
            break
    if home:
        g.grow(dom, OUTER_HOME, INNER_HOME)
    else:
        g.grow(dom, OUTER, INNER)
    landings, ship = [], None
    if home:
        for p in range(4):
            landings.append(g.dock(p ^ 2)[2:])
    else:
        sx, sy, dx, dy = g.dock(d)
        landings.append((dx, dy))
        ship = (sx, sy)
    n = (4 if home else 3) + (g.rnd() & 3) + (10 if dom == 4 else 0)
    for _ in range(n):
        g.obstacle(G_MTN, 20 + (g.rnd() & 15))
    n = 1 + (g.rnd() & 1) + (4 if dom == 2 else 0)
    for _ in range(n):
        g.obstacle(G_LAKE, 14 + (g.rnd() & 7))
    wanted = HOME_PLACES if home else [(story, 8), (G_TOWN, 8)]
    placed = []
    for kind, apart in wanted:
        g.place(kind, apart, landings, placed)
    g.shoal()
    return Land([FINAL[c & 0x3F] for c in g.m], dom, landings, ship, placed)


GLYPH = {T_WATER: "-", T_DEEP: "~", T_GRASS: ".", T_FOREST: "F", T_SWAMP: "%",
         T_SHORE: ",", T_MOUNTAIN: "^", T_TOWN: "T",
         T_CASTLE: "C", T_DUNGEON: "D", T_LAIR: "L"}
NAMES = ["plains", "forest", "swamp", "desert", "mountains"]


def reachable(tiles):
    h = len(tiles) // W
    """(walkable squares, those that can be reached from the first, specials
    with no walkable square beside them): every plain walkable square must be
    reachable without passing through a landing or a town."""
    walk = [i for i, t in enumerate(tiles) if t in WALK]
    seen = {walk[0]}
    todo = [walk[0]]
    while todo:
        i = todo.pop()
        x, y = i % W, i // W
        for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            if 0 <= x + dx < W and 0 <= y + dy < h:
                j = i + dy * W + dx
                if j not in seen and tiles[j] in WALK:
                    seen.add(j)
                    todo.append(j)
    cut = 0
    for i, t in enumerate(tiles):
        if t in SPECIAL:
            x, y = i % W, i // W
            if not any(0 <= x + dx < W and 0 <= y + dy < h
                       and tiles[i + dy * W + dx] in WALK
                       for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0))):
                cut += 1
    return len(walk), len(seen), cut


def main():
    seed = int(sys.argv[1], 0)
    d = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    land = build(seed, d)
    for y in range(len(land.tiles) // W):
        print("".join(GLYPH[t] for t in land.tiles[y * W:(y + 1) * W]))
    print("seed %d dir %d: %s, landings %s, ship from %s, places %s, "
          "walkable/reached/stranded %s"
          % (seed, d, NAMES[land.dom], land.docks, land.start, land.places,
             reachable(land.tiles)))


if __name__ == "__main__":
    main()
