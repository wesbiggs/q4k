#!/usr/bin/env python3
"""A bit-exact model of src/lib/isle_gen.s, the continent generator.

The C64 code has its own random numbers (a 16-bit LFSR, so building a
continent never touches cc65's rand()), which makes the whole thing small
enough to model here: the same seed gives the same map, byte for byte, and
tools/checkisle.py compares the two. Also handy for looking at continents
without starting an emulator:

  tools/isleref.py SEED [DIR]        draw one (DIR 0-3: N E S W)
"""

import sys

W, H = 64, 31       # the 32nd row of local[] is the overworld's row buffer
BIOMES = 5
# The walker goes back to the middle every INNER steps, OUTER times, which
# keeps the land in one piece around it rather than strung out along wherever
# the walk wandered.
OUTER, INNER = 17, 128
GROW = OUTER * INNER
RESTART = INNER

G_SEA, G_LAND, G_MTN, G_LAKE, G_DOCK = 0, 1, 2, 4, 0x0B

# Tile codes -- see assets/tiles.txt -- by G_ value. Index 6, 8 and 10 are
# never held.
T_WATER, T_GRASS, T_MOUNTAIN, T_FOREST = 0x64, 0x60, 0x62, 0x61
T_SWAMP, T_SHORE, T_PORT = 0x66, 0x65, 0x7E
FINAL = [T_WATER, T_GRASS, T_MOUNTAIN, T_FOREST, T_WATER, T_SWAMP,
         T_GRASS, T_SHORE, T_GRASS, T_GRASS, T_GRASS, T_PORT]

STEP = [-W, 1, W, -1]                      # N E S W, as indexes into local[]
RING = [0, -W, 1, W, -1, -W - 1, -W + 1, W + 1, W - 1]
START = [(W // 2, H - 1), (0, H // 2), (W // 2, 0), (W - 1, H // 2)]


class Isle:
    def __init__(self, lore_seed, d):
        # The seed: the game's byte over one for the compass point, then
        # eight draws thrown away so that nearby seeds part company.
        self.s = (lore_seed & 0xFF) << 8 | ((d << 3) | 5)
        for _ in range(8):
            self.rnd()
        self.m = [G_SEA] * (W * H)
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
        if 4 <= (to & 63) < W - 4 and 3 * W <= to < (H - 3) * W:
            self.at = to
        return r

    def grow(self, dom):
        self.at = (H // 2) * W + W // 2
        lay = G_LAND + 2 * dom
        for i in range(GROW):
            if i % RESTART == 0:
                self.at = (H // 2) * W + W // 2
            r = self.walk()
            if self.rnd() & 63 == 0:
                k = self.rnd() & 15
                lay = G_LAND + 2 * (k if k < BIOMES else dom)
            for k in range(9 if r & 0x0C else 5):
                self.m[self.at + RING[k]] = lay

    def removable(self, p):
        b = [0] + [self.m[p + RING[k]] & 1 for k in range(1, 9)]
        n = b[1] + b[2] + b[3] + b[4]
        e = ((b[1] & b[2] & b[6]) + (b[2] & b[3] & b[7])
             + (b[3] & b[4] & b[8]) + (b[4] & b[1] & b[5]))
        return n != 0 and e + 1 >= n

    def obstacle(self, code, length):
        while True:
            x = self.rnd() & 63
            if not 4 <= x < W - 4:
                continue
            y = self.rnd() & 31
            if not 3 <= y < H - 3:
                continue
            if self.m[y * W + x] & 1:
                break
        self.at = y * W + x
        for _ in range(length):
            p = self.at
            if self.m[p] & 1 and self.m[p] != G_DOCK and self.removable(p):
                self.m[p] = code
            self.walk()

    def dock(self, d):
        sx, sy = START[d]
        i = sy * W + sx
        while not self.m[i] & 1:
            i += STEP[d]
        self.m[i] = G_DOCK
        return sx, sy, i % W, i // W


def build(lore_seed, d):
    """Returns (tiles, dock, start, biome): the W*H tile codes, and the
    landing and the ship's starting square as (x, y)."""
    g = Isle(lore_seed, d)
    while True:
        dom = g.rnd() & 7
        if dom < BIOMES:
            break
    g.grow(dom)
    sx, sy, dx, dy = g.dock(d)
    n = 3 + (g.rnd() & 3) + (10 if dom == 4 else 0)
    for _ in range(n):
        g.obstacle(G_MTN, 20 + (g.rnd() & 15))
    n = 1 + (g.rnd() & 1) + (4 if dom == 2 else 0)
    for _ in range(n):
        g.obstacle(G_LAKE, 14 + (g.rnd() & 7))
    return [FINAL[c] for c in g.m], (dx, dy), (sx, sy), dom


GLYPH = {T_WATER: "~", T_GRASS: ".", T_FOREST: "F", T_SWAMP: "%",
         T_SHORE: ",", T_MOUNTAIN: "^", T_PORT: "P"}
NAMES = ["plains", "forest", "swamp", "desert", "mountains"]


def reachable(tiles, dock):
    walk = {T_GRASS, T_FOREST, T_SWAMP, T_SHORE, T_PORT}
    seen = {dock}
    todo = [dock]
    while todo:
        x, y = todo.pop()
        for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            n = (x + dx, y + dy)
            if (0 <= n[0] < W and 0 <= n[1] < H and n not in seen
                    and tiles[n[1] * W + n[0]] in walk):
                seen.add(n)
                todo.append(n)
    total = sum(1 for t in tiles if t in walk)
    return total, len(seen)


def main():
    seed = int(sys.argv[1], 0)
    d = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    tiles, dock, start, dom = build(seed, d)
    for y in range(H):
        print("".join(GLYPH[t] for t in tiles[y * W:(y + 1) * W]))
    print("seed %d dir %d: %s, dock %s, ship from %s, walkable/reached %s"
          % (seed, d, NAMES[dom], dock, start, reachable(tiles, dock)))


if __name__ == "__main__":
    main()
