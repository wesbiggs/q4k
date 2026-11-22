#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check that everywhere in the towns drawn by hand can actually be walked to.

Hand-edited towns can quietly break the game, and no screen test would
notice. Because people block movement, a shopkeeper standing in a doorway can
wall off the NPC behind them -- which is exactly how the smith became
unreachable. (The land itself is generated, and tools/checkisle.py searches
that.)

This floods each town from its arrival square, and fails if anything in it --
or any treasure in assets/loot.txt -- cannot be walked to.
"""

import os
import sys
from collections import deque

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mkloot
import mktiles


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

        seen = flood(passable, start, t.w, t.h, False)
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

    loot = mkloot.parse()
    bad = 0
    bad += check_towns(by_char, walkable, loot)

    print("checkmap: %d problems" % bad)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
