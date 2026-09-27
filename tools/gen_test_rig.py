"""Write assets/test_rig.glb: a 15-joint box humanoid with Idle/Walk/Attack/Hurt.

Every value is fixed so AnimLab can assert exact numbers (DEMO_PLAN P2.5).
One mesh, one primitive, rigid skinning (one box per joint, weight 1.0),
joints parent-first, legs straight down -Y, +Y up, facing +Z. Clips are
linear keys sampled at 30 fps. Stdlib only.
Regenerate with: python3 tools/gen_test_rig.py [out.glb]
"""
import json
import math
import os
import struct
import sys

FPS = 30

# name, parent, rest offset from parent
JOINTS = [
    ("Hips",       None,         (0.0, 1.00, 0.0)),
    ("Spine",      "Hips",       (0.0, 0.25, 0.0)),
    ("Head",       "Spine",      (0.0, 0.35, 0.0)),
    ("UpperArm.L", "Spine",      (0.22, 0.28, 0.0)),
    ("LowerArm.L", "UpperArm.L", (0.0, -0.28, 0.0)),
    ("Hand.L",     "LowerArm.L", (0.0, -0.25, 0.0)),
    ("UpperArm.R", "Spine",      (-0.22, 0.28, 0.0)),
    ("LowerArm.R", "UpperArm.R", (0.0, -0.28, 0.0)),
    ("Hand.R",     "LowerArm.R", (0.0, -0.25, 0.0)),
    ("UpperLeg.L", "Hips",       (0.12, 0.0, 0.0)),
    ("LowerLeg.L", "UpperLeg.L", (0.0, -0.45, 0.0)),
    ("Foot.L",     "LowerLeg.L", (0.0, -0.45, 0.0)),
    ("UpperLeg.R", "Hips",       (-0.12, 0.0, 0.0)),
    ("LowerLeg.R", "UpperLeg.R", (0.0, -0.45, 0.0)),
    ("Foot.R",     "LowerLeg.R", (0.0, -0.45, 0.0)),
]
INDEX = {name: i for i, (name, _, _) in enumerate(JOINTS)}

# box per joint: (min, max) relative to the joint's model position
BOXES = {
    "Hips":       ((-0.16, -0.08, -0.10), (0.16, 0.08, 0.10)),
    "Spine":      ((-0.15, 0.00, -0.09), (0.15, 0.33, 0.09)),
    "Head":       ((-0.11, 0.02, -0.11), (0.11, 0.24, 0.11)),
    "UpperArm.L": ((-0.045, -0.27, -0.045), (0.045, 0.0, 0.045)),
    "LowerArm.L": ((-0.04, -0.24, -0.04), (0.04, 0.0, 0.04)),
    "Hand.L":     ((-0.04, -0.09, -0.04), (0.04, 0.0, 0.04)),
    "UpperLeg.L": ((-0.06, -0.44, -0.06), (0.06, 0.0, 0.06)),
    "LowerLeg.L": ((-0.05, -0.44, -0.05), (0.05, 0.0, 0.05)),
    "Foot.L":     ((-0.06, -0.10, -0.06), (0.06, 0.0, 0.16)),
}
for name in list(BOXES):
    if name.endswith(".L"):
        (x0, y0, z0), (x1, y1, z1) = BOXES[name]
        BOXES[name[:-2] + ".R"] = ((-x1, y0, z0), (-x0, y1, z1))

# (time, joint) -> event ids live in the game's ClipEvents table; print them
EVENTS = {"Attack": [(0.35, 1)], "Walk": [(0.0, 2), (0.5, 3)]}


def model_pos(i):
    name, parent, off = JOINTS[i]
    if parent is None:
        return off
    p = model_pos(INDEX[parent])
    return (p[0] + off[0], p[1] + off[1], p[2] + off[2])


def quat_x(deg):
    a = math.radians(deg) * 0.5
    return (math.sin(a), 0.0, 0.0, math.cos(a))


def quat_y(deg):
    a = math.radians(deg) * 0.5
    return (0.0, math.sin(a), 0.0, math.cos(a))


def piecewise(keys, t):
    for (t0, v0), (t1, v1) in zip(keys, keys[1:]):
        if t0 <= t <= t1:
            return v0 + (v1 - v0) * (t - t0) / (t1 - t0)
    return keys[-1][1]


def sampled(length, fn):
    n = int(round(length * FPS))
    return [(i / FPS, fn(i / FPS)) for i in range(n + 1)]


def clips():
    tau = 2.0 * math.pi
    hips = JOINTS[INDEX["Hips"]][2]
    walk_leg = lambda s: (lambda t: quat_x(-25.0 * s * math.sin(tau * t)))
    walk_knee = lambda s: (lambda t: quat_x(40.0 * max(0.0, s * math.sin(tau * t + math.pi / 2))))
    walk_arm = lambda s: (lambda t: quat_x(20.0 * s * math.sin(tau * t)))
    return {
        "Idle": (2.0, [
            ("Spine", "rotation", lambda t: quat_x(3.0 * math.sin(tau * t / 2.0))),
            ("Head", "rotation", lambda t: quat_y(10.0 * math.sin(tau * t / 2.0))),
        ]),
        "Walk": (1.0, [
            ("UpperLeg.L", "rotation", walk_leg(1.0)),
            ("UpperLeg.R", "rotation", walk_leg(-1.0)),
            ("LowerLeg.L", "rotation", walk_knee(1.0)),
            ("LowerLeg.R", "rotation", walk_knee(-1.0)),
            ("UpperArm.L", "rotation", walk_arm(-1.0)),
            ("UpperArm.R", "rotation", walk_arm(1.0)),
            ("Hips", "translation",
             lambda t: (hips[0], hips[1] + 0.03 * abs(math.sin(tau * t)), hips[2])),
        ]),
        "Attack": (0.6, [
            ("UpperArm.R", "rotation",
             lambda t: quat_x(piecewise([(0.0, 0.0), (0.25, -100.0), (0.35, -20.0), (0.6, 0.0)], t))),
        ]),
        "Hurt": (0.4, [
            ("Spine", "rotation",
             lambda t: quat_x(piecewise([(0.0, 0.0), (0.1, -15.0), (0.4, 0.0)], t))),
        ]),
    }


class Buffer:
    def __init__(self):
        self.data = bytearray()
        self.views = []
        self.accessors = []

    def add(self, raw, target=None):
        while len(self.data) % 4:
            self.data.append(0)
        view = {"buffer": 0, "byteOffset": len(self.data), "byteLength": len(raw)}
        if target:
            view["target"] = target
        self.data.extend(raw)
        self.views.append(view)
        return len(self.views) - 1

    def accessor(self, raw, comp, count, typ, target=None, minmax=None):
        acc = {"bufferView": self.add(raw, target), "componentType": comp, "count": count, "type": typ}
        if minmax:
            acc["min"], acc["max"] = minmax
        self.accessors.append(acc)
        return len(self.accessors) - 1


def build_mesh():
    positions, normals, joints, weights, indices = [], [], [], [], []
    faces = [
        ((1, 0, 0), [(1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1)]),
        ((-1, 0, 0), [(0, 0, 1), (0, 1, 1), (0, 1, 0), (0, 0, 0)]),
        ((0, 1, 0), [(0, 1, 1), (1, 1, 1), (1, 1, 0), (0, 1, 0)]),
        ((0, -1, 0), [(0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1)]),
        ((0, 0, 1), [(1, 0, 1), (1, 1, 1), (0, 1, 1), (0, 0, 1)]),
        ((0, 0, -1), [(0, 0, 0), (0, 1, 0), (1, 1, 0), (1, 0, 0)]),
    ]
    for j, (name, _, _) in enumerate(JOINTS):
        base = model_pos(j)
        lo, hi = BOXES[name]
        for n, corners in faces:
            start = len(positions)
            for cx, cy, cz in corners:
                positions.append((base[0] + (hi[0] if cx else lo[0]),
                                  base[1] + (hi[1] if cy else lo[1]),
                                  base[2] + (hi[2] if cz else lo[2])))
                normals.append(n)
                joints.append((j, 0, 0, 0))
                weights.append((1.0, 0.0, 0.0, 0.0))
            indices += [start, start + 1, start + 2, start, start + 2, start + 3]
    return positions, normals, joints, weights, indices


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "assets", "test_rig.glb")
    buf = Buffer()
    positions, normals, joints, weights, indices = build_mesh()
    mins = [min(p[k] for p in positions) for k in range(3)]
    maxs = [max(p[k] for p in positions) for k in range(3)]
    a_pos = buf.accessor(b"".join(struct.pack("<3f", *p) for p in positions), 5126,
                         len(positions), "VEC3", 34962, (mins, maxs))
    a_nrm = buf.accessor(b"".join(struct.pack("<3f", *n) for n in normals), 5126,
                         len(normals), "VEC3", 34962)
    a_jnt = buf.accessor(b"".join(struct.pack("<4B", *j) for j in joints), 5121,
                         len(joints), "VEC4", 34962)
    a_wgt = buf.accessor(b"".join(struct.pack("<4f", *w) for w in weights), 5126,
                         len(weights), "VEC4", 34962)
    a_idx = buf.accessor(b"".join(struct.pack("<H", i) for i in indices), 5123,
                         len(indices), "SCALAR", 34963)

    inv_bind = []
    for j in range(len(JOINTS)):
        x, y, z = model_pos(j)
        inv_bind += [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -x, -y, -z, 1]
    a_ibm = buf.accessor(struct.pack(f"<{len(inv_bind)}f", *inv_bind), 5126, len(JOINTS), "MAT4")

    nodes = []
    for name, parent, off in JOINTS:
        nodes.append({"name": name, "translation": list(off)})
    for i, (name, parent, _) in enumerate(JOINTS):
        if parent is not None:
            nodes[INDEX[parent]].setdefault("children", []).append(i)
    mesh_node = len(nodes)
    nodes.append({"name": "Body", "mesh": 0, "skin": 0})
    armature = len(nodes)
    nodes.append({"name": "Armature", "children": [INDEX["Hips"], mesh_node]})

    animations = []
    for clip_name, (length, tracks) in clips().items():
        samplers, channels = [], []
        for joint, path, fn in tracks:
            keys = sampled(length, fn)
            a_in = buf.accessor(struct.pack(f"<{len(keys)}f", *[k[0] for k in keys]), 5126,
                                len(keys), "SCALAR", None, ([0.0], [length]))
            width = 4 if path == "rotation" else 3
            flat = [c for _, v in keys for c in v]
            a_out = buf.accessor(struct.pack(f"<{len(flat)}f", *flat), 5126, len(keys),
                                 "VEC4" if width == 4 else "VEC3")
            samplers.append({"input": a_in, "output": a_out, "interpolation": "LINEAR"})
            channels.append({"sampler": len(samplers) - 1,
                             "target": {"node": INDEX[joint], "path": path}})
        animations.append({"name": clip_name, "samplers": samplers, "channels": channels})

    gltf = {
        "asset": {"version": "2.0", "generator": "tools/gen_test_rig.py"},
        "scene": 0,
        "scenes": [{"nodes": [armature]}],
        "nodes": nodes,
        "meshes": [{"name": "Body", "primitives": [{
            "attributes": {"POSITION": a_pos, "NORMAL": a_nrm, "JOINTS_0": a_jnt, "WEIGHTS_0": a_wgt},
            "indices": a_idx, "mode": 4}]}],
        "skins": [{"joints": list(range(len(JOINTS))), "inverseBindMatrices": a_ibm,
                   "skeleton": INDEX["Hips"]}],
        "animations": animations,
        "buffers": [{"byteLength": len(buf.data)}],
        "bufferViews": buf.views,
        "accessors": buf.accessors,
    }
    js = json.dumps(gltf, separators=(",", ":")).encode()
    js += b" " * ((4 - len(js) % 4) % 4)
    while len(buf.data) % 4:
        buf.data.append(0)
    body = struct.pack("<II", len(js), 0x4E4F534A) + js + struct.pack("<II", len(buf.data), 0x004E4942) + bytes(buf.data)
    glb = struct.pack("<III", 0x46546C67, 2, 12 + len(body)) + body
    with open(out, "wb") as f:
        f.write(glb)
    print(f"wrote {len(glb)}B, {len(JOINTS)} joints, {len(indices) // 3} tris, "
          f"clips {list(clips())} -> {os.path.normpath(out)}")
    print(f"events (must match src/game/ClipEvents.cpp): {EVENTS}")


if __name__ == "__main__":
    main()
