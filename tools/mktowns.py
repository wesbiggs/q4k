#!/usr/bin/env python3
"""Turn assets/towns.txt into C tables.

Each town is a map of screen codes, any size up to MAX_W by MAX_H (its rows
say how wide, their count how tall), plus a short entity list. The maps and
their sizes go in src/lib/town_maps.c, which only the open world's town
overlay links; the people, the names and the ids in src/lib/town_data.c,
which the screens of text link too.
"""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mktiles
import rle

SRC = "assets/towns.txt"
MAX_W, MAX_H = 40, 25   # the town overlay's region table (src/ow/town.c)
GEN_W, GEN_H = 21, 19   # the towns the game makes (src/lib/tgen.s)
SHOPS = {"weapon": "SHOP_WEAPON", "armour": "SHOP_ARMOUR", "item": "SHOP_ITEM",
         "bow": "SHOP_BOW", "magic": "SHOP_MAGIC"}


class Town:
    __slots__ = ("id", "name", "ents", "rows", "start", "exit",
                 "placed", "codes", "inns", "w", "h")


def parse(by_char):
    towns, cur, reading = [], None, False
    for lineno, raw in enumerate(open(SRC), 1):
        line = raw.rstrip("\n")
        if reading:
            if not line.strip():
                reading = False
                continue
            if cur.rows and len(line) != len(cur.rows[0]):
                sys.exit("%s:%d: row is %d wide, the first was %d"
                         % (SRC, lineno, len(line), len(cur.rows[0])))
            cur.rows.append(line)
            continue
        if not line.strip() or line.startswith("#"):
            continue

        word = line.split(None, 1)[0]
        if word == "town":
            _, tid, name = line.split(None, 2)
            cur = Town()
            cur.id, cur.name = tid, name
            cur.ents, cur.rows, cur.inns = {}, [], {}
            cur.start = cur.exit = None
            towns.append(cur)
        elif word == "ent":
            parts = line.split(None, 4)
            cur.ents[parts[1]] = (parts[2], parts[3])
            if parts[2] == "inn":
                if len(parts) < 5:
                    sys.exit("%s:%d: an inn needs a name" % (SRC, lineno))
                cur.inns[parts[3]] = parts[4]
        elif word == "map":
            reading = True
        else:
            sys.exit("%s:%d: unexpected %r" % (SRC, lineno, word))

    for t in towns:
        if not t.rows:
            sys.exit("town %s has no map" % t.id)
        t.w, t.h = len(t.rows[0]), len(t.rows)
        if t.w > MAX_W or t.h > MAX_H:
            sys.exit("town %s is %dx%d; a town may be %dx%d at most"
                     % (t.id, t.w, t.h, MAX_W, MAX_H))
    return towns


def resolve(towns, by_char):
    """Replace entity and marker characters with a tile, remembering where."""
    floor = by_char["_"].code
    door = by_char["+"].code

    for t in towns:
        grid = [list(r) for r in t.rows]
        t.placed = []
        for y in range(t.h):
            for x in range(t.w):
                c = grid[y][x]
                if c in by_char:
                    continue
                # Markers and entities sit on whatever surrounds them, so a
                # villager standing on grass is not suddenly on flagstones.
                under = neighbour_tile(t, grid, by_char, x, y, floor)
                if c == "$":
                    t.start = (x, y)
                    grid[y][x] = under
                elif c == "x":
                    t.exit = (x, y)
                    grid[y][x] = door
                elif c in t.ents:
                    kind, param = t.ents[c]
                    t.placed.append((x, y, kind, param))
                    grid[y][x] = under
                else:
                    sys.exit("town %s: map character %r is neither a tile nor "
                             "a declared entity" % (t.id, c))
        t.codes = [
            (by_char[c].code if isinstance(c, str) else c)
            for row in grid for c in row
        ]
        if t.start is None:
            sys.exit("town %s has no '$' arrival point" % t.id)
        if t.exit is None:
            sys.exit("town %s has no 'x' exit" % t.id)


def neighbour_tile(t, grid, by_char, x, y, default):
    for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0)):
        nx, ny = x + dx, y + dy
        if 0 <= nx < t.w and 0 <= ny < t.h:
            c = grid[ny][nx]
            # Nobody stands on a wall, a counter of potions or a bed.
            if (isinstance(c, str) and c in by_char
                    and by_char[c].flag != "TF_BLOCK"):
                return by_char[c].code
    return default


def main():
    tiles = mktiles.parse("assets/tiles.txt")
    by_char = {t.mapchar: t for t in tiles}
    towns = parse(by_char)
    resolve(towns, by_char)

    npc_ids, inns, ents, first = [], [], [], []
    for t in towns:
        first.append(len(ents))
        for x, y, kind, param in t.placed:
            if kind == "npc":
                if param not in npc_ids:
                    npc_ids.append(param)
                ents.append((x, y, "ENT_NPC", npc_ids.index(param)))
            elif kind == "shop":
                if param not in SHOPS:
                    sys.exit("town %s: unknown shop %r" % (t.id, param))
                ents.append((x, y, "ENT_SHOP", SHOPS[param]))
            elif kind == "inn":
                if any(i == param for i, _ in inns):
                    sys.exit("town %s: inn %r is declared twice" % (t.id, param))
                ents.append((x, y, "ENT_INN", len(inns)))
                inns.append((param, t.inns[param]))
            else:
                sys.exit("town %s: unknown entity kind %r" % (t.id, kind))
    first.append(len(ents))

    with open("include/q4k/town_data.h", "w") as f:
        f.write("/* GENERATED by tools/mktowns.py from %s -- do not edit. */\n"
                "#ifndef Q4K_TOWN_DATA_H\n#define Q4K_TOWN_DATA_H\n\n"
                "/* The biggest a town may be, and the size of those the\n"
                "   game makes (tgen.s). */\n"
                "#define TOWN_MAX_W  %d\n#define TOWN_MAX_H  %d\n"
                "#define TGEN_W      %d\n#define TGEN_H      %d\n"
                "#define NUM_TOWNS   %d\n#define NUM_TOWN_ENTS %d\n"
                "#define NUM_INNS    %d\n\n"
                "#define ENT_NPC     0\n#define ENT_SHOP    1\n"
                "#define ENT_INN     2\n#define ENT_VILLAGER 3\n\n"
                "#define SHOP_WEAPON 0\n#define SHOP_ARMOUR 1\n"
                "#define SHOP_ITEM   2\n#define SHOP_BOW    3\n"
                "#define SHOP_MAGIC  4\n\n"
                "typedef struct {\n"
                "    unsigned char x, y;\n"
                "    unsigned char kind;   /* ENT_NPC | ENT_SHOP | ENT_INN | ENT_VILLAGER */\n"
                "    unsigned char param;  /* npc, shop kind or inn index */\n"
                "} TownEnt;\n\n"
                % (SRC, MAX_W, MAX_H, GEN_W, GEN_H, len(towns), len(ents),
                   len(inns)))
        for i, t in enumerate(towns):
            f.write("#define TOWN_%-11s %d\n" % (t.id.upper(), i))
        for i, n in enumerate(npc_ids):
            f.write("#define NPC_%-11s %d\n" % (n.upper(), i))
        f.write("\n/* town_maps.c: run-length encoded (rle_unpack()), town_w by\n"
                "   town_h each, with where you arrive and the way out. */\n"
                "extern const unsigned char *const town_map[NUM_TOWNS];\n"
                "extern const unsigned char town_w[NUM_TOWNS];\n"
                "extern const unsigned char town_h[NUM_TOWNS];\n"
                "extern const unsigned char town_sx[NUM_TOWNS];\n"
                "extern const unsigned char town_sy[NUM_TOWNS];\n"
                "extern const unsigned char town_ex[NUM_TOWNS];\n"
                "extern const unsigned char town_ey[NUM_TOWNS];\n\n"
                "/* town_data.c */\n"
                "extern const char *const   town_names[NUM_TOWNS];\n"
                "extern const TownEnt       town_ents[NUM_TOWN_ENTS];\n"
                "extern const unsigned char town_ent_first[NUM_TOWNS + 1];\n"
                "extern const char *const   npc_ids[%d];\n"
                "extern const char *const   npc_names[%d];\n"
                "/* The id is what the dialogue daemon knows the inn by. */\n"
                "extern const char *const   inn_ids[NUM_INNS];\n"
                "extern const char *const   inn_names[NUM_INNS];\n\n"
                "#endif /* Q4K_TOWN_DATA_H */\n" % (len(npc_ids), len(npc_ids)))

    with open("src/lib/town_maps.c", "w") as f:
        f.write("/* GENERATED by tools/mktowns.py from %s -- do not edit. */\n\n"
                "#include <q4k/town_data.h>\n\n"
                "/* Only the open world's town overlay links this (the Makefile's\n"
                "   ow_town_maps.o). */\n\n" % SRC)
        packed = 0
        for i, t in enumerate(towns):
            f.write("/* %s, %dx%d */\n" % (t.name, t.w, t.h))
            packed += rle.write_c(f, "map_%d" % i, t.codes, 21)
            f.write("\n")
        f.write("const unsigned char *const town_map[NUM_TOWNS] = {\n    %s\n};\n\n"
                % ", ".join("map_%d" % i for i in range(len(towns))))

        def table(name, values, fmt="%d"):
            f.write("const unsigned char %s[NUM_TOWNS] = { %s };\n"
                    % (name, ", ".join(fmt % v for v in values)))

        table("town_w", [t.w for t in towns])
        table("town_h", [t.h for t in towns])
        table("town_sx", [t.start[0] for t in towns])
        table("town_sy", [t.start[1] for t in towns])
        table("town_ex", [t.exit[0] for t in towns])
        table("town_ey", [t.exit[1] for t in towns])

    with open("src/lib/town_data.c", "w") as f:
        f.write("/* GENERATED by tools/mktowns.py from %s -- do not edit. */\n\n"
                "#include <q4k/town_data.h>\n\n" % SRC)
        f.write("const char *const town_names[NUM_TOWNS] = {\n")
        for t in towns:
            f.write('    "%s",\n' % t.name)
        # The ids are what the dialogue host knows people and inns by, and
        # only a conversation needs them: the talk's engine (engine.h).
        # Spelled out as arrays so that the strings go there too.
        f.write("};\n\n#pragma rodata-name (push, \"TALKDATA\")\n")
        for n in npc_ids:
            f.write('static const char id_%s[] = "%s";\n' % (n, n))
        for i, _ in inns:
            f.write('static const char id_%s[] = "%s";\n' % (i, i))
        f.write("\nconst char *const npc_ids[%d] = {\n" % len(npc_ids))
        for n in npc_ids:
            f.write('    id_%s,\n' % n)
        f.write("};\n\nconst char *const inn_ids[NUM_INNS] = {\n")
        for i, _ in inns:
            f.write('    id_%s,\n' % i)
        f.write("};\n#pragma rodata-name (pop)\n")
        roster = json.load(open("assets/npcs.json"))
        f.write("\nconst char *const npc_names[%d] = {\n" % len(npc_ids))
        for n in npc_ids:
            if n not in roster:
                sys.exit("town entity names npc %r, which is not in "
                         "assets/npcs.json" % n)
            f.write('    "%s",\n' % roster[n]["name"])
        f.write("};\n\nconst char *const inn_names[NUM_INNS] = {\n")
        for _, n in inns:
            f.write('    "%s",\n' % n)
        f.write("};\n\nconst TownEnt town_ents[NUM_TOWN_ENTS] = {\n")
        for x, y, kind, param in ents:
            f.write("    { %2d, %2d, %-8s %s },\n" % (x, y, kind + ",", param))
        f.write("};\n\nconst unsigned char town_ent_first[NUM_TOWNS + 1] = {\n"
                "    %s\n};\n" % ", ".join(str(i) for i in first))

    print("mktowns: %d towns, %d entities, %d npcs, %d inns, maps packed to "
          "%d bytes" % (len(towns), len(ents), len(npc_ids), len(inns), packed))


if __name__ == "__main__":
    main()
