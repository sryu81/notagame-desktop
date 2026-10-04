import math

import bmesh
from mathutils import Euler, Matrix, Vector


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


def tube(points, radii, segs=10, samples=3, caps=(True, True)):
    pts = [Vector(p) for p in points]
    rad = [r if isinstance(r, tuple) else (r, r) for r in radii]
    ring_p, ring_r = [], []
    for i in range(len(pts) - 1):
        for k in range(samples):
            t = k / samples
            ring_p.append(pts[i].lerp(pts[i + 1], t))
            ring_r.append((rad[i][0] + (rad[i + 1][0] - rad[i][0]) * t, rad[i][1] + (rad[i + 1][1] - rad[i][1]) * t))
    ring_p.append(pts[-1])
    ring_r.append(rad[-1])
    n = len(ring_p)
    tangents = []
    for i in range(n):
        a = ring_p[max(i - 1, 0)]
        b = ring_p[min(i + 1, n - 1)]
        tangents.append((b - a).normalized())
    ref = Vector((1, 0, 0)) if abs(tangents[0].x) < 0.9 else Vector((0, 1, 0))
    normal = (ref - tangents[0] * ref.dot(tangents[0])).normalized()
    v, f = [], []
    for i in range(n):
        t = tangents[i]
        normal = (normal - t * normal.dot(t)).normalized()
        bi = t.cross(normal)
        rx, ry = ring_r[i]
        for j in range(segs):
            a = 2 * math.pi * j / segs
            v.append(ring_p[i] + normal * (math.cos(a) * rx) + bi * (math.sin(a) * ry))
    for i in range(n - 1):
        for j in range(segs):
            k = (j + 1) % segs
            f.append((i * segs + j, i * segs + k, (i + 1) * segs + k, (i + 1) * segs + j))
    if caps[0]:
        v.append(ring_p[0] - tangents[0] * min(ring_r[0]) * 0.5)
        c = len(v) - 1
        f += [(c, (j + 1) % segs, j) for j in range(segs)]
    if caps[1]:
        v.append(ring_p[-1] + tangents[-1] * min(ring_r[-1]) * 0.5)
        c = len(v) - 1
        b = (n - 1) * segs
        f += [(b + j, b + (j + 1) % segs, c) for j in range(segs)]
    return [tuple(x) for x in v], f


def ring(radius, minor, segs=16, minor_segs=6):
    v, f = [], []
    for i in range(segs):
        a = 2 * math.pi * i / segs
        c, s_ = math.cos(a), math.sin(a)
        for j in range(minor_segs):
            b = 2 * math.pi * j / minor_segs
            r = radius + minor * math.cos(b)
            v.append((r * c, r * s_, minor * math.sin(b)))
    for i in range(segs):
        for j in range(minor_segs):
            a, b = i * minor_segs + j, i * minor_segs + (j + 1) % minor_segs
            c_, d = ((i + 1) % segs) * minor_segs + (j + 1) % minor_segs, ((i + 1) % segs) * minor_segs + j
            f.append((a, b, c_, d))
    return v, f


def dome(seg=12, rings=5, cut=0.0, thick=0.04):
    lo = math.asin(max(-1.0, min(1.0, cut)))
    outer, inner = [], []
    for r in range(rings + 1):
        phi = lo + (math.pi / 2 - lo) * r / rings
        for i in range(seg):
            a = 2 * math.pi * i / seg
            d = (math.cos(phi) * math.cos(a), math.cos(phi) * math.sin(a), math.sin(phi))
            outer.append(d)
            inner.append(tuple(x * (1.0 - thick) for x in d))
    v = outer + inner
    off = len(outer)
    f = []
    for r in range(rings):
        for i in range(seg):
            j = (i + 1) % seg
            a, b, c, d = r * seg + i, r * seg + j, (r + 1) * seg + j, (r + 1) * seg + i
            f.append((a, b, c, d))
            f.append((off + d, off + c, off + b, off + a))
    for i in range(seg):
        j = (i + 1) % seg
        f.append((off + i, off + j, j, i))
    return v, f


def facing(direction, up=(0, 0, 1)):
    d = Vector(direction).normalized()
    return Vector((0, -1, 0)).rotation_difference(d).to_matrix().to_4x4()


def seg_dist(p, a, b):
    ab = b - a
    t = 0.0 if ab.length_squared < 1e-12 else max(0.0, min(1.0, (p - a).dot(ab) / ab.length_squared))
    return (p - (a + ab * t)).length


def envelope(p, bones, segs, keep=2):
    ws = []
    for b in bones:
        h, t = segs[b]
        ws.append((1.0 / (seg_dist(p, h, t) + 0.015) ** 4, b))
    ws.sort(reverse=True)
    ws = ws[:keep]
    total = sum(w for w, _ in ws)
    out = [(b, w / total) for w, b in ws if w / total > 0.04]
    total = sum(w for _, w in out)
    return [(b, w / total) for b, w in out]


class Model:
    def __init__(self):
        self.bones = []
        self.parts = []

    def bone(self, name, head, tail, parent=None):
        self.bones.append((name, Vector(head), Vector(tail), parent))

    def add(self, geo, bone, color, m=None, final=False, level=0, smooth=False):
        v, f = geo
        m = m or Matrix()
        self.parts.append({"v": [m @ Vector(p) for p in v], "f": f, "bone": bone, "color": color,
                           "final": final, "level": level, "smooth": smooth})

    def soft(self, geo, bones, color, m=None, level=1):
        self.add(geo, list(bones), color, m, final=True, level=level, smooth=True)

    def along(self, a, b, w, d, bone, color, top=(1.0, 1.0), segs=0):
        m, length = along(a, b)
        geo = limb(w, d, length, top[0], segs) if segs else box(w, d, length, top)
        self.add(geo, bone, color, m)

    def merge(self, other):
        self.bones += other.bones
        self.parts += other.parts


def pose_world(model, pose, offset=Matrix()):
    rest = {n: (h, t, p) for n, h, t, p in model.bones}
    world = {}

    def W(n):
        if n not in world:
            h, _, p = rest[n]
            r = Euler([math.radians(a) for a in pose.get(n, (0, 0, 0))], "XYZ").to_matrix().to_4x4()
            parent = W(p) if p else offset
            world[n] = parent @ Matrix.Translation(h) @ r @ Matrix.Translation(-h)
        return world[n]

    for n in rest:
        W(n)
    return world


def joints(model, pose, offset=Matrix()):
    world = pose_world(model, pose, offset)
    out = {}
    for n, h, t, p in model.bones:
        parent = world[p] if p else offset
        out[n] = (parent @ h, world[n] @ t, world[n])
    return out


def fk(model, pose, offset=Matrix()):
    world = pose_world(model, pose, offset)
    bones = []
    for n, h, t, p in model.bones:
        parent = world[p] if p else offset
        bones.append((n, parent @ h, world[n] @ t, p))
    parts = []
    for part in model.parts:
        q = dict(part)
        if not part["final"]:
            q["v"] = [world[part["bone"]] @ p for p in part["v"]]
            q["final"] = True
        parts.append(q)
    out = Model()
    out.bones, out.parts = bones, parts
    return out
