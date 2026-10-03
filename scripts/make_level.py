"""Write assets/level_01..03.lvl (.lvl v3): flat, hill, fort.

v3 = v2 + header win condition, unit catalogId (16 bytes) + unitId (u16),
prop records by cell. Factions: 0 player, 1 enemy, 2 ally, 3 neutral.
Win: 0 defeat_all, 1 survive_n_rounds (a = rounds), 2 reach_cell (a, b).
Props: 0 crate, 1 rock, 2 tree.
Usage: python3 scripts/make_level.py [out_dir]
"""
import math
import os
import struct
import sys

MAGIC   = 0x4C564C31
VERSION = 3

DEFEAT_ALL, SURVIVE, REACH_CELL = 0, 1, 2
CRATE, ROCK, TREE = 0, 1, 2
PLAYER, ENEMY, ALLY, NEUTRAL = 0, 1, 2, 3

UNIT_TYPES = {
    "knight": (PLAYER, (0.95, 0.55, 0.25), (1.0, 0.95, 0.30), (12, 6, 4, 4, 1)),
    "mage":   (ALLY,   (0.30, 0.80, 0.30), (0.80, 1.00, 0.30), (8, 4, 3, 3, 2)),
    "archer": (PLAYER, (0.30, 0.75, 0.45), (0.60, 1.00, 0.60), (9, 5, 2, 3, 4)),
    "goblin": (ENEMY,  (0.30, 0.70, 0.95), (1.0, 0.30, 0.30), (11, 7, 3, 4, 1)),
    "slinger": (ENEMY, (0.70, 0.60, 0.25), (1.0, 0.30, 0.30), (9, 6, 1, 3, 3)),
}

PROP_COLORS = {CRATE: (0.55, 0.38, 0.20), ROCK: (0.50, 0.50, 0.52), TREE: (0.25, 0.55, 0.25)}


def unit(catalog_id, x, y, unit_id):
    faction, base, sel, stats = UNIT_TYPES[catalog_id]
    return struct.pack('<BBBBfff fff HHHHH 16sH', faction, x, y, 0, *base, *sel, *stats,
                       catalog_id.encode(), unit_id)


def prop(type_, x, y, scale=1.0, yaw=0.0, blocks=True):
    return struct.pack('<BBBB f f fff', type_, x, y, 1 if blocks else 0, yaw, scale,
                       *PROP_COLORS[type_])


def make_lvl(width, height, cells, units, props, win, out_path, cell_size=1.0):
    assert len(cells) == width * height
    out = bytearray(struct.pack('<IIHHHHf BBBx', MAGIC, VERSION, width, height,
                                len(units), len(props), cell_size, *win))
    for t, y in cells:
        out += struct.pack('<B3xf', t, y)
    for i, (cid, x, y) in enumerate(units):
        out += unit(cid, x, y, i + 1)
    for p in props:
        out += prop(*p)
    with open(out_path, 'wb') as f:
        f.write(bytes(out))
    return len(out)


def level_flat():
    w = h = 20
    blocked = {(12, 12), (13, 12), (12, 13), (13, 13)}
    cells = [(0 if (x, y) in blocked else 1, 0.25 * math.sin((x + y) * 0.4))
             for y in range(h) for x in range(w)]
    units = [("knight", 5, 12), ("goblin", 16, 15), ("archer", 4, 10), ("goblin", 15, 17),
             ("mage", 6, 9), ("slinger", 18, 18)]
    props = [(CRATE, 7, 9, 0.8), (CRATE, 8, 9, 0.8, 0.3), (ROCK, 15, 4, 1.2),
             (TREE, 3, 3), (TREE, 16, 15), (TREE, 9, 17)]
    return w, h, cells, units, props, (DEFEAT_ALL, 0, 0)


def level_hill():
    w = h = 16
    cx, cz = 10.5, 10.5

    def y_at(x, z):
        d2 = (x + 0.5 - cx) ** 2 + (z + 0.5 - cz) ** 2
        return round(max(0.0, 1.0 - d2 / 36.0) * 1.5 / 0.25) * 0.25

    cells = [(1, y_at(x, z)) for z in range(h) for x in range(w)]
    units = [("knight", 2, 2), ("goblin", 10, 10), ("archer", 3, 2), ("goblin", 11, 9),
             ("mage", 2, 3), ("slinger", 9, 11)]
    props = [(TREE, 5, 7), (TREE, 7, 4), (TREE, 13, 5), (TREE, 4, 13), (ROCK, 12, 13, 0.9)]
    return w, h, cells, units, props, (SURVIVE, 8, 0)


def level_fort():
    w = h = 16
    cells = []
    for z in range(h):
        for x in range(w):
            inside = 5 <= x <= 10 and 5 <= z <= 10
            wall = inside and (x in (5, 10) or z in (5, 10))
            gate = z == 5 and x in (7, 8)
            if gate:
                cells.append((1, 0.5))
            elif wall:
                cells.append((0, 1.0))
            elif inside:
                cells.append((1, 1.0))
            else:
                cells.append((1, 0.0))
    units = [("knight", 7, 1), ("goblin", 7, 8), ("archer", 8, 1), ("slinger", 8, 9),
             ("mage", 6, 1), ("goblin", 3, 12)]
    props = [(CRATE, 6, 9), (CRATE, 9, 6, 0.8), (ROCK, 2, 5, 1.1), (ROCK, 13, 3),
             (TREE, 13, 13), (TREE, 1, 14)]
    return w, h, cells, units, props, (REACH_CELL, 8, 8)


if __name__ == '__main__':
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '..', 'assets')
    for i, make in enumerate((level_flat, level_hill, level_fort), start=1):
        w, h, cells, units, props, win = make()
        path = os.path.join(out_dir, f'level_{i:02d}.lvl')
        sz = make_lvl(w, h, cells, units, props, win, path)
        print(f"WROTE {sz}B {w}x{h} units={len(units)} props={len(props)} -> {os.path.normpath(path)}")
