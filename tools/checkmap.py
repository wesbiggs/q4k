#!/usr/bin/env python3
"""Check that everywhere the quest sends you can actually be walked to.

Two hand-edited things can quietly break the game, and no screen test would
notice either. A town can end up sealed behind the mountains. And because
people block movement, a shopkeeper standing in a doorway can wall off the
NPC behind them -- which is exactly how the smith became unreachable.

This floods the overworld from the start position, and each town from its
arrival square, and fails if anything the quest needs -- or any treasure in
assets/loot.txt -- cannot be walked to.
"""

import os
import sys
from collections import deque

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mkloot
import mktiles

SRC = "assets/overworld.txt"


def flood(passable, start, w, h, wrap):
    seen = {start}
    queue = deque([start])
    while queue:
        x, y = queue.popleft()
        for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            nx, ny = x + dx, y + dy
            if wrap:
                nx, ny = nx % w, ny % h
            elif not (0 <= nx < w and 0 <= ny < h):
                continue
            if (nx, ny) in seen or not passable(nx, ny):
                continue
            seen.add((nx, ny))
            queue.append((nx, ny))
    return seen


def check_towns(by_char, walkable, loot):
    """Every entity must be reachable, and entities block the way. So must
    every treasure, which you have to stand on rather than beside."""
    import mktowns

    towns = mktowns.parse(by_char)
    bad = 0
    for t in towns:
        ent_at = {}
        grid = [list(r) for r in t.rows]
        start = None
        for y, row in enumerate(grid):
            for x, c in enumerate(row):
                if c == "$":
                    start = (x, y)
                elif c in t.ents:
                    ent_at[(x, y)] = t.ents[c]

        def passable(x, y, grid=grid, ent_at=ent_at):
            if (x, y) in ent_at:
                return False          # you cannot walk through people
            c = grid[y][x]
            return c in walkable or c in "$x"

        seen = flood(passable, start, mktowns.W, mktowns.H, False)
        for (x, y), (kind, param) in sorted(ent_at.items()):
            # Reachable means you can stand next to them and walk into them.
            ok = any((x + dx, y + dy) in seen
                     for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0)))
            print("  %-11s %-6s %-8s %2d,%-2d  %s"
                  % (t.id, kind, param, x, y,
                     "reachable" if ok else "UNREACHABLE"))
            bad += not ok
        for where, x, y, kind, amount, _ in loot:
            if where != t.id:
                continue
            ok = (x, y) in seen
            print("  %-11s %-6s %-8s %2d,%-2d  %s"
                  % (t.id, "loot", "%d %s" % (amount, kind), x, y,
                     "reachable" if ok else "UNREACHABLE"))
            bad += not ok
    return bad


def main():
    tiles = mktiles.parse("assets/tiles.txt")
    by_char = {t.mapchar: t for t in tiles}
    # TF_BLOCK and TF_WATER stop you; everything else you can step on.
    walkable = {c for c, t in by_char.items()
                if t.flag not in ("TF_BLOCK", "TF_WATER", "actor")}

    rows, start = [], None
    for line in open(SRC):
        line = line.rstrip("\n")
        if line.startswith("start "):
            _, x, y = line.split()
            start = (int(x), int(y))
        elif line.strip() and not line.startswith("#"):
            rows.append(line)

    h, w = len(rows), len(rows[0])
    seen = flood(lambda x, y: rows[y][x] in walkable, start, w, h, True)

    targets = []
    for y in range(h):
        for x in range(w):
            c = rows[y][x]
            if c in "TCDL":
                targets.append((c, x, y))

    label = {"T": "town", "C": "castle", "D": "dungeon", "L": "lair"}
    bad = 0
    for c, x, y in targets:
        ok = (x, y) in seen
        print("  %-8s %2d,%-2d  %s" % (label[c], x, y,
                                       "reachable" if ok else "UNREACHABLE"))
        bad += not ok

    loot = mkloot.parse()
    for where, x, y, kind, amount, _ in loot:
        if where != mkloot.WORLD:
            continue
        ok = (x, y) in seen
        print("  %-8s %2d,%-2d  %s" % ("loot", x, y,
                                       "reachable" if ok else "UNREACHABLE"))
        bad += not ok

    bad += check_towns(by_char, walkable, loot)

    print("checkmap: %d overworld targets, %d walkable squares, %d problems"
          % (len(targets), len(seen), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
