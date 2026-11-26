#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Quest for the King -- the loading icon's pictures (assets/icons.txt) as
OW-ICON: a program file that loads at $FF00, under the KERNAL, with the
disk, then the die's two frames, a 64-byte sprite block each, in that
order (src/lib/ow_disk.s and include/q4k/ow.h's OW_ICON_* say where).

    tools/mkicons.py OUT.prg
"""

import sys

ICON_ART = 0xFF00
ORDER = ["disk", "d20a", "d20b"]


def read(path):
    icons, name, rows = {}, None, []
    for lineno, line in enumerate(open(path), 1):
        line = line.rstrip("\n")
        if line.startswith("#") and name is None:
            continue
        if line.startswith("icon "):
            name, rows = line.split()[1], []
            icons[name] = rows
        elif name is not None and line:
            if len(line) != 24 or set(line) - set(".#"):
                sys.exit("%s:%d: want 24 of '.#', got %r" % (path, lineno, line))
            rows.append(line)
    for name in ORDER:
        if len(icons.get(name, [])) != 21:
            sys.exit("%s: icon %s wants 21 rows" % (path, name))
    return icons


def block(rows):
    out = []
    for r in rows:
        bits = int(r.replace(".", "0").replace("#", "1"), 2)
        out += [bits >> 16, (bits >> 8) & 255, bits & 255]
    return out + [0]


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    icons = read("assets/icons.txt")
    data = [ICON_ART & 255, ICON_ART >> 8]
    for name in ORDER:
        data += block(icons[name])
    assert ICON_ART + len(data) - 2 <= 0xFFFA, "the icon runs into the CPU's vectors"
    open(sys.argv[1], "wb").write(bytes(data))


if __name__ == "__main__":
    main()
