#!/usr/bin/env python3
"""How much of the game each mode needs, from ld65's map file.

    tools/modes.py [build/offline/q4k.map]

Groups every module the game links into the mode that needs it -- what has to
be in memory all the time (core), only while walking a map, only on a
one-shot screen the map opens (a shop, the inn, a voyage, building a land),
only in a fight, or only at start-up -- and prints each group's bytes by
where they live. It is a starting point for deciding what to load from disk
when, and the way to measure what an experiment actually freed: see
ENGINES.md. In the offline build, with engines, ENGINE is what is loaded
from the disk (an engine's code and constants), and OVL the title screen,
which the PRG brings in the engines' slot; tools/mkengines.py says how full
each engine is. Modules it does not know are listed, so a new one cannot go
uncounted.
"""

import re
import sys
from collections import defaultdict

# Which mode needs each module. Some straddle two (battle.c also holds the
# random-number stirring, the potions and the arms' names that the map and
# the panel use): they are counted where most of their bytes go, and
# ENGINES.md says what would have to move.
MODES = {
    "core": """text panel sprite music music_data vault stick probe rle actor
               local font_data tiles_names view game isle arms engine boot""",
    "map": """q4k world fov town_data town_gen place_near loot loot_data scene
              draw fightcall""",
    "service": """shop inn help talk savegame sail isle_gen tgen dungeon gift""",
    "fight": """battle arena_data open9 overlay""",
    "startup": """title charset_define tiles_data""",
}
ORDER = ["core", "map", "service", "fight", "startup", "runtime"]

# Where a segment's bytes are: in the file and in memory, or memory only.
ENGSEGS = {e + k for e in ("MAP", "FIGHT", "TALK", "SHOP")
           for k in ("CODE", "DATA")}
LOADED = ["CODE", "RODATA", "DATA", "HICODE", "HIDATA", "CHDATA1", "CHDATA2",
          "CHDATA3", "TITLECODE", "TITLEDATA", "STARTUP", "LOWCODE", "INIT",
          "ONCE", "EXEHDR", "LOADADDR"] + sorted(ENGSEGS)
COLUMNS = [("MAIN", {"CODE", "RODATA", "DATA", "STARTUP", "LOWCODE", "INIT",
                     "ONCE", "EXEHDR", "LOADADDR"}),
           ("CHDATA", {"CHDATA1", "CHDATA2", "CHDATA3"}),
           ("HIGH", {"HICODE", "HIDATA"}),
           ("OVL", {"TITLECODE", "TITLEDATA"}),
           ("ENGINE", ENGSEGS),
           ("BSS", {"BSS", "BSS2", "ZEROPAGE"})]


def parse(path):
    text = open(path).read()
    mods = text.split("Modules list:")[1].split("Segment list:")[0]
    sizes = defaultdict(lambda: defaultdict(int))
    current = None
    for line in mods.splitlines():
        if line and not line.startswith(" "):
            name = line.strip().rstrip(":")
            # "q4k.lib(battle.o)" or "build/offline/q4k.o" or "c64.lib(...)"
            m = re.match(r"(.*?)\((.*)\.o\)$", name)
            if m:
                current = m.group(2) if "q4k.lib" in m.group(1) \
                    else "runtime:" + m.group(2)
            else:
                current = re.sub(r".*/", "", name).replace(".o", "")
            continue
        m = re.match(r"\s+(\w+)\s+Offs=\w+\s+Size=([0-9A-F]+)", line)
        if m and current:
            sizes[current][m.group(1)] += int(m.group(2), 16)
    return sizes


def mode_of(module):
    if module.startswith("runtime:"):
        return "runtime"
    for mode, names in MODES.items():
        if module in names.split():
            return mode
    return None


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "build/offline/q4k.map"
    sizes = parse(path)
    groups = defaultdict(lambda: defaultdict(int))
    per_mod = defaultdict(dict)
    unknown = []
    for module, segs in sizes.items():
        mode = mode_of(module)
        if mode is None:
            unknown.append(module)
            continue
        for seg, n in segs.items():
            groups[mode][seg] += n
        if mode != "runtime":
            per_mod[mode][module] = segs

    head = "".join(f"{c:>8}" for c, _ in COLUMNS)
    print(f"{'':22}{head}{'loaded':>9}")
    for mode in ORDER:
        segs = groups[mode]
        cols = "".join(f"{sum(segs[s] for s in which):8}"
                       for _, which in COLUMNS)
        loaded = sum(n for s, n in segs.items() if s in LOADED)
        print(f"{mode:22}{cols}{loaded:9}")
        for module, msegs in sorted(per_mod[mode].items(),
                                    key=lambda kv: -sum(kv[1].values())):
            cols = "".join(f"{sum(msegs.get(s, 0) for s in which):8}"
                           for _, which in COLUMNS)
            loaded = sum(n for s, n in msegs.items() if s in LOADED)
            print(f"  {module:20}{cols}{loaded:9}")
    if unknown:
        print("not classified:", " ".join(sorted(unknown)))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
