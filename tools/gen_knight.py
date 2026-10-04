"""Write assets/knight.glb, assets/horse.glb, assets/knight_mounted.glb.

Low-poly faceted units built from primitives, rigid skinning (one bone per
part, weight 1.0), one mesh, one primitive, one material (assets/knight_palette.png,
colour by UV cell). Blender units are 2x game size (battle baseScale 0.5):
knight 1.8 tall, horse and mounted knight inside a 1.8 x 1.8 footprint
(one 1 m cell in game). Faces -Y in Blender (+Z in glTF), feet at z = 0.
Clips Idle/Walk/Attack/Hurt/Death use the test_rig durations so the
ClipEvents times (Attack hit 0.35 s, Walk feet 0 / 0.5 s) line up.

Run: blender -b --factory-startup -P tools/gen_knight.py
Clears the startup scene, builds, exports, saves art/knights.blend.
Headless only: glTF export from a non-active scene crashes Blender 5.1.
"""
import math
from pathlib import Path

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

ROOT = Path(__file__).resolve().parent.parent
FPS = 30
SCENE = "Knights"

COLORS = [
    ("steel", (0.66, 0.69, 0.74)),
    ("dark", (0.30, 0.32, 0.37)),
    ("blue", (0.16, 0.27, 0.62)),
    ("gold", (0.88, 0.68, 0.22)),
    ("leather", (0.42, 0.26, 0.13)),
    ("white", (0.92, 0.92, 0.88)),
    ("coat", (0.50, 0.33, 0.19)),
    ("mane", (0.13, 0.10, 0.08)),
    ("hoof", (0.18, 0.16, 0.15)),
    ("black", (0.04, 0.04, 0.05)),
    ("wood", (0.55, 0.40, 0.22)),
    ("red", (0.62, 0.14, 0.12)),
]
CELL = 16
PAL = 64
UV = {}
for _i, (_n, _c) in enumerate(COLORS):
    UV[_n] = (((_i % 4) * CELL + CELL / 2) / PAL, ((_i // 4) * CELL + CELL / 2) / PAL)


def M(loc=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
    q = Euler([math.radians(a) for a in rot], "XYZ").to_quaternion()
    return Matrix.LocRotScale(Vector(loc), q, Vector(scale))


def along(a, b):
    a, b = Vector(a), Vector(b)
    d = b - a
    q = Vector((0, 0, 1)).rotation_difference(d.normalized())
    return Matrix.LocRotScale((a + b) / 2, q, Vector((1, 1, 1))), d.length


def box(sx, sy, sz, top=(1.0, 1.0)):
    v = []
    for z, (fx, fy) in ((-sz / 2, (1, 1)), (sz / 2, top)):
        for x, y in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            v.append((x * sx / 2 * fx, y * sy / 2 * fy, z))
    f = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    return v, f


def prism(r, h, segs=8, top=1.0, phase=None):
    ph = math.pi / segs if phase is None else phase
    ang = [ph + 2 * math.pi * i / segs for i in range(segs)]
    v = [(r * math.cos(a), r * math.sin(a), -h / 2) for a in ang]
    f = [tuple(range(segs - 1, -1, -1))]
    if top == 0:
        v.append((0, 0, h / 2))
        f += [(i, (i + 1) % segs, segs) for i in range(segs)]
    else:
        v += [(r * top * math.cos(a), r * top * math.sin(a), h / 2) for a in ang]
        f.append(tuple(range(segs, 2 * segs)))
        f += [(i, (i + 1) % segs, segs + (i + 1) % segs, segs + i) for i in range(segs)]
    return v, f


def ico():
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=1, radius=1.0)
    v = [tuple(x.co) for x in bm.verts]
    f = [tuple(x.index for x in fc.verts) for fc in bm.faces]
    bm.free()
    return v, f


def sphere(seg=8, rings=5):
    v = [(0, 0, -1)]
    for r in range(1, rings):
        phi = math.pi * r / rings - math.pi / 2
        for i in range(seg):
            a = 2 * math.pi * i / seg
            v.append((math.cos(phi) * math.cos(a), math.cos(phi) * math.sin(a), math.sin(phi)))
    v.append((0, 0, 1))
    top = len(v) - 1
    f = [(0, 1 + (i + 1) % seg, 1 + i) for i in range(seg)]
    for r in range(rings - 2):
        b = 1 + r * seg
        for i in range(seg):
            j = (i + 1) % seg
            f.append((b + i, b + j, b + seg + j, b + seg + i))
    b = 1 + (rings - 2) * seg
    f += [(b + i, b + (i + 1) % seg, top) for i in range(seg)]
    return v, f


def limb(sx, sy, sz, top=1.0, segs=8, phase=None):
    v, f = prism(0.5, sz, segs, top, phase)
    return [(x * sx, y * sy, z) for x, y, z in v], f


def slab(pts, t):
    n = len(pts)
    v = [(x, -t / 2, z) for x, z in pts] + [(x, t / 2, z) for x, z in pts]
    f = [tuple(range(n)), tuple(range(2 * n - 1, n - 1, -1))]
    f += [(i, (i + 1) % n, n + (i + 1) % n, n + i) for i in range(n)]
    return v, f


class Model:
    def __init__(self):
        self.bones = []
        self.parts = []

    def bone(self, name, head, tail, parent=None):
        self.bones.append((name, Vector(head), Vector(tail), parent))

    def add(self, geo, bone, color, m=None, final=False):
        v, f = geo
        m = m or Matrix()
        self.parts.append(([m @ Vector(p) for p in v], f, bone, color, final))

    def along(self, a, b, w, d, bone, color, top=(1.0, 1.0), segs=0):
        m, length = along(a, b)
        geo = limb(w, d, length, top[0], segs) if segs else box(w, d, length, top)
        self.add(geo, bone, color, m)

    def merge(self, other):
        self.bones += other.bones
        self.parts += other.parts


def fk(model, pose, offset=Matrix()):
    rest = {n: (h, t, p) for n, h, t, p in model.bones}
    world = {}

    def W(n):
        if n not in world:
            h, _, p = rest[n]
            r = Euler([math.radians(a) for a in pose.get(n, (0, 0, 0))], "XYZ").to_matrix().to_4x4()
            parent = W(p) if p else offset
            world[n] = parent @ Matrix.Translation(h) @ r @ Matrix.Translation(-h)
        return world[n]

    bones = []
    for n, h, t, p in model.bones:
        parent = W(p) if p else offset
        bones.append((n, parent @ h, W(n) @ t, p))
    parts = []
    for v, f, b, c, final in model.parts:
        parts.append(([p.copy() if final else W(b) @ p for p in v], f, b, c, True))
    out = Model()
    out.bones, out.parts = bones, parts
    return out


def knight_model():
    k = Model()
    k.bone("Hips", (0, 0, 0.92), (0, 0, 1.05))
    k.bone("Spine", (0, 0, 1.05), (0, 0, 1.42), "Hips")
    k.bone("Head", (0, 0, 1.42), (0, 0, 1.75), "Spine")
    for s, side in ((1, "L"), (-1, "R")):
        k.bone(f"UpperArm.{side}", (0.24 * s, 0, 1.38), (0.27 * s, 0, 1.12), "Spine")
        k.bone(f"LowerArm.{side}", (0.27 * s, 0, 1.12), (0.28 * s, -0.02, 0.88), f"UpperArm.{side}")
        k.bone(f"Hand.{side}", (0.28 * s, -0.02, 0.88), (0.28 * s, -0.04, 0.78), f"LowerArm.{side}")
        k.bone(f"UpperLeg.{side}", (0.11 * s, 0, 0.9), (0.11 * s, 0, 0.5), "Hips")
        k.bone(f"LowerLeg.{side}", (0.11 * s, 0, 0.5), (0.11 * s, 0.01, 0.1), f"UpperLeg.{side}")
        k.bone(f"Foot.{side}", (0.11 * s, 0.01, 0.1), (0.11 * s, -0.14, 0.03), f"LowerLeg.{side}")

    k.add(limb(0.38, 0.26, 0.18, 0.95, 10), "Hips", "steel", M((0, 0, 0.9)))
    k.add(limb(0.40, 0.275, 0.05, 1.0, 10), "Hips", "leather", M((0, 0, 0.99)))
    k.add(box(0.06, 0.02, 0.05), "Hips", "gold", M((0, -0.138, 0.99)))
    k.add(box(0.26, 0.03, 0.3, (0.8, 1)), "Hips", "blue", M((0, -0.125, 0.8)))
    k.add(box(0.26, 0.03, 0.3, (0.8, 1)), "Hips", "blue", M((0, 0.125, 0.8)))

    k.add(limb(0.42, 0.27, 0.36, 1.1, 10), "Spine", "steel", M((0, 0, 1.22)))
    k.add(box(0.3, 0.02, 0.32, (1.1, 1)), "Spine", "blue", M((0, -0.137, 1.2)))
    k.add(box(0.3, 0.02, 0.32, (1.1, 1)), "Spine", "blue", M((0, 0.137, 1.2)))
    k.add(box(0.06, 0.012, 0.18), "Spine", "gold", M((0, -0.152, 1.21)))
    k.add(box(0.16, 0.012, 0.05), "Spine", "gold", M((0, -0.152, 1.25)))
    k.add(limb(0.22, 0.2, 0.07, 1.0, 12), "Spine", "dark", M((0, 0, 1.43)))

    k.add(limb(0.25, 0.25, 0.27, 1.0, 12), "Head", "steel", M((0, 0, 1.585)))
    k.add(limb(0.25, 0.25, 0.05, 0.5, 12), "Head", "steel", M((0, 0, 1.745)))
    k.add(limb(0.265, 0.265, 0.035, 1.0, 12), "Head", "dark", M((0, 0, 1.47)))
    k.add(box(0.17, 0.03, 0.028), "Head", "black", M((0, -0.118, 1.62)))
    k.add(box(0.028, 0.03, 0.12), "Head", "gold", M((0, -0.123, 1.55)))
    k.add(prism(0.045, 0.16, segs=8, top=0), "Head", "white", M((0, 0.02, 1.84), (-12, 0, 0)))

    for s, side in ((1, "L"), (-1, "R")):
        k.add(sphere(8, 5), f"UpperArm.{side}", "steel", M((0.25 * s, 0, 1.37), scale=(0.12, 0.13, 0.09)))
        k.add(limb(0.1, 0.11, 0.24), f"UpperArm.{side}", "steel", M((0.26 * s, 0, 1.24)))
        k.add(sphere(6, 4), f"LowerArm.{side}", "dark", M((0.27 * s, 0.01, 1.12), scale=(0.06, 0.06, 0.06)))
        k.add(limb(0.09, 0.1, 0.24, 1.12), f"LowerArm.{side}", "steel", M((0.275 * s, -0.01, 1.0)))
        k.add(box(0.09, 0.1, 0.11), f"Hand.{side}", "dark", M((0.28 * s, -0.03, 0.82)))
        k.add(limb(0.15, 0.17, 0.42, 1.1), f"UpperLeg.{side}", "steel", M((0.11 * s, 0, 0.71)))
        k.add(sphere(6, 4), f"LowerLeg.{side}", "dark", M((0.11 * s, -0.06, 0.5), scale=(0.07, 0.06, 0.07)))
        k.add(limb(0.12, 0.14, 0.4, 1.12), f"LowerLeg.{side}", "steel", M((0.11 * s, 0.005, 0.3)))
        k.add(box(0.12, 0.18, 0.09), f"Foot.{side}", "dark", M((0.11 * s, -0.02, 0.045)))
        k.add(limb(0.11, 0.09, 0.1, 0.5, 6), f"Foot.{side}", "dark", M((0.11 * s, -0.15, 0.045), (90, 0, 0)))

    sword = Matrix.Translation((-0.28, -0.03, 0.82)) @ M(rot=(20, 0, 0))
    k.add(limb(0.035, 0.035, 0.16, 1.0, 6), "Hand.R", "leather", sword @ M(rot=(90, 0, 0)))
    k.add(sphere(6, 4), "Hand.R", "gold", sword @ M((0, 0.1, 0), scale=(0.03, 0.03, 0.03)))
    k.add(box(0.03, 0.03, 0.2), "Hand.R", "gold", sword @ M((0, -0.09, 0)))
    k.add(limb(0.075, 0.02, 0.62, 1.0, 4, 0.0), "Hand.R", "steel", sword @ M((0, -0.41, 0), (90, 0, 0)))
    k.add(limb(0.075, 0.02, 0.1, 0.0, 4, 0.0), "Hand.R", "steel", sword @ M((0, -0.77, 0), (90, 0, 0)))

    kite = [(-0.19, 0.2), (-0.1, 0.225), (0.0, 0.232), (0.1, 0.225), (0.19, 0.2), (0.19, 0.05),
            (0.15, -0.12), (0.08, -0.26), (0.0, -0.36), (-0.08, -0.26), (-0.15, -0.12), (-0.19, 0.05)]
    shield = Matrix.Translation((0.33, -0.17, 1.07)) @ M(rot=(0, 0, 40))
    k.add(slab(kite, 0.03), "LowerArm.L", "blue", shield, final=True)
    k.add(box(0.04, 0.012, 0.5), "LowerArm.L", "gold", shield @ M((0, -0.018, -0.04)), final=True)
    k.add(box(0.34, 0.012, 0.04), "LowerArm.L", "gold", shield @ M((0, -0.018, 0.1)), final=True)
    k.add(sphere(6, 4), "LowerArm.L", "gold", shield @ M((0, -0.025, 0.05), scale=(0.05, 0.025, 0.05)), final=True)
    return k


KNIGHT_REST = {
    "UpperArm.L": (0, -12, 0),
    "LowerArm.L": (-60, 0, 0),
    "UpperArm.R": (0, 8, 0),
    "LowerArm.R": (-40, 0, 0),
}
RIDER_REST = dict(KNIGHT_REST, **{
    "UpperLeg.L": (-42, -26, 0),
    "LowerLeg.L": (52, 18, 0),
    "Foot.L": (-6, 0, 0),
    "UpperLeg.R": (-42, 26, 0),
    "LowerLeg.R": (52, -18, 0),
    "Foot.R": (-6, 0, 0),
})


def horse_model():
    h = Model()
    h.bone("Body", (0, 0.35, 1.0), (0, -0.3, 1.0))
    h.bone("Neck", (0, -0.35, 1.1), (0, -0.55, 1.45), "Body")
    h.bone("HorseHead", (0, -0.55, 1.45), (0, -0.82, 1.25), "Neck")
    h.bone("Tail", (0, 0.46, 1.12), (0, 0.6, 0.72), "Body")
    legs = {"FL": (0.13, -0.27, 0.88, 0.0), "FR": (-0.13, -0.27, 0.88, 0.0),
            "BL": (0.13, 0.3, 0.9, 0.04), "BR": (-0.13, 0.3, 0.9, 0.04)}
    for n, (x, y, z, back) in legs.items():
        h.bone(f"{n}_Up", (x, y, z), (x, y + back, 0.47), "Body")
        h.bone(f"{n}_Low", (x, y + back, 0.47), (x, y, 0.09), f"{n}_Up")
        h.bone(f"{n}_Hoof", (x, y, 0.09), (x, y - 0.03, 0.0), f"{n}_Low")
        h.along((x, y, z + 0.08), (x, y + back, 0.45), 0.12, 0.15, f"{n}_Up", "coat", (1.25, 1.2), segs=8)
        h.along((x, y + back, 0.49), (x, y, 0.09), 0.075, 0.085, f"{n}_Low", "coat", segs=6)
        h.add(limb(0.1, 0.11, 0.09, 0.8), f"{n}_Hoof", "hoof", M((x, y - 0.01, 0.045)))

    h.add(sphere(10, 6), "Body", "coat", M((0, 0.0, 1.0), scale=(0.22, 0.44, 0.26)))
    h.add(sphere(8, 5), "Body", "coat", M((0, -0.34, 1.03), scale=(0.2, 0.18, 0.24)))
    h.add(sphere(8, 5), "Body", "coat", M((0, 0.34, 1.05), scale=(0.21, 0.2, 0.24)))
    h.add(box(0.47, 0.44, 0.2), "Body", "blue", M((0, 0.02, 1.16)))
    h.add(box(0.48, 0.45, 0.03), "Body", "gold", M((0, 0.02, 1.065)))
    h.add(box(0.28, 0.32, 0.05), "Body", "leather", M((0, 0.02, 1.28)))
    h.add(limb(0.26, 0.06, 0.09, 0.9), "Body", "leather", M((0, 0.17, 1.32)))
    h.add(limb(0.16, 0.06, 0.09, 0.8), "Body", "leather", M((0, -0.13, 1.32)))
    for s in (1, -1):
        h.add(box(0.012, 0.025, 0.3), "Body", "leather", M((0.245 * s, 0.0, 1.1)))
        h.add(box(0.05, 0.08, 0.02), "Body", "dark", M((0.25 * s, 0.0, 0.94)))
        h.along((0.07 * s, -0.84, 1.3), (0.06 * s, -0.13, 1.33), 0.012, 0.012, "Body", "leather")

    h.along((0, -0.33, 1.08), (0, -0.56, 1.48), 0.15, 0.22, "Neck", "coat", (0.85, 0.75), segs=8)
    for i in range(3):
        t = i / 3
        a = (0, -0.30 - 0.2 * t, 1.22 + 0.34 * t)
        b = (0, -0.30 - 0.2 * (t + 0.36), 1.22 + 0.34 * (t + 0.36))
        h.along(a, b, 0.05, 0.08, "Neck", "mane", (0.7, 1.0))
    h.along((0, -0.53, 1.5), (0, -0.84, 1.27), 0.15, 0.18, "HorseHead", "coat", (0.78, 0.8), segs=8)
    h.add(sphere(8, 5), "HorseHead", "coat", M((0, -0.86, 1.25), (-38, 0, 0), (0.06, 0.065, 0.075)))
    h.along((0, -0.53, 1.5), (0, -0.84, 1.27), 0.155, 0.04, "HorseHead", "leather", (0.78, 1))
    h.add(limb(0.135, 0.15, 0.03, 1.0, 8), "HorseHead", "leather", M((0, -0.8, 1.28), (-38, 0, 0)))
    h.add(box(0.05, 0.04, 0.08), "HorseHead", "mane", M((0, -0.55, 1.57), (-30, 0, 0)))
    for s in (1, -1):
        h.add(prism(0.03, 0.1, segs=6, top=0), "HorseHead", "coat", M((0.045 * s, -0.55, 1.6), (-15, 0, 0)))
        h.add(box(0.02, 0.04, 0.03), "HorseHead", "black", M((0.072 * s, -0.66, 1.42)))
    h.along((0, 0.43, 1.17), (0, 0.5, 1.02), 0.09, 0.1, "Tail", "mane", (1.0, 1.0), segs=6)
    h.along((0, 0.48, 1.06), (0, 0.62, 0.7), 0.09, 0.1, "Tail", "mane", (1.7, 1.5), segs=6)
    return h


def knight_clips(mounted=False):
    def mirror(rot):
        return {n.replace(".L", ".X").replace(".R", ".L").replace(".X", ".R"): v for n, v in rot.items()}

    walk0 = {"UpperLeg.L": (-25, 0, 0), "LowerLeg.L": (5, 0, 0), "UpperLeg.R": (20, 0, 0),
             "LowerLeg.R": (15, 0, 0), "UpperArm.R": (-15, 0, 0), "UpperArm.L": (8, 0, 0),
             "Spine": (2, 0, 4)}
    walk1 = {"UpperLeg.L": (0, 0, 0), "LowerLeg.L": (8, 0, 0), "UpperLeg.R": (-5, 0, 0),
             "LowerLeg.R": (50, 0, 0), "Spine": (2, 0, 0)}
    clips = {
        "Idle": (2.0, [
            (0.0, {}, {}),
            (1.0, {"Spine": (-2, 0, 0), "Head": (3, 0, 0), "LowerArm.R": (-5, 0, 0)}, {"Hips": (0, 0, -0.012)}),
            (2.0, {}, {}),
        ]),
        "Walk": (1.0, [
            (0.0, walk0, {}),
            (0.25, walk1, {"Hips": (0, 0, 0.03)}),
            (0.5, mirror(walk0), {}),
            (0.75, mirror(walk1), {"Hips": (0, 0, 0.03)}),
            (1.0, walk0, {}),
        ]),
        "Attack": (0.6, [
            (0.0, {}, {}),
            (0.2, {"UpperArm.R": (-150, 0, 0), "LowerArm.R": (-30, 0, 0), "Spine": (-8, 0, -15),
                   "UpperLeg.L": (-15, 0, 0), "LowerLeg.L": (10, 0, 0)}, {}),
            (0.35, {"UpperArm.R": (-70, 0, 0), "LowerArm.R": (15, 0, 0), "Spine": (12, 0, 20),
                    "UpperLeg.L": (-25, 0, 0), "LowerLeg.L": (15, 0, 0), "UpperLeg.R": (15, 0, 0)},
             {"Hips": (0, -0.05, -0.04)}),
            (0.6, {}, {}),
        ]),
        "Hurt": (0.4, [
            (0.0, {}, {}),
            (0.12, {"Spine": (-15, 0, 0), "Head": (-15, 0, 0), "UpperArm.R": (15, 0, 0),
                    "UpperArm.L": (10, 0, 0)}, {"Hips": (0, 0.05, 0)}),
            (0.4, {}, {}),
        ]),
        "Death": (0.8, [
            (0.0, {}, {}),
            (0.25, {"Spine": (-20, 0, 0), "Head": (-15, 0, 0), "UpperLeg.L": (-20, 0, 0),
                    "UpperLeg.R": (-10, 0, 0), "LowerLeg.L": (30, 0, 0), "LowerLeg.R": (30, 0, 0)},
             {"Hips": (0, 0.05, -0.05)}),
            (0.5, {"Hips": (-50, 0, 0), "Spine": (-10, 0, 0), "UpperLeg.L": (-30, 0, 0),
                   "UpperLeg.R": (-20, 0, 0), "LowerLeg.L": (40, 0, 0), "LowerLeg.R": (40, 0, 0)},
             {"Hips": (0, 0.15, -0.35)}),
            (0.8, {"Hips": (-88, 0, 0), "Spine": (5, 0, 0), "Head": (10, 0, 0),
                   "UpperArm.L": (0, -50, 0), "UpperArm.R": (0, 50, 0),
                   "UpperLeg.L": (5, -8, 0), "UpperLeg.R": (5, 8, 0)},
             {"Hips": (0, 0.13, -0.77)}),
        ]),
    }
    if not mounted:
        return clips
    legs = ("Hips", "UpperLeg", "LowerLeg", "Foot")
    out = {}
    for name, (dur, keys) in clips.items():
        ks = []
        for t, rot, loc in keys:
            r = {n: v for n, v in rot.items() if not n.startswith(legs)}
            ks.append((t, r, {}))
        out[name] = (dur, ks)
    w = out["Walk"][1]
    out["Walk"] = (1.0, [(t, {"Spine": (4, 0, 0), "UpperArm.R": (-6 if i % 2 else 0, 0, 0)}, {})
                         for i, (t, _, _) in enumerate(w)])
    out["Death"] = (0.8, [
        (0.0, {}, {}),
        (0.25, {"Spine": (15, 0, 0), "Head": (10, 0, 0)}, {}),
        (0.5, {"Spine": (35, 0, 8), "Head": (15, 0, 0), "UpperArm.R": (20, 10, 0)}, {}),
        (0.8, {"Spine": (50, 0, 10), "Head": (25, 0, 0), "UpperArm.L": (30, 0, 0),
               "UpperArm.R": (35, 20, 0)}, {}),
    ])
    return out


def horse_clips(mounted=False):
    def legs(fl, fr, bl, br):
        r = {}
        for n, (up, low) in (("FL", fl), ("FR", fr), ("BL", bl), ("BR", br)):
            r[f"{n}_Up"] = (up, 0, 0)
            r[f"{n}_Low"] = (low, 0, 0)
        return r

    a = dict(legs((-25, 5), (20, 10), (20, -5), (-20, -5)), Neck=(4, 0, 0), Tail=(0, 8, 0))
    b = dict(legs((0, 0), (-5, 60), (-5, -40), (0, 0)), Neck=(-2, 0, 0), Tail=(0, 0, 0))
    c = dict(legs((20, 10), (-25, 5), (-20, -5), (20, -5)), Neck=(4, 0, 0), Tail=(0, -8, 0))
    d = dict(legs((-5, 60), (0, 0), (0, 0), (-5, -40)), Neck=(-2, 0, 0), Tail=(0, 0, 0))
    rear = dict(legs((-60, 70), (-45, 70), (25, 0), (25, 0)), Body=(-25, 0, 0), Neck=(-15, 0, 0),
                HorseHead=(-10, 0, 0))
    strike = dict(legs((-30, 0), (-25, 10), (-5, 0), (-5, 0)), Body=(5, 0, 0), Neck=(12, 0, 0))
    fold = legs((-10, 30), (-10, 30), (10, -20), (10, -20))
    down = legs((-30, 60), (-15, 40), (-20, -40), (-35, -50))
    kneel = (0.8, [
        (0.0, {}, {}),
        (0.25, dict(legs((-15, 60), (-10, 50), (10, -20), (10, -20)), Body=(10, 0, 0), Neck=(10, 0, 0)),
         {"Body": (0, 0, -0.15)}),
        (0.5, dict(legs((-30, 100), (-30, 100), (30, -60), (30, -60)), Body=(6, 0, 0), Neck=(25, 0, 0)),
         {"Body": (0, 0, -0.45)}),
        (0.8, dict(legs((-35, 120), (-35, 120), (45, -100), (45, -100)), Body=(2, 0, 4), Neck=(35, 0, 0),
                   HorseHead=(15, 0, 0)), {"Body": (0, 0, -0.6)}),
    ])
    clips = {
        "Idle": (2.0, [
            (0.0, {}, {}),
            (1.0, {"Neck": (6, 0, 0), "HorseHead": (5, 0, 0), "Tail": (0, 15, 0)}, {"Body": (0, 0, -0.01)}),
            (2.0, {}, {}),
        ]),
        "Walk": (1.0, [
            (0.0, a, {}),
            (0.25, b, {"Body": (0, 0, 0.03)}),
            (0.5, c, {}),
            (0.75, d, {"Body": (0, 0, 0.03)}),
            (1.0, a, {}),
        ]),
        "Attack": (0.6, [
            (0.0, {}, {}),
            (0.2, rear, {}),
            (0.35, strike, {"Body": (0, -0.06, -0.02)}),
            (0.6, {}, {}),
        ]),
        "Hurt": (0.4, [
            (0.0, {}, {}),
            (0.12, {"Neck": (-20, 0, 0), "HorseHead": (-20, 0, 0), "Body": (-4, 0, 0)}, {"Body": (0, 0.05, 0)}),
            (0.4, {}, {}),
        ]),
        "Death": (0.8, [
            (0.0, {}, {}),
            (0.25, dict(fold, Body=(0, 10, 0), Neck=(10, 0, 0)), {"Body": (0, 0, -0.08)}),
            (0.5, dict(down, Body=(0, 45, 0), Neck=(20, 0, 0)), {"Body": (0.12, 0, -0.35)}),
            (0.8, dict(down, Body=(0, 85, 0), Neck=(25, 0, 0), HorseHead=(10, 0, 0)), {"Body": (0.3, 0, -0.75)}),
        ]),
    }
    if mounted:
        clips["Death"] = kneel
    return clips


def merge_clips(a, b):
    out = {}
    for name, (dur, ka) in a.items():
        kb = b[name][1]
        assert [k[0] for k in ka] == [k[0] for k in kb], name
        out[name] = (dur, [(t, {**ra, **rb}, {**la, **lb}) for (t, ra, la), (_, rb, lb) in zip(ka, kb)])
    return out


def palette():
    img = bpy.data.images.get("knight_palette")
    if img:
        bpy.data.images.remove(img)
    img = bpy.data.images.new("knight_palette", PAL, PAL)
    px = [0.0] * (PAL * PAL * 4)
    for i, (_, c) in enumerate(COLORS):
        cx, cy = i % 4, i // 4
        for y in range(cy * CELL, cy * CELL + CELL):
            for x in range(cx * CELL, cx * CELL + CELL):
                o = (y * PAL + x) * 4
                px[o:o + 4] = [c[0], c[1], c[2], 1.0]
    img.pixels[:] = px
    path = ROOT / "assets" / "knight_palette.png"
    img.filepath_raw = str(path)
    img.file_format = "PNG"
    img.save()
    mat = bpy.data.materials.get("knight_palette")
    if mat:
        bpy.data.materials.remove(mat)
    mat = bpy.data.materials.new("knight_palette")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = img
    tex.interpolation = "Closest"
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.6
    return mat


def mode(scn, ob, m):
    vl = scn.view_layers[0]
    vl.objects.active = ob
    with bpy.context.temp_override(scene=scn, view_layer=vl, active_object=ob, object=ob):
        bpy.ops.object.mode_set(mode=m)


def build(scn, name, model, mat):
    arm = bpy.data.armatures.new(name)
    arm_ob = bpy.data.objects.new(name, arm)
    scn.collection.objects.link(arm_ob)
    mode(scn, arm_ob, "EDIT")
    for n, h, t, p in model.bones:
        eb = arm.edit_bones.new(n)
        eb.head, eb.tail, eb.roll = h, t, 0.0
        if p:
            eb.parent = arm.edit_bones[p]
    mode(scn, arm_ob, "OBJECT")

    verts, faces, cols, vbone = [], [], [], []
    for v, f, b, c, _ in model.parts:
        base = len(verts)
        verts += [tuple(p) for p in v]
        vbone += [b] * len(v)
        faces += [[base + i for i in fc] for fc in f]
        cols += [c] * len(f)
    me = bpy.data.meshes.new(name + "_mesh")
    me.from_pydata(verts, [], faces)
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    uv = me.uv_layers.new(name="UVMap")
    for poly in me.polygons:
        poly.use_smooth = False
        for li in poly.loop_indices:
            uv.data[li].uv = UV[cols[poly.index]]
    me.materials.append(mat)
    ob = bpy.data.objects.new(name + "_mesh", me)
    scn.collection.objects.link(ob)
    groups = {}
    for i, b in enumerate(vbone):
        groups.setdefault(b, []).append(i)
    for n, _, _, _ in model.bones:
        g = ob.vertex_groups.new(name=n)
        if n in groups:
            g.add(groups[n], 1.0, "REPLACE")
    ob.parent = arm_ob
    mod = ob.modifiers.new("Armature", "ARMATURE")
    mod.object = arm_ob
    return arm_ob, ob


def ground(arm_ob, mesh_ob, times, rest):
    scn = bpy.context.scene
    root = arm_ob.pose.bones[0]
    mi = rest[root.name].inverted()
    for t in times:
        f = t * FPS
        scn.frame_set(int(f), subframe=f - int(f))
        dg = bpy.context.evaluated_depsgraph_get()
        ev = mesh_ob.evaluated_get(dg)
        me = ev.to_mesh()
        low = min((mesh_ob.matrix_world @ v.co).z for v in me.vertices)
        ev.to_mesh_clear()
        root.location = root.location + mi @ Vector((0, 0, -low))
        root.keyframe_insert("location", frame=f)
    scn.frame_set(0)


def animate(arm_ob, mesh_ob, clips):
    ad = arm_ob.animation_data_create()
    rest = {pb.name: pb.bone.matrix_local.to_3x3() for pb in arm_ob.pose.bones}
    for pb in arm_ob.pose.bones:
        pb.rotation_mode = "QUATERNION"
    for clip, (_, keys) in clips.items():
        act = bpy.data.actions.new(f"{arm_ob.name}_{clip}")
        ad.action = act
        for t, rot, loc in keys:
            for bn in rest:
                pb = arm_ob.pose.bones[bn]
                m = rest[bn]
                mi = m.inverted()
                r = Euler([math.radians(a) for a in rot.get(bn, (0, 0, 0))], "XYZ").to_matrix()
                pb.rotation_quaternion = (mi @ r @ m).to_quaternion()
                pb.location = mi @ Vector(loc.get(bn, (0, 0, 0)))
                pb.keyframe_insert("rotation_quaternion", frame=t * FPS)
                pb.keyframe_insert("location", frame=t * FPS)
        ground(arm_ob, mesh_ob, [k[0] for k in keys], rest)
        track = ad.nla_tracks.new()
        track.name = clip
        track.strips.new(clip, 0, act)
        ad.action = None
    for pb in arm_ob.pose.bones:
        pb.rotation_quaternion = (1, 0, 0, 0)
        pb.location = (0, 0, 0)


def export(scn, arm_ob, mesh_ob, path):
    vl = scn.view_layers[0]
    for o in scn.objects:
        o.select_set(False, view_layer=vl)
    arm_ob.select_set(True, view_layer=vl)
    mesh_ob.select_set(True, view_layer=vl)
    vl.objects.active = arm_ob
    with bpy.context.temp_override(scene=scn, view_layer=vl, active_object=arm_ob):
        bpy.ops.export_scene.gltf(filepath=str(path), export_format="GLB", use_selection=True,
                                  export_animation_mode="NLA_TRACKS", export_yup=True,
                                  export_cameras=False, export_lights=False)


def cell_ref(scn, x):
    me = bpy.data.meshes.new("cell_ref")
    me.from_pydata([(-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)], [(0, 1), (1, 2), (2, 3), (3, 0)], [])
    ob = bpy.data.objects.new("cell_ref", me)
    ob.location.x = x
    scn.collection.objects.link(ob)


def main():
    bpy.context.preferences.filepaths.save_version = 0
    scn = bpy.context.scene
    scn.name = SCENE
    for o in list(scn.objects):
        bpy.data.objects.remove(o)
    scn.render.fps = FPS
    scn.unit_settings.system = "METRIC"
    mat = palette()

    knight = fk(knight_model(), KNIGHT_REST)
    horse = fk(horse_model(), {}, Matrix.Translation((0, 0.12, 0)))
    rider = fk(knight_model(), RIDER_REST, Matrix.Translation((0, 0.15, 0.46)))
    mounted = Model()
    mounted.merge(horse)
    rider.bones = [(n, h, t, p or "Body") for n, h, t, p in rider.bones]
    mounted.merge(rider)

    jobs = [
        ("knight", knight, knight_clips(), -2.5),
        ("horse", horse, horse_clips(), 0.0),
        ("knight_mounted", mounted, merge_clips(horse_clips(mounted=True), knight_clips(mounted=True)), 2.5),
    ]
    report = {}
    for name, model, clips, x in jobs:
        arm_ob, mesh_ob = build(scn, name, model, mat)
        animate(arm_ob, mesh_ob, clips)
        export(scn, arm_ob, mesh_ob, ROOT / "assets" / f"{name}.glb")
        for track in arm_ob.animation_data.nla_tracks:
            track.mute = True
        arm_ob.animation_data.action = bpy.data.actions[f"{name}_Idle"]
        arm_ob.location.x = x
        cell_ref(scn, x)
        report[name] = {"bones": len(model.bones), "verts": len(mesh_ob.data.vertices),
                        "tris": sum(len(p.vertices) - 2 for p in mesh_ob.data.polygons)}
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT / "art" / "knights.blend"))
    return report


result = main()
print(result)
