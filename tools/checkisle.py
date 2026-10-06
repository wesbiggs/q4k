#!/usr/bin/env python3
"""Check the continent generator, on the C64 and off it.

The generator (src/lib/isle_gen.s) promises that every walkable square of a
continent can be walked to from its landing, whatever mountains and lakes
there are. It also has to build the same land every time. This checks both:

  1. tools/isleref.py is a bit-exact model of it, so the model's output is
     searched for squares that cannot be reached -- over every game seed and
     every compass point, which a program on the machine could not afford --
     and for a landing that does not sit where the ship comes in.
  2. dist/isletest.prg builds the same continents on an emulated C64 and
     keeps a checksum of each; they must equal the model's. One whole map is
     then read out of memory and compared square for square.

  tools/checkisle.py            both
  tools/checkisle.py --model    just the first, which needs no emulator
"""

import argparse
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import isleref as ref
import townref

PRG = "dist/isletest.prg"


def checksum(tiles):
    s = 0
    for t in tiles:
        s = (((s << 1) | (s >> 15)) + t) & 0xFFFF
    return s


def story(seed):
    """What a continent's first place is, in the model's terms: the machine's
    test (src/isletest.c) makes it a dungeon for an odd seed."""
    return ref.G_DUNGEON if seed & 1 else ref.G_TOWN


def count(tiles, code):
    return sum(1 for t in tiles if t == code)


def check_model(seeds):
    bad = 0
    lands = []
    homes = []
    biomes = [0] * ref.BIOMES
    for seed in seeds:
        for d in range(5):
            home = d == ref.HOME
            land = ref.build(seed, d, story(seed))
            tiles = land.tiles
            if not home:
                biomes[land.dom] += 1
            total, reached, stranded = ref.reachable(tiles)
            (homes if home else lands).append(total)
            problems = []
            if total != reached:
                problems.append("%d of %d walkable squares cannot be reached"
                                % (total - reached, total))
            if stranded:
                problems.append("%d places with no walkable square beside"
                                % stranded)
            # Ships come in along a straight line and must meet nothing but
            # sea until the landing.
            if home:
                lanes = [(ref.start(ref.HOME_H)[p ^ 2], land.docks[p], p ^ 2)
                         for p in range(4)]
            else:
                lanes = [(land.start, land.docks[0], d)]
            for (x, y), dock, heading in lanes:
                dx, dy = ((0, -1), (1, 0), (0, 1), (-1, 0))[heading]
                while (x, y) != dock:
                    if tiles[y * ref.W + x] != ref.T_WATER:
                        problems.append("land on the way in at %d,%d" % (x, y))
                        break
                    x, y = x + dx, y + dy
                if tiles[dock[1] * ref.W + dock[0]] != ref.T_PORT:
                    problems.append("the landing is not a port")
            if count(tiles, ref.T_PORT) != (4 if home else 1):
                problems.append("wrong number of ports")
            want = ([ref.G_TOWN, ref.G_CASTLE, ref.G_DUNGEON, ref.G_LAIR,
                     ref.G_TOWN] if home else [story(seed), ref.G_TOWN])
            for (x, y), kind in zip(land.places, want):
                if tiles[y * ref.W + x] != ref.FINAL[kind]:
                    problems.append("place at %d,%d is not a %02X"
                                    % (x, y, kind))
                if kind == ref.G_LAIR and not any(
                        tiles[(y + dy) * ref.W + x + dx] == ref.T_MOUNTAIN
                        for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0))):
                    problems.append("the lair has no mountain beside it")
            near = [min(max(abs(x - a), abs(y - b)) for a, b in land.places
                        if (a, b) != (x, y)) for x, y in land.places]
            if min(near) < 4:
                problems.append("two places only %d apart" % min(near))
            if total < (900 if home else 150):
                problems.append("only %d walkable squares" % total)
            for p in problems:
                print("  seed %3d, compass %d: %s" % (seed, d, p))
            bad += bool(problems)
    print("model: %d continents, walkable squares %d-%d (mean %d), "
          "biomes %s; home %d-%d (mean %d); %d problems"
          % (len(lands), min(lands), max(lands), sum(lands) // len(lands),
             biomes, min(homes), max(homes), sum(homes) // len(homes), bad))
    return bad


def town_checksum(tiles, ents):
    s = 0
    for t in tiles:
        s = (((s << 1) | (s >> 15)) + t) & 0xFFFF
    for e in ents:
        for b in e:
            s = (((s << 1) | (s >> 15)) + b) & 0xFFFF
    return s


BLOCKS = {townref.T_WALL, townref.T_TREE, townref.T_BOULDER, townref.T_POTION,
          townref.T_SWORD, townref.T_BED, townref.T_SHIELD, townref.T_BOW}


def check_towns(seeds):
    """Every door and every person in a made town can be walked to from the
    gate, whatever the land and the seed."""
    bad = 0
    count = 0
    W, H = townref.W, townref.H
    for seed in seeds:
        for land in range(5):
            for biome in range(ref.BIOMES):
                count += 1
                tiles, ents, start, gate = townref.build(seed, land, biome)
                people = {(x, y) for x, y, _, _ in ents}
                seen = {start}
                todo = [start]
                while todo:
                    x, y = todo.pop()
                    for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0)):
                        n = (x + dx, y + dy)
                        if (0 <= n[0] < W and 0 <= n[1] < H and n not in seen
                                and n not in people
                                and tiles[n[1] * W + n[0]] not in BLOCKS):
                            seen.add(n)
                            todo.append(n)
                problems = []
                doors = [(i % W, i // W) for i, t in enumerate(tiles)
                         if t == townref.T_DOOR]
                for d in doors:
                    if d not in seen:
                        problems.append("door %s cannot be reached" % (d,))
                for x, y, kind, _ in ents:
                    if tiles[y * W + x] != townref.T_FLOOR:
                        problems.append("person at %d,%d is not on floor" % (x, y))
                    if not any((x + dx, y + dy) in seen
                               for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0))):
                        problems.append("person at %d,%d cannot be reached" % (x, y))
                if len(doors) != 1 + len(ents) - (
                        1 if any(k == townref.ENT_SHOP and p == townref.SHOP_ITEM
                                 for _, _, k, p in ents) else 0) \
                        and len(doors) < 4:
                    problems.append("%d doors" % len(doors))
                kinds = [k for _, _, k, _ in ents]
                if kinds.count(townref.ENT_INN) != 1 or \
                        townref.ENT_SHOP not in kinds:
                    problems.append("no inn or no shop")
                for p in problems:
                    print("  town seed %3d land %d biome %d: %s"
                          % (seed, land, biome, p))
                bad += bool(problems)
    print("towns: %d made, %d problems" % (count, bad))
    return bad


def peek(args, symbol, width):
    out = subprocess.run(["tools/screendump.py", PRG, "--keys", "x",
                          "--stage", "2"] + args
                         + ["--peek", "%s:%d" % (symbol, width)],
                         capture_output=True, text=True, timeout=300).stderr
    m = re.search(r"%s\+0 @ \$[0-9A-F]+ = (?:\d+|-) \(\$([0-9A-F]+)\)" % symbol, out)
    if not m:
        sys.exit("checkisle: no answer from the emulator:\n" + out)
    return bytes.fromhex(m.group(1))[::-1]


def check_machine(first, count):
    bad = 0
    for start in range(first, first + count, 16):
        n = min(16, first + count - start)
        raw = peek(["--poke", "first_seed=%d" % start,
                    "--poke", "seed_count=%d" % n], "sums", 160)
        for s in range(n):
            for d in range(5):
                want = checksum(ref.build(start + s, d,
                                          story(start + s)).tiles)
                got = raw[(s * 5 + d) * 2] | raw[(s * 5 + d) * 2 + 1] << 8
                if got != want:
                    print("  seed %3d, compass %d: the C64 built checksum "
                          "$%04X, the model $%04X" % (start + s, d, got, want))
                    bad += 1
    print("machine: seeds %d-%d, four compass points and home each, %d differ"
          % (first, first + count - 1, bad))
    tbad = 0
    for start in range(first, first + count, 16):
        n = min(16, first + count - start)
        raw = peek(["--poke", "first_seed=%d" % start,
                    "--poke", "seed_count=%d" % n], "town_sums", 160)
        for s in range(n):
            for d in range(5):
                seed = start + s
                tiles, ents, _, _ = townref.build(seed, d, (seed + d) % 5)
                want = town_checksum(tiles, ents)
                got = raw[(s * 5 + d) * 2] | raw[(s * 5 + d) * 2 + 1] << 8
                if got != want:
                    print("  seed %3d, land %d: the C64 built town checksum "
                          "$%04X, the model $%04X" % (seed, d, got, want))
                    tbad += 1
    print("machine: the same seeds' towns, %d differ" % tbad)
    bad += tbad

    # One map whole, so that a checksum that agrees by luck cannot hide a
    # generator that does not.
    diffs = 0
    for seed, d, y0 in ((7, 2, 0), (8, ref.HOME, 0), (8, ref.HOME, 31),
                        (8, ref.HOME, 33)):
        local = peek(["--poke", "show_seed=%d" % seed,
                      "--poke", "show_dir=%d" % d,
                      "--poke", "home_y0=%d" % y0,
                      "--poke", "seed_count=0"], "local", ref.W * ref.H)
        tiles = ref.build(seed, d, story(seed)).tiles[y0 * ref.W:]
        diff = sum(1 for a, b in zip(local, tiles) if a != b)
        print("machine: seed %d compass %d rows %d-%d read back whole, "
              "%d squares differ" % (seed, d, y0, y0 + ref.H - 1, diff))
        diffs += diff != 0
    return bad + diffs


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--model", action="store_true",
                    help="only check the model, with no emulator")
    ap.add_argument("--seeds", type=int, default=255,
                    help="how many game seeds the model is checked over")
    ap.add_argument("--machine-seeds", type=int, default=32,
                    help="how many the emulator is asked to build")
    args = ap.parse_args()

    bad = check_model(range(1, args.seeds + 1))
    bad += check_towns(range(1, args.seeds + 1))
    if not args.model:
        bad += check_machine(1, args.machine_seeds)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
