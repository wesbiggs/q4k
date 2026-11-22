#!/usr/bin/env python3
"""A bit-exact model of src/lib/town_gen.c, the town generator.

Every land has a town of its own: an inn, a shop, a third building (a potion
shop, or an empty house, small or large; home's is always the magic shop) and up to two more with a villager in.
It is laid out on a grid of six lots, three across and two down, with a street
between them and a plaza at the gate, each building bottom-aligned in its lot
with its door in the bottom wall -- so no two overlap and every door opens
on a street, by construction. Each but a house has its sign in that street,
diagonal from the door. (Interiors are 3 by 3, or 2 by 2 and 3 by 4 for
the small and large empty house; the keeper stands just inside the door.) tools/checkisle.py checks that, and that the
C64 builds the same town.

  tools/townref.py SEED LAND          draw one
"""

import sys

W, H = 21, 19

T_GRASS, T_SHORE, T_SWAMP = 0x60, 0x65, 0x66
T_WALL, T_FLOOR, T_DOOR, T_TREE, T_BOULDER = 0x6B, 0x6C, 0x6D, 0x72, 0x73
T_POTION, T_SWORD, T_BED, T_SHIELD, T_BOW = 0x74, 0x75, 0x76, 0x77, 0x79
T_BOOK = 0x7E
T_EXIT = 0x97
T_SIGNINN, T_SIGNWEAPON, T_SIGNARMOUR, T_SIGNBOW = 0x9A, 0x9B, 0x9C, 0x9D
T_SIGNITEM, T_SIGNMAGIC = 0x9E, 0x9F

ENT_NPC, ENT_SHOP, ENT_INN, ENT_VILLAGER = 0, 1, 2, 3
INN_PROC = 255          # an inn the generator made: not one of the drawn ones
SHOP_WEAPON, SHOP_ARMOUR, SHOP_ITEM, SHOP_BOW, SHOP_MAGIC = 0, 1, 2, 3, 4
ISLE_HOME = 4

BIOME_GROUND = [T_GRASS, T_GRASS, T_SWAMP, T_SHORE, T_GRASS]
# what a spare square of ground may be given, one time in four
BIOME_SCENERY = [0, T_TREE, 0, 0, T_BOULDER]
SHOP_STOCK = [T_SWORD, T_SHIELD, T_POTION, T_BOW]
SHOP_SIGN = [T_SIGNWEAPON, T_SIGNARMOUR, T_SIGNITEM, T_SIGNBOW]

NAMES = ["Harrowby", "Thistledown", "Marrow Ford", "Oldcombe", "Saltmere",
         "Greywick", "Fenhallow", "Stonebridge"]
INNS = ["The Wayfarer", "The Salted Pig", "The Rusty Plough", "The Last Lantern"]

# building kinds, in the order they are dealt
B_INN, B_SHOP, B_THIRD, B_HOUSE = 0, 1, 2, 3


class Rng:
    def __init__(self, lore_seed, land):
        self.s = (lore_seed & 0xFF) << 8 | ((land << 3) | 3)
        for _ in range(4):
            self.rnd()

    def rnd(self):
        s = self.s
        for _ in range(8):
            carry = s & 1
            s >>= 1
            if carry:
                s ^= 0xB400
        self.s = s
        return s & 0xFF


def name(lore_seed, land):
    return NAMES[(lore_seed + 3 * land) & 7]


def inn_name(lore_seed, land):
    return INNS[(lore_seed + land) & 3]


def build(lore_seed, land, biome):
    """Returns (tiles, ents, start, exit): the W*H tile codes, the people as
    (x, y, kind, param), and where you come in and go out."""
    r = Rng(lore_seed, land)
    m = [T_WALL] * (W * H)

    def rect(x0, y0, w, h, t):
        for y in range(y0, y0 + h):
            for x in range(x0, x0 + w):
                m[y * W + x] = t

    rect(1, 1, W - 2, H - 2, T_FLOOR)
    ground = BIOME_GROUND[biome]
    scenery = BIOME_SCENERY[biome]

    # the lots: ground, with scenery on it one square in four
    for lot in range(6):
        for y in range(2 + 7 * (lot // 3), 8 + 7 * (lot // 3)):
            for x in range(2 + 6 * (lot % 3), 7 + 6 * (lot % 3)):
                t = ground
                if scenery and r.rnd() & 3 == 0:
                    t = scenery
                m[y * W + x] = t

    # which lot each building gets: a shuffle
    order = list(range(6))
    for i in range(5, 0, -1):
        j = r.rnd() % (i + 1)
        order[i], order[j] = order[j], order[i]

    shop = r.rnd() % 3
    third = r.rnd() % 3
    if land == ISLE_HOME:
        third = 0                               # home's is the magic shop
    houses = r.rnd() % 3

    ents = []
    for n in range(3 + houses):
        lot = order[n]
        w, h = 5, 5
        if n == 2 and third == 1:
            w, h = 4, 4                         # a small empty house
        elif n == 2 and third == 2:
            w, h = 5, 6                         # and a large one
        x0 = 2 + 6 * (lot % 3)
        if w == 4:
            x0 += r.rnd() % 2
        yb = 7 + 7 * (lot // 3)
        y0 = yb - h + 1
        rect(x0, y0, w, h, T_WALL)
        rect(x0 + 1, y0 + 1, w - 2, h - 2, T_FLOOR)
        dx = x0 + 1 + r.rnd() % (w - 2)
        m[yb * W + dx] = T_DOOR
        # the way in is the square above the door and the keeper stands just
        # beyond it. Whatever the building sells is on its top row's corners.
        t = sign = 0
        if n == 0:
            ents.append((dx, yb - 2, ENT_INN, INN_PROC))
            t, sign = T_BED, T_SIGNINN
        elif n == 1:
            kind = SHOP_BOW if shop == 2 else shop
            ents.append((dx, yb - 2, ENT_SHOP, kind))
            t, sign = SHOP_STOCK[kind], SHOP_SIGN[kind]
        elif n == 2 and third == 0 and land == ISLE_HOME:
            ents.append((dx, yb - 2, ENT_SHOP, SHOP_MAGIC))
            t, sign = T_BOOK, T_SIGNMAGIC
        elif n == 2 and third == 0:
            ents.append((dx, yb - 2, ENT_SHOP, SHOP_ITEM))
            t, sign = T_POTION, T_SIGNITEM
        elif n >= 3:
            ents.append((dx, yb - 2, ENT_VILLAGER, 0))
        # The sign: in the street, diagonal from the door, on the side of
        # the building the door is nearer. It is always in the lot's
        # columns, so it never stands on a street that runs across.
        if sign:
            sx = dx - 1 if dx - 1 < x0 + w // 2 else dx + 1
            m[(yb + 1) * W + sx] = sign
        if t:
            m[(y0 + 1) * W + x0 + 1] = t
            m[(y0 + 1) * W + x0 + w - 2] = t

    gate = (W // 2, H - 1)
    m[gate[1] * W + gate[0]] = T_EXIT
    return m, ents, (gate[0], gate[1] - 1), gate


GLYPH = {T_GRASS: ".", T_SHORE: ",", T_SWAMP: "%", T_WALL: "#", T_FLOOR: "_",
         T_DOOR: "+", T_TREE: "t", T_BOULDER: "o", T_POTION: "!", T_SWORD: "/",
         T_BOOK: "j", T_EXIT: "x",
         T_BED: "B", T_SHIELD: ")", T_BOW: "}",
         T_SIGNINN: "i", T_SIGNWEAPON: "X", T_SIGNARMOUR: "a", T_SIGNBOW: "y",
         T_SIGNITEM: "p", T_SIGNMAGIC: "e"}


def show(tiles, ents, start):
    where = {(x, y): "KSVNIX"[k] for x, y, k, _ in ents for k in [k]}
    for y in range(H):
        print("".join("@" if (x, y) == start else
                      {ENT_INN: "i", ENT_SHOP: "s", ENT_VILLAGER: "v",
                       ENT_NPC: "n"}[next(e[2] for e in ents if (e[0], e[1]) == (x, y))]
                      if any((e[0], e[1]) == (x, y) for e in ents)
                      else GLYPH[tiles[y * W + x]] for x in range(W)))


if __name__ == "__main__":
    seed, land = int(sys.argv[1], 0), int(sys.argv[2])
    tiles, ents, start, gate = build(seed, land, int(sys.argv[3]) if len(sys.argv) > 3 else 0)
    show(tiles, ents, start)
    print(name(seed, land), "-", inn_name(seed, land), ents)
