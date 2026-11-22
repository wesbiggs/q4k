#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""assets/names.txt -> include/q4k/names_data.h.

The tables are macros, not arrays, so each module declares its own `static
const` table where it always did and its rodata lands where its module's
does. tools/mkgeo.py reads the same file for the host.
"""

import sys

SRC = "assets/names.txt"
OUT = "include/q4k/names_data.h"
COUNTS = {"land": 5, "town": 8, "inn": 4}   # town, inn: powers of two


def parse(path=SRC):
    names = {k: [] for k in COUNTS}
    names["stride"] = {}
    for n, line in enumerate(open(path), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        kind, _, name = line.partition(" ")
        if kind == "stride":
            which, _, num = name.partition(" ")
            if which not in ("town", "inn") or not num.strip().isdigit():
                sys.exit("%s:%d: expected stride town|inn <n>" % (path, n))
            names["stride"][which] = int(num)
            continue
        if kind not in names or not name.strip():
            sys.exit("%s:%d: expected land|town|inn <Name>" % (path, n))
        names[kind].append(name.strip())
    for k in ("town", "inn"):
        if k not in names["stride"]:
            sys.exit("%s: no stride for %s" % (path, k))
    for k, want in COUNTS.items():
        if len(names[k]) != want:
            sys.exit("%s: %d %s names, the game indexes %d"
                     % (path, len(names[k]), k, want))
    for k in ("town", "inn"):
        if len(names[k]) & (len(names[k]) - 1):
            sys.exit("%s: the game masks the index, so %s needs a power of "
                     "two names" % (path, k))
    return names


def main():
    names = parse()
    with open(OUT, "w") as f:
        f.write("/* GENERATED from %s by tools/mknames.py -- do not edit. */\n"
                "#ifndef Q4K_NAMES_DATA_H\n#define Q4K_NAMES_DATA_H\n\n" % SRC)
        for kind in ("land", "town", "inn"):
            body = ", ".join('"%s"' % n for n in names[kind])
            f.write("#define %s_NAMES { %s }\n" % (kind.upper(), body))
        for kind in ("town", "inn"):
            f.write("\n/* index = (lore_seed + STRIDE * land) & (COUNT - 1) */\n"
                    "#define %s_STRIDE %d\n#define %s_COUNT %d\n"
                    % (kind.upper(), names["stride"][kind],
                       kind.upper(), len(names[kind])))
        f.write("\n#endif\n")
    print("mknames: %d lands, %d towns, %d inns"
          % tuple(len(names[k]) for k in ("land", "town", "inn")))


if __name__ == "__main__":
    main()
