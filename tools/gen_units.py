"""Write assets/{knight,horse,knight_mounted,archer,goblin,mage}.glb and
static props assets/prop_{crate,rock,tree}.glb (1:1 game scale, no skeleton).

SD (super-deformed, 2.5 heads tall) units from tools/unitgen/sd.py: swept
tubes for torso, limbs, robes and capes with smooth envelope weights across
the bones they span, rigid heads, gear and weapons, Subdivision Surface per
part level, smooth shading, ~10k triangles (decimated only above the cap).
One mesh, one primitive, one material (assets/knight_palette.png, colour by
UV cell). Blender units are 2x game size (battle baseScale 0.5): units 1.8
tall, footprints inside 1.8 x 1.8 (one 1 m cell in game). Faces -Y in
Blender (+Z in glTF), feet at z = 0. Clips Idle/Walk/Attack/Hurt/Death use
the test_rig durations so the ClipEvents times (Attack hit 0.35 s, Walk feet
0 / 0.5 s) line up.

Run: blender -b --factory-startup -P tools/gen_units.py
Clears the startup scene, builds, exports, saves art/units.blend.
Headless only: glTF export from a non-active scene crashes Blender 5.1.
"""
import sys
from pathlib import Path

import bpy
from mathutils import Matrix

sys.path.insert(0, str(Path(__file__).resolve().parent))

from unitgen import blender_io as io  # noqa: E402
from unitgen.clips import archer_clips, horse_clips, knight_clips, mage_clips, merge_clips  # noqa: E402
from unitgen.geo import Model, fk  # noqa: E402
from unitgen.palette import palette  # noqa: E402
from unitgen.props import crate_model, rock_model, tree_model  # noqa: E402
from unitgen.sd import (SD_ARCHER_REST, SD_GOBLIN_REST, SD_KNIGHT_REST, SD_MAGE_REST, SD_RIDER_REST,  # noqa: E402
                        archer_sd, goblin_sd, horse_sd, knight_sd, mage_sd, scale_model)

ROOT = Path(__file__).resolve().parent.parent
SCENE = "Units"
CAP = 10000
MOUNTED_CAP = 20000
GOBLIN_SCALE = 0.8
RIDER_OFFSET = Matrix.Translation((0, 0.04, 0.5))


def mounted_model():
    horse = fk(horse_sd(), {})
    rider = fk(knight_sd(SD_RIDER_REST, RIDER_OFFSET), SD_RIDER_REST, RIDER_OFFSET)
    rider.bones = [(n, h, t, p or "Body") for n, h, t, p in rider.bones]
    out = Model()
    out.merge(horse)
    out.merge(rider)
    return out


def main():
    bpy.context.preferences.filepaths.save_version = 0
    scn = bpy.context.scene
    scn.name = SCENE
    for o in list(scn.objects):
        bpy.data.objects.remove(o)
    scn.render.fps = io.FPS
    scn.unit_settings.system = "METRIC"
    mat = palette(ROOT)

    jobs = [
        ("knight", fk(knight_sd(), SD_KNIGHT_REST), knight_clips(), CAP, -2.5),
        ("horse", fk(horse_sd(), {}), horse_clips(), CAP, 0.0),
        ("knight_mounted", mounted_model(),
         merge_clips(horse_clips(mounted=True), knight_clips(mounted=True)), MOUNTED_CAP, 2.5),
        ("archer", fk(archer_sd(), SD_ARCHER_REST), archer_clips(), CAP, 5.0),
        ("goblin", scale_model(fk(goblin_sd(), SD_GOBLIN_REST), GOBLIN_SCALE), knight_clips(), CAP, 7.5),
        ("mage", fk(mage_sd(), SD_MAGE_REST), mage_clips(), CAP, 10.0),
    ]
    report = {}
    for name, model, clips, cap, x in jobs:
        arm_ob, mesh_ob = io.build(scn, name, model, mat, cap)
        io.animate(arm_ob, mesh_ob, clips)
        io.export(scn, arm_ob, mesh_ob, ROOT / "assets" / f"{name}.glb")
        for track in arm_ob.animation_data.nla_tracks:
            track.mute = True
        arm_ob.animation_data.action = bpy.data.actions[f"{name}_Idle"]
        arm_ob.location.x = x
        io.cell_ref(scn, x)
        report[name] = {"bones": len(model.bones), "tris": io.tri_count(mesh_ob)}
    for i, (name, model) in enumerate((("prop_crate", crate_model()), ("prop_rock", rock_model()),
                                       ("prop_tree", tree_model()))):
        ob = io.make_mesh(scn, name, model, mat)
        io.export_static(scn, ob, ROOT / "assets" / f"{name}.glb")
        ob.location.x = 12.5 + 2.5 * i
        ob.scale = (2.0, 2.0, 2.0)
        io.cell_ref(scn, ob.location.x)
        report[name] = {"tris": io.tri_count(ob)}
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT / "art" / "units.blend"))
    return report


result = main()
print(result)
