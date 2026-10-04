import math

from mathutils import Matrix, Vector

from .geo import M, Model, dome, facing, joints, limb, prism, ring, slab, sphere, tube

HC = Vector((0.0, 0.0, 1.24))
HR = Vector((0.36, 0.33, 0.34))


def sd_humanoid(k):
    k.bone("Hips", (0, 0, 0.46), (0, 0, 0.58))
    k.bone("Spine", (0, 0, 0.58), (0, 0, 0.9), "Hips")
    k.bone("Head", (0, 0, 0.9), (0, 0, 1.6), "Spine")
    for s, side in ((1, "L"), (-1, "R")):
        k.bone(f"UpperArm.{side}", (0.19 * s, 0, 0.84), (0.25 * s, 0, 0.68), "Spine")
        k.bone(f"LowerArm.{side}", (0.25 * s, 0, 0.68), (0.28 * s, -0.02, 0.53), f"UpperArm.{side}")
        k.bone(f"Hand.{side}", (0.28 * s, -0.02, 0.53), (0.29 * s, -0.03, 0.45), f"LowerArm.{side}")
        k.bone(f"UpperLeg.{side}", (0.1 * s, 0, 0.46), (0.1 * s, 0, 0.27), "Hips")
        k.bone(f"LowerLeg.{side}", (0.1 * s, 0, 0.27), (0.1 * s, 0.01, 0.09), f"UpperLeg.{side}")
        k.bone(f"Foot.{side}", (0.1 * s, 0.01, 0.09), (0.1 * s, -0.12, 0.03), f"LowerLeg.{side}")


def at(J, bone, p):
    return J[bone][2] @ Vector(p)


def rigid(k, J, bone, geo, color, m=None, level=1, smooth=True):
    k.add(geo, bone, color, J[bone][2] @ (m or Matrix()), final=True, level=level, smooth=smooth)


def soft_tube(k, J, pts, radii, bones, color, segs=10, samples=2, level=1, caps=(True, True)):
    k.soft(tube([at(J, b, p) for b, p in pts], radii, segs, samples, caps), bones, color, level=level)


def surface(d, c=HC, r=HR, inset=0.0):
    d = Vector(d).normalized()
    p = c + Vector((d.x * r.x, d.y * r.y, d.z * r.z))
    n = Vector(((p - c).x / r.x ** 2, (p - c).y / r.y ** 2, (p - c).z / r.z ** 2)).normalized()
    return p - n * inset, n


def decal(k, J, d, size, color, c=HC, r=HR, geo=None, level=0, inset=0.006):
    p, n = surface(d, c, r, inset)
    m = Matrix.Translation(p) @ facing(n) @ Matrix.Diagonal((*size, 1.0))
    rigid(k, J, "Head", geo or sphere(12, 8), color, m, level=level)
    return p, n


def face(k, J, skin="skin", eye="black", brow="hair", c=HC, r=HR, eye_size=1.0, ears=True, blush=True,
         mouth=True):
    rigid(k, J, "Head", sphere(14, 9), skin, M(c, scale=r))
    for s in (1, -1):
        p, n = decal(k, J, (0.36 * s, -0.92, -0.08), (0.06 * eye_size, 0.022, 0.085 * eye_size), eye, c, r)
        hp = p + n * 0.012 + Vector((0.018 * s, 0, 0.032 * eye_size))
        rigid(k, J, "Head", sphere(6, 4), "white", M(hp, scale=(0.02, 0.012, 0.02)), level=0)
        if brow:
            bp, bn = surface((0.36 * s, -0.86, 0.26), c, r, 0.004)
            m = Matrix.Translation(bp) @ facing(bn) @ M(rot=(0, -8 * s, 0)) @ Matrix.Diagonal((0.055, 0.014, 0.014, 1))
            rigid(k, J, "Head", sphere(10, 6), brow, m, level=0)
        if blush:
            decal(k, J, (0.56 * s, -0.78, -0.3), (0.045, 0.01, 0.022), "blush", c, r)
        if ears:
            rigid(k, J, "Head", sphere(8, 6), skin,
                  M((c.x + r.x * 0.97 * s, c.y + 0.02, c.z - 0.03), scale=(0.04, 0.055, 0.075)))
    if mouth:
        decal(k, J, (0, -0.95, -0.4), (0.032, 0.012, 0.014), "mouth", c, r)
    decal(k, J, (0, -1.0, -0.2), (0.024, 0.02, 0.02), skin, c, r, inset=0.0)


def body(k, J, torso, sleeve, legs, boots, hands, torso_r=(0.16, 0.125)):
    tx, ty = torso_r
    soft_tube(k, J, [("Hips", (0, 0, 0.42)), ("Hips", (0, 0, 0.58)), ("Spine", (0, 0, 0.76)),
                     ("Spine", (0, 0, 0.93))],
              [(tx * 0.95, ty * 0.95), (tx, ty), (tx * 1.05, ty * 1.04), (tx * 0.62, ty * 0.66)],
              ["Hips", "Spine"], torso, segs=10)
    for s, side in ((1, "L"), (-1, "R")):
        soft_tube(k, J, [("Spine", (0.15 * s, 0, 0.85)), (f"UpperArm.{side}", (0.25 * s, 0, 0.68)),
                         (f"LowerArm.{side}", (0.28 * s, -0.02, 0.53))],
                  [0.062, 0.054, 0.048], ["Spine", f"UpperArm.{side}", f"LowerArm.{side}", f"Hand.{side}"],
                  sleeve, segs=8)
        rigid(k, J, f"Hand.{side}", sphere(12, 8), hands, M((0.29 * s, -0.03, 0.49), scale=(0.064, 0.06, 0.068)),
              level=0)
        soft_tube(k, J, [("Hips", (0.1 * s, 0, 0.5)), (f"LowerLeg.{side}", (0.1 * s, 0, 0.27)),
                         (f"Foot.{side}", (0.1 * s, 0.01, 0.09))],
                  [0.074, 0.063, 0.056], ["Hips", f"UpperLeg.{side}", f"LowerLeg.{side}", f"Foot.{side}"], legs,
                  segs=8)
        rigid(k, J, f"Foot.{side}", sphere(12, 8), boots, M((0.1 * s, -0.035, 0.055), scale=(0.078, 0.12, 0.066)),
              level=0)


def belt(k, J, color, buckle="gold", z=0.56, r=(0.17, 0.135)):
    rigid(k, J, "Hips", ring(1.0, 0.13, 24, 8), color, M((0, 0, z), scale=(r[0], r[1], 0.18)), level=0)
    if buckle:
        rigid(k, J, "Hips", sphere(8, 5), buckle, M((0, -r[1] - 0.01, z), scale=(0.035, 0.015, 0.03)))


def cape(k, J, color, top=0.9, bottom=0.32, radius=0.19, span=150.0, cols=6, rows=4, thick=0.02, flare=0.08):
    outer, inner = [], []
    for v in range(rows + 1):
        t = v / rows
        z = top + (bottom - top) * t
        rr = radius + flare * t
        for u in range(cols + 1):
            a = math.radians(-span / 2 + span * u / cols)
            d = Vector((math.sin(a), math.cos(a), 0))
            outer.append(Vector((0, 0.02, z)) + d * rr)
            inner.append(Vector((0, 0.02, z)) + d * (rr - thick))
    w = cols + 1
    verts = outer + inner
    off = len(outer)
    f = []
    for v in range(rows):
        for u in range(cols):
            a, b, c, d = v * w + u, v * w + u + 1, (v + 1) * w + u + 1, (v + 1) * w + u
            f.append((a, b, c, d))
            f.append((off + d, off + c, off + b, off + a))
    for u in range(cols):
        for v0 in (0, rows):
            a, b = v0 * w + u, v0 * w + u + 1
            f.append((a, off + a, off + b, b) if v0 == 0 else (b, off + b, off + a, a))
    for v in range(rows):
        for u0 in (0, cols):
            a, b = v * w + u0, (v + 1) * w + u0
            f.append((b, off + b, off + a, a) if u0 == 0 else (a, off + a, off + b, b))
    pts = []
    for p in verts:
        pts.append(J["Spine"][2] @ p if p.z > 0.58 else J["Hips"][2] @ p)
    k.soft(([tuple(p) for p in pts], f), ["Spine", "Hips"], color, level=1)


SD_KNIGHT_REST = {"UpperArm.L": (0, -14, 0), "LowerArm.L": (-55, 0, 0), "UpperArm.R": (0, 12, 0),
                  "LowerArm.R": (-40, 0, 0)}
SD_RIDER_REST = dict(SD_KNIGHT_REST, **{"UpperLeg.L": (-55, -42, 0), "LowerLeg.L": (50, 30, 0),
                                        "UpperLeg.R": (-55, 42, 0), "LowerLeg.R": (50, -30, 0)})
SD_ARCHER_REST = {"UpperArm.L": (0, -10, 0), "LowerArm.L": (-20, 0, 0), "UpperArm.R": (0, 10, 0),
                  "LowerArm.R": (-25, 0, 0)}
SD_MAGE_REST = {"UpperArm.L": (0, -12, 0), "LowerArm.L": (-35, 0, 0), "UpperArm.R": (0, 10, 0),
                "LowerArm.R": (-50, 0, 0)}
SD_GOBLIN_REST = {"Spine": (14, 0, 0), "Head": (-12, 0, 0), "UpperArm.L": (0, -18, 0), "UpperArm.R": (0, 18, 0),
                  "LowerArm.L": (-30, 0, 0), "LowerArm.R": (-35, 0, 0), "UpperLeg.L": (-12, -6, 0),
                  "UpperLeg.R": (-12, 6, 0), "LowerLeg.L": (22, 0, 0), "LowerLeg.R": (22, 0, 0),
                  "Foot.L": (-10, 0, 0), "Foot.R": (-10, 0, 0)}


def sword(k, J, hand=(-0.29, -0.03, 0.49), pre=20):
    base = Matrix.Translation(hand) @ M(rot=(pre, 0, 0))
    rigid(k, J, "Hand.R", tube([(0, 0.09, 0), (0, -0.07, 0)], [0.022, 0.022], 8, 1), "leather", base)
    rigid(k, J, "Hand.R", sphere(8, 6), "gold", base @ M((0, 0.11, 0), scale=(0.032, 0.032, 0.032)))
    rigid(k, J, "Hand.R", tube([(0, -0.08, -0.11), (0, -0.09, 0), (0, -0.08, 0.11)], [0.018, 0.024, 0.018], 8, 2),
          "gold", base)
    rigid(k, J, "Hand.R", limb(0.09, 0.022, 0.5, 1.0, 4, 0.0), "steel", base @ M((0, -0.34, 0), (90, 0, 0)), level=0)
    rigid(k, J, "Hand.R", limb(0.09, 0.022, 0.1, 0.0, 4, 0.0), "steel", base @ M((0, -0.64, 0), (90, 0, 0)), level=0)
    rigid(k, J, "Hand.R", limb(0.02, 0.026, 0.44, 1.0, 4, 0.0), "white", base @ M((0, -0.33, 0), (90, 0, 0)), level=0)


def kite_shield(k, J, center, yaw=40):
    pts = []
    for i in range(7):
        a = math.radians(180 - i * 30)
        pts.append((0.2 * math.cos(a), 0.12 + 0.05 * math.sin(a)))
    pts += [(0.19, -0.02), (0.13, -0.17), (0.0, -0.3), (-0.13, -0.17), (-0.19, -0.02)]
    frame = Matrix.Translation(center) @ M(rot=(0, 0, yaw))
    k.add(slab(pts, 0.04), "LowerArm.L", "blue", frame, final=True, level=0)
    rim = [(x * 1.0, -0.024, z) for x, z in pts] + [(pts[0][0], -0.024, pts[0][1])]
    k.add(tube(rim, [0.018] * len(rim), 6, 1, (False, False)), "LowerArm.L", "gold", frame, final=True, level=1,
          smooth=True)
    k.add(limb(0.045, 0.012, 0.36, 1.0, 4), "LowerArm.L", "gold", frame @ M((0, -0.026, -0.03)), final=True, level=0)
    k.add(limb(0.24, 0.012, 0.045, 1.0, 4), "LowerArm.L", "gold", frame @ M((0, -0.026, 0.06)), final=True, level=0)


def knight_sd(pose=None, offset=Matrix()):
    k = Model()
    sd_humanoid(k)
    pose = SD_KNIGHT_REST if pose is None else pose
    J = joints(k, pose, offset)
    body(k, J, "steel", "steel", "steel", "dark", "dark")
    face(k, J)
    rigid(k, J, "Head", dome(14, 4, 0.12, 0.06), "steel", M(HC + Vector((0, 0.01, 0)), (-14, 0, 0), HR * 1.08))
    rigid(k, J, "Head", ring(1.0, 0.07, 24, 8), "gold",
          M(HC + Vector((0, 0.004, 0.06)), (-14, 0, 0), (HR.x * 1.08, HR.y * 1.06, 0.4)), level=0)
    rigid(k, J, "Head", tube([(0, -0.05, 1.6), (0, 0.18, 1.66), (0, 0.38, 1.52), (0, 0.46, 1.3)],
                             [0.05, 0.075, 0.06, 0.02], 10, 2), "red")
    k.soft(limb(0.28, 0.06, 0.36, 0.85, 8), ["Hips", "Spine"], "blue",
           J["Spine"][2] @ M((0, -0.125, 0.64), (-4, 0, 0)), level=1)
    rigid(k, J, "Spine", limb(0.05, 0.02, 0.18, 1.0, 4), "gold", M((0, -0.158, 0.66), (-4, 0, 0)), level=0)
    rigid(k, J, "Spine", limb(0.14, 0.02, 0.045, 1.0, 4), "gold", M((0, -0.158, 0.7), (-4, 0, 0)), level=0)
    belt(k, J, "leather")
    cape(k, J, "red")
    for s, side in ((1, "L"), (-1, "R")):
        rigid(k, J, f"UpperArm.{side}", dome(10, 3, -0.3, 0.12), "steel",
              M((0.2 * s, 0, 0.84), (0, 25 * s, 0), (0.12, 0.12, 0.1)))
        rigid(k, J, f"LowerArm.{side}", sphere(10, 6), "steel", M((0.25 * s, 0.01, 0.68), scale=(0.055, 0.055, 0.05)),
              level=0)
        rigid(k, J, f"LowerLeg.{side}", sphere(10, 6), "steel", M((0.1 * s, -0.05, 0.27), scale=(0.06, 0.045, 0.055)),
              level=0)
    sword(k, J)
    lo = J["LowerArm.L"]
    mid = (lo[0] + lo[1]) / 2
    kite_shield(k, J, mid + Vector((0.08, -0.07, 0.0)))
    return k


def archer_sd():
    k = Model()
    sd_humanoid(k)
    J = joints(k, SD_ARCHER_REST)
    body(k, J, "green", "green", "bark", "leather", "skin")
    face(k, J)
    rigid(k, J, "Head", dome(14, 5, -0.1, 0.06), "green", M(HC + Vector((0, 0.03, 0.01)), (-22, 0, 0), HR * 1.1))
    rigid(k, J, "Head", tube([(0, 0.3, 1.42), (0, 0.42, 1.3), (0, 0.48, 1.16)], [0.11, 0.06, 0.012], 10, 2), "green")
    rigid(k, J, "Head", tube([(-0.27, -0.17, 1.38), (-0.14, -0.29, 1.46), (0.02, -0.32, 1.47), (0.16, -0.28, 1.45),
                             (0.28, -0.16, 1.37)], [0.03, 0.055, 0.06, 0.055, 0.03], 8, 2), "hair")
    for s in (1, -1):
        rigid(k, J, "Head", tube([(0.3 * s, -0.12, 1.38), (0.34 * s, -0.12, 1.22), (0.33 * s, -0.14, 1.12)],
                                 [0.04, 0.035, 0.01], 8, 2), "hair")
    soft_tube(k, J, [("Hips", (0, 0, 0.58)), ("Hips", (0, 0, 0.36))], [(0.17, 0.135), (0.205, 0.165)],
              ["Hips", "UpperLeg.L", "UpperLeg.R"], "green", segs=14, samples=2, caps=(False, False))
    rigid(k, J, "Spine", ring(1.0, 0.18, 24, 8), "green", M((0, 0, 0.9), scale=(0.13, 0.11, 0.15)), level=0)
    belt(k, J, "leather")
    k.soft(tube([at(J, "Spine", (0.15, -0.12, 0.86)), at(J, "Spine", (0.0, -0.15, 0.72)),
                 at(J, "Hips", (-0.15, -0.12, 0.56))], [0.016] * 3, 6, 2), ["Spine", "Hips"], "leather")
    quiver = Matrix.Translation((0.08, 0.17, 0.72)) @ M(rot=(0, -22, 0))
    rigid(k, J, "Spine", tube([(0, 0, -0.2), (0, 0, 0.2)], [0.055, 0.06], 10, 2), "leather", quiver)
    for dx in (-0.025, 0.0, 0.025):
        rigid(k, J, "Spine", sphere(6, 4), "white", quiver @ M((dx, 0, 0.25), scale=(0.012, 0.03, 0.05)), level=0)
    for s, side in ((1, "L"), (-1, "R")):
        rigid(k, J, f"LowerArm.{side}", tube([(0.265 * s, -0.01, 0.64), (0.28 * s, -0.02, 0.55)], [0.058, 0.055], 10, 1),
              "leather")
        rigid(k, J, f"LowerLeg.{side}", tube([(0.1 * s, 0.005, 0.2), (0.1 * s, 0.01, 0.1)], [0.068, 0.062], 10, 1),
              "leather")
    pts = []
    for i in range(9):
        z = -0.4 + 0.8 * i / 8
        pts.append(Vector((0, -0.11 * (1 - (z / 0.4) ** 2), z)))
    base = Matrix.Translation((0.29, -0.03, 0.49)) @ M(rot=(20, 0, 0))
    k.add(tube(pts, [0.012, 0.02, 0.024, 0.026, 0.026, 0.026, 0.024, 0.02, 0.012], 8, 1), "Hand.L", "wood",
          J["Hand.L"][2] @ base, final=True, level=1, smooth=True)
    k.add(tube([pts[0], pts[-1]], [0.004, 0.004], 4, 1), "Hand.L", "white", J["Hand.L"][2] @ base, final=True)
    return k


def mage_sd():
    k = Model()
    sd_humanoid(k)
    J = joints(k, SD_MAGE_REST)
    body(k, J, "purple", "purple", "dark", "leather", "skin")
    face(k, J, brow="white")
    soft_tube(k, J, [("Hips", (0, 0, 0.6)), ("Hips", (0, 0, 0.36)), ("Hips", (0, 0, 0.06))],
              [(0.17, 0.135), (0.22, 0.18), (0.27, 0.23)],
              ["Hips", "UpperLeg.L", "UpperLeg.R", "LowerLeg.L", "LowerLeg.R"], "purple", segs=16, samples=3,
              caps=(False, False))
    rigid(k, J, "Hips", ring(1.0, 0.06, 24, 8), "gold", M((0, 0, 0.07), scale=(0.27, 0.23, 0.3)), level=0)
    belt(k, J, "gold", buckle="blue")
    for s, side in ((1, "L"), (-1, "R")):
        soft_tube(k, J, [("Spine", (0.16 * s, 0, 0.85)), (f"UpperArm.{side}", (0.25 * s, 0, 0.68)),
                         (f"LowerArm.{side}", (0.28 * s, -0.02, 0.56))],
                  [0.07, 0.075, 0.105], ["Spine", f"UpperArm.{side}", f"LowerArm.{side}"], "purple",
                  caps=(True, False))
    rigid(k, J, "Head", sphere(12, 8), "white", M((0, -0.22, 0.98), (-25, 0, 0), (0.17, 0.1, 0.2)))
    rigid(k, J, "Head", prism(1.0, 1.0, 10, 0.0), "white", M((0, -0.25, 0.78), (190, 0, 0), (0.1, 0.07, 0.16)))
    for s in (1, -1):
        rigid(k, J, "Head", sphere(8, 5), "white", M((0.06 * s, -0.33, 1.1), (0, -25 * s, 0), (0.07, 0.035, 0.03)))
    rigid(k, J, "Head", limb(1.0, 1.0, 0.035, 1.0, 24), "purple", M((0, 0.02, 1.47), (-8, 0, 0), (0.95, 0.9, 1)))
    rigid(k, J, "Head", tube([(0, 0.02, 1.47), (0, 0.04, 1.7), (0, 0.12, 1.88), (0, 0.27, 1.96)],
                             [0.3, 0.18, 0.08, 0.015], 14, 3), "purple")
    rigid(k, J, "Head", ring(1.0, 0.1, 24, 8), "gold", M((0, 0.022, 1.52), (-8, 0, 0), (0.29, 0.29, 0.35)), level=0)
    staff = Matrix.Translation((-0.29, -0.03, 0.49)) @ M(rot=(50, 0, 0))
    rigid(k, J, "Hand.R", tube([(0, 0, -0.45), (0, 0, 0.3), (0, 0, 0.85)], [0.024, 0.026, 0.03], 8, 3), "wood", staff)
    for a in (0, 120, 240):
        claw = staff @ M((0, 0, 0.88)) @ M(rot=(0, 0, a))
        rigid(k, J, "Hand.R", tube([(0.02, 0, 0), (0.08, 0, 0.06), (0.05, 0, 0.16)], [0.016, 0.014, 0.006], 6, 2),
              "gold", claw)
    rigid(k, J, "Hand.R", sphere(12, 9), "blue", staff @ M((0, 0, 0.99), scale=(0.075, 0.075, 0.075)), level=1)
    return k


def goblin_sd():
    k = Model()
    sd_humanoid(k)
    J = joints(k, SD_GOBLIN_REST)
    gc, gr = HC + Vector((0, -0.02, -0.02)), Vector((0.4, 0.34, 0.32))
    body(k, J, "goblin", "goblin", "goblin", "goblin", "goblin", torso_r=(0.17, 0.15))
    face(k, J, skin="goblin", eye="gold", brow="black", c=gc, r=gr, eye_size=1.25, ears=False, blush=False,
         mouth=False)
    for s in (1, -1):
        p, n = surface((0.36 * s, -0.92, -0.08), gc, gr, 0.0)
        rigid(k, J, "Head", sphere(8, 6), "black", Matrix.Translation(p + n * 0.012) @ facing(n) @
              Matrix.Diagonal((0.03, 0.012, 0.05, 1)))
        rigid(k, J, "Head", tube([(gc.x + 0.34 * s, gc.y + 0.04, gc.z + 0.02), (gc.x + 0.55 * s, 0.08, gc.z + 0.12),
                                  (gc.x + 0.7 * s, 0.12, gc.z + 0.2)], [(0.07, 0.03), (0.05, 0.02), 0.006], 8, 2),
              "goblin")
        decal(k, J, (0.1 * s, -0.9, -0.55), (0.022, 0.03, 0.04), "white", gc, gr,
              geo=prism(1.0, 1.0, 6, 0.0), level=0, inset=0.0)
    rigid(k, J, "Head", tube([surface((0, -1, -0.1), gc, gr, 0.02)[0], (0, gc.y - gr.y - 0.1, gc.z - 0.08),
                              (0, gc.y - gr.y - 0.16, gc.z - 0.14)], [0.05, 0.035, 0.012], 8, 2), "goblin")
    decal(k, J, (0, -0.9, -0.5), (0.13, 0.015, 0.03), "mouth", gc, gr)
    soft_tube(k, J, [("Hips", (0, 0, 0.56)), ("Hips", (0, 0, 0.36))], [(0.18, 0.16), (0.2, 0.18)],
              ["Hips", "UpperLeg.L", "UpperLeg.R"], "leather", segs=14, samples=2, caps=(False, False))
    belt(k, J, "red", buckle=None, z=0.55, r=(0.18, 0.16))
    k.soft(tube([at(J, "Spine", (0.15, -0.13, 0.86)), at(J, "Spine", (0.0, -0.17, 0.72)),
                 at(J, "Hips", (-0.16, -0.13, 0.56))], [0.02] * 3, 6, 2), ["Spine", "Hips"], "leather")
    rigid(k, J, "UpperArm.L", dome(10, 4, -0.2, 0.12), "dark", M((0.2, 0, 0.84), (0, 25, 0), (0.11, 0.11, 0.09)))
    for a in (-30, 30):
        rigid(k, J, "UpperArm.L", prism(1.0, 1.0, 6, 0.0), "white",
              M((0.22, 0, 0.92), (0, 25 + a, 0), (0.018, 0.018, 0.07)))
    club = Matrix.Translation((-0.29, -0.03, 0.49)) @ M(rot=(15, 0, 0))
    rigid(k, J, "Hand.R", tube([(0, 0.08, 0), (0, -0.12, 0), (0, -0.4, 0), (0, -0.55, 0)],
                               [0.028, 0.035, 0.06, 0.05], 10, 2), "wood", club)
    for a in (0, 90, 180, 270):
        rigid(k, J, "Hand.R", prism(1.0, 1.0, 5, 0.0), "dark",
              club @ M((0, -0.45, 0)) @ M(rot=(0, a, 0)) @ M((0, 0, 0.07), scale=(0.015, 0.015, 0.05)))
    return k


def horse_sd():
    h = Model()
    h.bone("Body", (0, 0.3, 0.62), (0, -0.25, 0.62))
    h.bone("Neck", (0, -0.28, 0.72), (0, -0.4, 0.98), "Body")
    h.bone("HorseHead", (0, -0.4, 0.98), (0, -0.68, 0.92), "Neck")
    h.bone("Tail", (0, 0.4, 0.74), (0, 0.56, 0.45), "Body")
    legs = {"FL": (0.14, -0.24), "FR": (-0.14, -0.24), "BL": (0.14, 0.24), "BR": (-0.14, 0.24)}
    for n, (x, y) in legs.items():
        h.bone(f"{n}_Up", (x, y, 0.48), (x, y, 0.27), "Body")
        h.bone(f"{n}_Low", (x, y, 0.27), (x, y, 0.07), f"{n}_Up")
        h.bone(f"{n}_Hoof", (x, y, 0.07), (x, y - 0.03, 0.0), f"{n}_Low")
    J = joints(h, {})
    for n, (x, y) in legs.items():
        soft_tube(h, J, [("Body", (x, y, 0.62)), (f"{n}_Low", (x, y, 0.27)), (f"{n}_Hoof", (x, y, 0.08))],
                  [0.085, 0.06, 0.066], ["Body", f"{n}_Up", f"{n}_Low", f"{n}_Hoof"], "coat", segs=8)
        rigid(h, J, f"{n}_Hoof", limb(0.15, 0.16, 0.08, 0.82, 12), "hoof", M((x, y - 0.01, 0.04)))
        rigid(h, J, f"{n}_Hoof", ring(1.0, 0.25, 16, 6), "white", M((x, y - 0.01, 0.1), scale=(0.07, 0.07, 0.06)), level=0)
    rigid(h, J, "Body", sphere(14, 9), "coat", M((0, 0.0, 0.63), scale=(0.26, 0.42, 0.25)))
    rigid(h, J, "Body", sphere(10, 7), "coat", M((0, -0.28, 0.67), scale=(0.22, 0.18, 0.22)))
    soft_tube(h, J, [("Body", (0, -0.24, 0.66)), ("Neck", (0, -0.36, 0.86)), ("HorseHead", (0, -0.43, 1.0))],
              [(0.13, 0.15), (0.11, 0.13), (0.1, 0.11)], ["Body", "Neck", "HorseHead"], "coat", segs=12)
    hc, hr = Vector((0, -0.56, 1.0)), Vector((0.16, 0.24, 0.17))
    rigid(h, J, "HorseHead", sphere(12, 9), "coat", M(hc, (18, 0, 0), hr))
    rigid(h, J, "HorseHead", sphere(10, 7), "tan", M((0, -0.74, 0.9), (18, 0, 0), (0.125, 0.12, 0.1)))
    for s in (1, -1):
        rigid(h, J, "HorseHead", sphere(6, 4), "black", M((0.05 * s, -0.84, 0.92), scale=(0.018, 0.012, 0.022)))
        p = hc + Vector((0.14 * s, -0.08, 0.06))
        n = Vector((0.9 * s, -0.35, 0.2)).normalized()
        rigid(h, J, "HorseHead", sphere(12, 8), "black",
              Matrix.Translation(p) @ facing(n) @ Matrix.Diagonal((0.045, 0.02, 0.06, 1)), level=0)
        rigid(h, J, "HorseHead", sphere(6, 4), "white", M(p + n * 0.012 + Vector((0, 0, 0.025)), scale=(0.015,) * 3),
              level=0)
        rigid(h, J, "HorseHead", tube([(0.07 * s, -0.46, 1.12), (0.085 * s, -0.45, 1.22)], [(0.035, 0.022), 0.006], 8, 2),
              "coat")
    rigid(h, J, "HorseHead", ring(1.0, 0.1, 24, 8), "leather", M((0, -0.72, 0.92), (108, 0, 0), (0.13, 0.11, 0.2)), level=0)
    for i in range(6):
        t = i / 5
        p = (0, -0.22 - 0.24 * t, 0.86 + 0.3 * t)
        bone = "Neck" if t > 0.3 else "Body"
        rigid(h, J, bone, sphere(12, 8), "mane", M(p, (-35, 0, 0), (0.05, 0.08, 0.075)), level=0)
    rigid(h, J, "HorseHead", sphere(8, 6), "mane", M((0, -0.5, 1.17), (-50, 0, 0), (0.05, 0.08, 0.06)))
    soft_tube(h, J, [("Body", (0, 0.38, 0.76)), ("Tail", (0, 0.5, 0.66)), ("Tail", (0, 0.56, 0.45)),
                     ("Tail", (0, 0.55, 0.3))], [0.05, 0.08, 0.075, 0.03], ["Body", "Tail"], "mane", segs=10)
    rigid(h, J, "Body", limb(0.56, 0.5, 0.16, 1.0, 16), "blue", M((0, 0.02, 0.82), scale=(1, 1, 1)))
    rigid(h, J, "Body", ring(1.0, 0.04, 24, 8), "gold", M((0, 0.02, 0.74), scale=(0.29, 0.26, 0.4)), level=0)
    rigid(h, J, "Body", limb(0.32, 0.34, 0.07, 0.9, 12), "leather", M((0, 0.02, 0.91)))
    rigid(h, J, "Body", tube([(-0.12, 0.17, 0.95), (0, 0.19, 0.98), (0.12, 0.17, 0.95)], [0.03] * 3, 8, 2), "leather")
    rigid(h, J, "Body", tube([(-0.07, -0.13, 0.95), (0, -0.15, 0.99), (0.07, -0.13, 0.95)], [0.03] * 3, 8, 2), "leather")
    for s in (1, -1):
        h.soft(tube([at(J, "HorseHead", (0.1 * s, -0.74, 0.92)), at(J, "Neck", (0.12 * s, -0.42, 0.95)),
                     at(J, "Body", (0.06 * s, -0.12, 0.96))], [0.01] * 3, 5, 3), ["HorseHead", "Neck", "Body"],
               "leather")
    return h


def scale_model(model, s):
    m = Matrix.Scale(s, 4)
    model.bones = [(n, m @ h, m @ t, p) for n, h, t, p in model.bones]
    for part in model.parts:
        part["v"] = [m @ v for v in part["v"]]
    return model
