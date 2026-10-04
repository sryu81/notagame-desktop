import math

import bmesh
import bpy
from mathutils import Euler, Vector

from .geo import envelope
from .palette import UV

FPS = 30


def mode(scn, ob, m):
    vl = scn.view_layers[0]
    vl.objects.active = ob
    with bpy.context.temp_override(scene=scn, view_layer=vl, active_object=ob, object=ob):
        bpy.ops.object.mode_set(mode=m)


def _cage(scn, name, parts, bones, segs):
    verts, faces, cols, smooth, weights = [], [], [], [], []
    for part in parts:
        base = len(verts)
        for p in part["v"]:
            verts.append(tuple(p))
            b = part["bone"]
            if b is None:
                weights.append([])
            elif isinstance(b, str):
                weights.append([(b, 1.0)])
            else:
                weights.append(envelope(Vector(p), b, segs))
        faces += [[base + i for i in fc] for fc in part["f"]]
        cols += [part["color"]] * len(part["f"])
        smooth += [part["smooth"]] * len(part["f"])
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    uv = me.uv_layers.new(name="UVMap")
    for poly in me.polygons:
        poly.use_smooth = smooth[poly.index]
        for li in poly.loop_indices:
            uv.data[li].uv = UV[cols[poly.index]]
    ob = bpy.data.objects.new(name, me)
    scn.collection.objects.link(ob)
    groups = {n: ob.vertex_groups.new(name=n) for n in bones}
    for i, ws in enumerate(weights):
        for b, w in ws:
            groups[b].add([i], w, "REPLACE")
    return ob


def _apply_modifiers(scn, ob):
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg), preserve_all_data_layers=True, depsgraph=dg)
    old = ob.data
    ob.modifiers.clear()
    ob.data = me
    bpy.data.meshes.remove(old)


def tri_count(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


def make_mesh(scn, name, model, mat, cap=0):
    bones = [b[0] for b in model.bones]
    segs = {n: (h, t) for n, h, t, _ in model.bones}
    objs = []
    for level in sorted({p["level"] for p in model.parts}):
        ob = _cage(scn, f"{name}_L{level}", [p for p in model.parts if p["level"] == level], bones, segs)
        if level > 0:
            mod = ob.modifiers.new("Subdiv", "SUBSURF")
            mod.levels = mod.render_levels = level
            _apply_modifiers(scn, ob)
        objs.append(ob)
    vl = scn.view_layers[0]
    if len(objs) > 1:
        with bpy.context.temp_override(scene=scn, view_layer=vl, active_object=objs[0], object=objs[0],
                                       selected_objects=objs, selected_editable_objects=objs):
            bpy.ops.object.join()
    ob = objs[0]
    ob.name = name + "_mesh"
    ob.data.name = name + "_mesh"
    if cap and tri_count(ob) > cap:
        mod = ob.modifiers.new("Cap", "DECIMATE")
        mod.ratio = cap / tri_count(ob) * 0.98
        _apply_modifiers(scn, ob)
    limit_weights(ob)
    ob.data.materials.clear()
    ob.data.materials.append(mat)
    return ob


def limit_weights(ob, keep=4):
    groups = ob.vertex_groups
    for v in ob.data.vertices:
        ws = sorted(((g.weight, g.group) for g in v.groups), reverse=True)
        if not ws:
            continue
        drop = [gi for w, gi in ws[keep:]] + [gi for w, gi in ws[:keep] if w < 1e-4]
        for gi in drop:
            groups[gi].remove([v.index])
        kept = [(w, gi) for w, gi in ws[:keep] if w >= 1e-4]
        total = sum(w for w, _ in kept)
        for w, gi in kept:
            groups[gi].add([v.index], w / total, "REPLACE")


def build(scn, name, model, mat, cap=0):
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

    ob = make_mesh(scn, name, model, mat, cap)
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


def export_static(scn, mesh_ob, path):
    vl = scn.view_layers[0]
    for o in scn.objects:
        o.select_set(False, view_layer=vl)
    mesh_ob.select_set(True, view_layer=vl)
    vl.objects.active = mesh_ob
    with bpy.context.temp_override(scene=scn, view_layer=vl, active_object=mesh_ob):
        bpy.ops.export_scene.gltf(filepath=str(path), export_format="GLB", use_selection=True,
                                  export_animations=False, export_skins=False, export_yup=True,
                                  export_cameras=False, export_lights=False)


def cell_ref(scn, x):
    me = bpy.data.meshes.new("cell_ref")
    me.from_pydata([(-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)], [(0, 1), (1, 2), (2, 3), (3, 0)], [])
    ob = bpy.data.objects.new("cell_ref", me)
    ob.location.x = x
    scn.collection.objects.link(ob)
