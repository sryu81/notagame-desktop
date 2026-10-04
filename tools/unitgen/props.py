import math

from .geo import M, Model, box, limb, prism, sphere


def jitter(geo, amt, seed):
    v, f = geo
    out = []
    for i, (x, y, z) in enumerate(v):
        h = math.sin(i * 12.9898 + seed * 78.233) * 43758.5453
        k = 1.0 + amt * ((h - math.floor(h)) - 0.5)
        out.append((x * k, y * k, z * k))
    return out, f


def crate_model():
    m = Model()
    m.add(box(0.66, 0.66, 0.66), None, "wood", M((0, 0, 0.35)))
    for sx in (1, -1):
        for sy in (1, -1):
            m.add(box(0.08, 0.08, 0.7), None, "bark", M((0.31 * sx, 0.31 * sy, 0.35)))
    for z in (0.04, 0.66):
        for sy in (1, -1):
            m.add(box(0.7, 0.08, 0.08), None, "bark", M((0, 0.31 * sy, z)))
            m.add(box(0.08, 0.7, 0.08), None, "bark", M((0.31 * sy, 0, z)))
    for sy in (1, -1):
        m.add(box(0.08, 0.02, 0.82), None, "bark", M((0, 0.335 * sy, 0.35), (0, 45, 0)))
        m.add(box(0.02, 0.08, 0.82), None, "bark", M((0.335 * sy, 0, 0.35), (45, 0, 0)))
    return m


def rock_model():
    m = Model()
    m.add(jitter(sphere(10, 6), 0.35, 1), None, "stone", M((0, 0, 0.18), scale=(0.4, 0.34, 0.26)))
    m.add(jitter(sphere(8, 5), 0.35, 2), None, "stone", M((0.26, -0.18, 0.1), scale=(0.18, 0.16, 0.14)))
    m.add(jitter(sphere(8, 4), 0.3, 3), None, "leaf", M((-0.05, 0.02, 0.36), scale=(0.22, 0.18, 0.06)))
    return m


def tree_model():
    m = Model()
    m.add(limb(0.2, 0.2, 0.95, 0.6, 8), None, "bark", M((0, 0, 0.475)))
    for a in (20, 140, 260):
        m.add(prism(0.06, 0.16, segs=5, top=0), None, "bark",
              M(rot=(0, 0, a)) @ M((0.1, 0, 0.04), (0, 70, 0)))
    for sx in (1, -1):
        m.along((0, 0, 0.72), (0.22 * sx, 0.05, 1.02), 0.07, 0.07, None, "bark", (0.7, 0.7), segs=6)
    m.add(jitter(sphere(10, 6), 0.25, 4), None, "leaf", M((0, 0, 1.25), scale=(0.38, 0.36, 0.32)))
    m.add(jitter(sphere(10, 6), 0.25, 5), None, "leafdark", M((0.2, 0.1, 1.08), scale=(0.24, 0.22, 0.2)))
    m.add(jitter(sphere(10, 6), 0.25, 6), None, "leafdark", M((-0.2, -0.08, 1.12), scale=(0.26, 0.24, 0.22)))
    m.add(jitter(sphere(10, 6), 0.25, 7), None, "leaf", M((0.02, -0.05, 1.5), scale=(0.24, 0.22, 0.2)))
    return m
