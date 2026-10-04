from pathlib import Path

import bpy

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
    ("skin", (0.86, 0.66, 0.52)),
    ("goblin", (0.46, 0.62, 0.22)),
    ("green", (0.20, 0.42, 0.18)),
    ("purple", (0.42, 0.20, 0.58)),
    ("stone", (0.53, 0.52, 0.50)),
    ("bark", (0.40, 0.27, 0.15)),
    ("leaf", (0.33, 0.58, 0.22)),
    ("leafdark", (0.20, 0.42, 0.16)),
    ("mouth", (0.55, 0.20, 0.20)),
    ("tan", (0.80, 0.63, 0.45)),
    ("blush", (0.96, 0.58, 0.52)),
    ("hair", (0.36, 0.22, 0.12)),
]
CELL = 8
COLS = 8
PAL = 64
UV = {}
for _i, (_n, _c) in enumerate(COLORS):
    UV[_n] = (((_i % COLS) * CELL + CELL / 2) / PAL, ((_i // COLS) * CELL + CELL / 2) / PAL)


def palette(root):
    img = bpy.data.images.get("knight_palette")
    if img:
        bpy.data.images.remove(img)
    img = bpy.data.images.new("knight_palette", PAL, PAL)
    px = [0.0] * (PAL * PAL * 4)
    for i, (_, c) in enumerate(COLORS):
        cx, cy = i % COLS, i // COLS
        for y in range(cy * CELL, cy * CELL + CELL):
            for x in range(cx * CELL, cx * CELL + CELL):
                o = (y * PAL + x) * 4
                px[o:o + 4] = [c[0], c[1], c[2], 1.0]
    img.pixels[:] = px
    path = root / "assets" / "knight_palette.png"
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
