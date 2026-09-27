import math
import os
import struct
import sys

MAGIC   = 0x4C564C31
VERSION = 2
CELL    = 8
UNIT    = 38
PROP    = 32
HEADER  = 20

ELEV_AMP = 0.25

def elevation(x, y):
    return ELEV_AMP * math.sin((x + y) * 0.4)

def pack_cell(terrain, y):
    return struct.pack('<B3xf', terrain, y)

def pack_unit(faction, x, y, skinned, base, sel, stats):
    baseR, baseG, baseB = base
    selR, selG, selB = sel
    maxHp, atk, defn, mov, range_ = stats
    return struct.pack('<BBBBfff fff HHHHH',
                       faction, x, y, skinned,
                       baseR, baseG, baseB,
                       selR, selG, selB,
                       maxHp, atk, defn, mov, range_)

def pack_prop(type_, pos, scale, rgb):
    px, py, pz = pos
    r, g, b = rgb
    return struct.pack('<B3xfff f fff', type_, px, py, pz, scale, r, g, b)

def make_lvl(width, height, cell_size, cells, units, props, out_path):
    if len(cells) != width * height:
        raise ValueError(f"cells length {len(cells)} != {width}*{height}")
    out = bytearray()
    out += struct.pack('<IIHHHHf', MAGIC, VERSION,
                       width, height, len(units), len(props), cell_size)
    for t, y in cells:
        out += pack_cell(t, y)
    for u in units:
        out += pack_unit(*u)
    for p in props:
        out += pack_prop(*p)
    with open(out_path, 'wb') as f:
        f.write(bytes(out))
    return len(out)

if __name__ == '__main__':
    W = H = 20
    blocked = {(12, 12), (13, 12), (12, 13), (13, 13)}
    cells = [(0 if (x, y) in blocked else 1, elevation(x, y))
             for y in range(H) for x in range(W)]

    units = [
        (0, 5, 12, 0,
         (0.95, 0.55, 0.25), (1.0, 0.95, 0.30),
         (12, 6, 4, 4, 1)),
        (2, 12, 5, 0,
         (0.30, 0.80, 0.30), (0.80, 1.00, 0.30),
         (8, 4, 3, 3, 2)),
        (1, 18, 18, 1,
         (0.30, 0.70, 0.95), (1.0, 0.30, 0.30),
         (9, 5, 3, 3, 1)),
    ]

    default_out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               '..', 'assets', 'level_01.lvl')
    out_path = sys.argv[1] if len(sys.argv) > 1 else default_out
    sz = make_lvl(W, H, 1.0, cells, units, [], out_path)
    print(f"WROTE {sz}B -> {os.path.normpath(out_path)}")
