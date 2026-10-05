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

PRG = "dist/isletest.prg"


def checksum(tiles):
    s = 0
    for t in tiles:
        s = (((s << 1) | (s >> 15)) + t) & 0xFFFF
    return s


def check_model(seeds):
    bad = 0
    lands = []
    biomes = [0] * ref.BIOMES
    for seed in seeds:
        for d in range(4):
            tiles, dock, start, dom = ref.build(seed, d)
            biomes[dom] += 1
            total, reached = ref.reachable(tiles, dock)
            lands.append(total)
            problems = []
            if total != reached:
                problems.append("%d of %d walkable squares cannot be reached"
                                % (total - reached, total))
            # The ship comes in along a straight line and must meet nothing
            # but sea until the landing.
            x, y = start
            dx, dy = ((0, -1), (1, 0), (0, 1), (-1, 0))[d]
            while (x, y) != dock:
                if tiles[y * ref.W + x] != ref.T_WATER:
                    problems.append("land on the way in at %d,%d" % (x, y))
                    break
                x, y = x + dx, y + dy
            if tiles[dock[1] * ref.W + dock[0]] != ref.T_PORT:
                problems.append("the landing is not a port")
            if sum(1 for t in tiles if t == ref.T_PORT) != 1:
                problems.append("not exactly one port")
            if total < 150:
                problems.append("only %d walkable squares" % total)
            for p in problems:
                print("  seed %3d, compass %d: %s" % (seed, d, p))
            bad += bool(problems)
    print("model: %d continents, walkable squares %d-%d (mean %d), "
          "biomes %s, %d problems"
          % (len(lands), min(lands), max(lands), sum(lands) // len(lands),
             biomes, bad))
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
                    "--poke", "seed_count=%d" % n], "sums", 128)
        for s in range(n):
            for d in range(4):
                want = checksum(ref.build(start + s, d)[0])
                got = raw[(s * 4 + d) * 2] | raw[(s * 4 + d) * 2 + 1] << 8
                if got != want:
                    print("  seed %3d, compass %d: the C64 built checksum "
                          "$%04X, the model $%04X" % (start + s, d, got, want))
                    bad += 1
    print("machine: seeds %d-%d, four compass points each, %d differ"
          % (first, first + count - 1, bad))

    # One map whole, so that a checksum that agrees by luck cannot hide a
    # generator that does not.
    seed, d = 7, 2
    local = peek(["--poke", "show_seed=%d" % seed, "--poke", "show_dir=%d" % d,
                  "--poke", "seed_count=0"], "local", ref.W * ref.H)
    tiles = ref.build(seed, d)[0]
    diff = sum(1 for a, b in zip(local, tiles) if a != b)
    print("machine: seed %d compass %d read back whole, %d squares differ"
          % (seed, d, diff))
    return bad + (diff != 0)


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
    if not args.model:
        bad += check_machine(1, args.machine_seeds)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
