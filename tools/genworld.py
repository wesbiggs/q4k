#!/usr/bin/env python3
"""Author a draft overworld into assets/overworld.txt.

This is a one-shot authoring aid, not a build step: it runs from a fixed seed,
the result is checked in, and from then on the map is hand-edited. Re-running
it throws away those edits, which is why the Makefile never calls it.

The world wraps in both axes (Ultima-style), so the continent is ringed by
ocean and the seams meet water on water.
"""

import random
import sys

W = H = 64
SEED = 0x51344B  # 'Q4K'


def smooth_noise(rng, coarse=8):
    """Value noise: random values on a coarse grid, bilinearly interpolated."""
    grid = [[rng.random() for _ in range(coarse + 1)] for _ in range(coarse + 1)]
    step = W / coarse
    field = []
    for y in range(H):
        gy, fy = divmod(y / step, 1)
        gy = int(gy)
        row = []
        for x in range(W):
            gx, fx = divmod(x / step, 1)
            gx = int(gx)
            a = grid[gy][gx] * (1 - fx) + grid[gy][gx + 1] * fx
            b = grid[gy + 1][gx] * (1 - fx) + grid[gy + 1][gx + 1] * fx
            row.append(a * (1 - fy) + b * fy)
        field.append(row)
    return field


def island_mask(x, y):
    """1.0 at the centre falling to 0.0 at the edges, so the map is an island
    and the wrap seam is open ocean."""
    dx = (x - W / 2) / (W / 2)
    dy = (y - H / 2) / (H / 2)
    d = min(1.0, (dx * dx + dy * dy) ** 0.5)
    return 1.0 - d * d


def main():
    rng = random.Random(SEED)
    base = smooth_noise(rng, 8)
    detail = smooth_noise(rng, 16)

    grid = []
    for y in range(H):
        row = []
        for x in range(W):
            h = (base[y][x] * 0.7 + detail[y][x] * 0.3) * island_mask(x, y)
            if h < 0.16:
                c = "~"
            elif h < 0.21:
                c = ","
            elif h < 0.26:
                c = "%" if rng.random() < 0.35 else "."
            elif h < 0.40:
                c = "."
            elif h < 0.50:
                c = "F"
            else:
                c = "^"
            row.append(c)
        grid.append(row)

    # Settlements go on open ground with open ground around them, kept apart so
    # the player always has somewhere new to walk to.
    placed = []

    def spot(want, min_dist):
        for _ in range(4000):
            x, y = rng.randrange(4, W - 4), rng.randrange(4, H - 4)
            if grid[y][x] != ".":
                continue
            if any(grid[y + dy][x + dx] in "~^"
                   for dy in (-1, 0, 1) for dx in (-1, 0, 1)):
                continue
            if any(abs(x - px) + abs(y - py) < min_dist for px, py in placed):
                continue
            grid[y][x] = want
            placed.append((x, y))
            return x, y
        sys.exit("could not place %r -- loosen the constraints" % want)

    start = spot("C", 0)          # the castle is where the game begins
    for _ in range(4):
        spot("T", 14)
    for _ in range(3):
        spot("D", 12)

    with open("assets/overworld.txt", "w") as f:
        f.write("# q4k overworld -- %dx%d, wraps in both axes.\n" % (W, H))
        f.write("# Drafted by tools/genworld.py, hand-edited from here on.\n")
        f.write("# Legend: see assets/tiles.txt. start = %d,%d\n"
                % (start[0], start[1]))
        f.write("start %d %d\n" % start)
        for row in grid:
            f.write("".join(row) + "\n")

    counts = {}
    for row in grid:
        for c in row:
            counts[c] = counts.get(c, 0) + 1
    print("genworld: %dx%d, start %d,%d" % (W, H, start[0], start[1]))
    print("  " + "  ".join("%s=%d" % kv for kv in sorted(counts.items())))


if __name__ == "__main__":
    main()
