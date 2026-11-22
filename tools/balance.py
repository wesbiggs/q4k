#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fight the keepers and Maldrath many times over, and count what happens.

    tools/balance.py [--runs N] [--only NAME] [--party CLASSES] [--disk D64]

The open world's fight (src/fight/) has an autoplayer: the module built
again with FIGHT_BOT (FIGHTBOT on the arena's disk) plays the party, eight
ways, on a 20 by 9 field. This pokes the multicolour arena (src/owarena.c)
with a fight, a party, its level and its arms -- the arms each keeper was
meant for, by the land whose shops sell them -- has it fight `--runs` times,
and reads back how many it won and lost, and how many draughts and rounds
that took, on average.

The keepers were tuned for one hero: "a near thing", two or three draughts
each, and five or six for Maldrath. A party should find them about as near a
thing. Runs go four at a time, one VICE each.
"""

import argparse
import concurrent.futures
import re
import subprocess

# Menu entries: the wanderers first, then the keepers, then Maldrath.
NUM_WANDERERS = 11
FIGHTS = {
    "gaoler":  (NUM_WANDERERS + 0, 4),
    "drowned": (NUM_WANDERERS + 1, 6),
    "warden":  (NUM_WANDERERS + 2, 8),
    "maldrath": (NUM_WANDERERS + 3, 10),
}
# Wanderers, at a level where they are common, in the biggest group the game
# sends against a party of that size (battle_wander(), as
# src/lib/ow_fight.c has it): (menu entry, level,
# the level it first turns up at, which kit).
WANDER = {
    "goblins": (1, 1, 1, "start"),
    "wolves":  (2, 3, 2, "start"),
    "ogres":   (8, 5, 5, "gaoler"),
    "wraiths": (9, 7, 6, "drowned"),
    "drakes":  (10, 9, 7, "warden"),
}

# What each class carries for each fight: weapon, armour, bow, as indices
# into arms.c's lists -- the best its land sells that the class may use.
#   weapons: 2 short sword, 4 war axe, 6 runeblade, 7 Dragonbane
#   armour:  1 robes, 3 chain, 5 plate, 7 runed plate
#   bows:    2 shortbow, 4 composite, 5 arbalest
KIT = {
    "start":    {"W": (1, 0, 0), "A": (0, 0, 1), "Z": (8, 0, 0)},
    "gaoler":   {"W": (2, 3, 0), "A": (2, 3, 2), "Z": (2, 1, 0)},
    "drowned":  {"W": (4, 5, 0), "A": (2, 5, 4), "Z": (2, 1, 0)},
    "warden":   {"W": (6, 7, 0), "A": (2, 7, 5), "Z": (2, 1, 0)},
    "maldrath": {"W": (7, 7, 0), "A": (2, 7, 5), "Z": (2, 1, 0)},
}
CLS = {"W": 0, "A": 1, "Z": 2}


def run(fight, party, runs, disk="dist/owarena.d64"):
    group, kit = 1, fight
    if fight in WANDER:
        mon, level, start, kit = WANDER[fight]
        group = min(6, (level + len(party) - 1) // start)
    else:
        mon, level = FIGHTS[fight]
    args = ["dist/owarena.prg", "--disk", disk,
        "--poke", "level=%d" % level, "--poke", "auto_mon=%d" % mon,
        "--poke", "group=%d" % group,
        "--poke", "auto_runs=%d" % runs, "--poke", "auto_n=%d" % len(party),
        "--keys", "*", "--stage", "5", "--timeout", "3600"]
    for i, c in enumerate(party):
        w, a, b = KIT[kit][c]
        args += ["--poke", "auto_cls+%d=%d" % (i, CLS[c]),
                 "--poke", "auto_w+%d=%d" % (i, w),
                 "--poke", "auto_a+%d=%d" % (i, a),
                 "--poke", "auto_b+%d=%d" % (i, b)]
    for r in ("won", "lost", "stall", "draughts", "rounds"):
        args += ["--peek", "res_%s:2" % r]
    out = subprocess.run(["tools/screendump.py"] + args, capture_output=True,
                         text=True).stderr
    got = {m.group(1): int(m.group(2))
           for m in re.finditer(r"res_(\w+)\+0 @ \S+ = (\d+)", out)}
    return fight, party, got


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", type=int, default=40)
    ap.add_argument("--disk", default="dist/owarena.d64",
                    help="the arena's disk, to try another FIGHTBOT")
    ap.add_argument("--only", action="append",
                    choices=sorted(FIGHTS) + sorted(WANDER))
    ap.add_argument("--wander", action="store_true",
                    help="the wanderers instead of the keepers")
    ap.add_argument("--party", action="append",
                    help="classes, e.g. WAZ (W warrior, A archer, Z wizard); "
                         "repeatable. Default: W, WZ, WAZ, ZZZ, AAA")
    args = ap.parse_args()
    parties = args.party or ["W", "WZ", "WAZ", "ZZZ", "AAA"]
    fights = args.only or list(WANDER if args.wander else FIGHTS)

    jobs = [(f, p) for f in fights for p in parties]
    print("%-9s %-5s %5s %5s %5s %9s %7s"
          % ("fight", "party", "won", "lost", "stall", "draughts", "rounds"))
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        for f, p, got in pool.map(
                lambda j: run(j[0], j[1], args.runs, args.disk),
                jobs):
            if not got:
                print("%-9s %-5s did not finish" % (f, p))
                continue
            n = args.runs
            print("%-9s %-5s %4d%% %4d%% %4d%% %9.1f %7.1f"
                  % (f, p, 100 * got["won"] // n, 100 * got["lost"] // n,
                     100 * got["stall"] // n, got["draughts"] / n,
                     got["rounds"] / n))


if __name__ == "__main__":
    main()
