import struct, json

def pad4(b):
    while len(b) % 4: b += b' '
    return b

def build_glb(json_obj, bin_data):
    j = pad4(json.dumps(json_obj, separators=(',', ':')).encode())
    b = pad4(bin_data)
    total = 12 + 8 + len(j) + 8 + len(b)
    out = bytearray()
    out += struct.pack('<4sII', b'glTF', 2, total)
    out += struct.pack('<II', len(j), 0x4E4F534A)  # JSON chunk
    out += j
    out += struct.pack('<II', len(b), 0x004E4942)  # BIN chunk
    out += b
    return bytes(out)

# Build binary buffer (matches the offsets in the JSON below)
buf = bytearray()
def f32(*xs):
    for x in xs: buf.extend(struct.pack('<f', float(x)))
def u32(*xs):
    for x in xs: buf.extend(struct.pack('<I', int(x)))
def u8(*xs):
    for x in xs: buf.extend(struct.pack('<B', int(x)))

# POSITION (48B) — 4 verts on quad XY
f32(-1, -1, 0,  1, -1, 0,  1, 1, 0,  -1, 1, 0)
# NORMAL (48B)
f32(0, 0, 1,  0, 0, 1,  0, 0, 1,  0, 0, 1)
# JOINTS (16B) — all bone 0
u8(0,0,0,0,  0,0,0,0,  0,0,0,0,  0,0,0,0)
# WEIGHTS (64B) — weight 1 on bone 0
f32(1,0,0,0,  1,0,0,0,  1,0,0,0,  1,0,0,0)
# indices (24B)
u32(0, 1, 2,  0, 2, 3)
# inverseBindMatrices (64B) — identity
f32(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1)
# anim input (8B)
f32(0.0, 1.0)
# anim output (24B) — translations
f32(0,0,0, 0.5,0,0)
assert len(buf) == 296, f"got {len(buf)}"

j = {
  "asset": {"version": "2.0"},
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [
    {"mesh": 0, "skin": 0, "children": [1]},
    {"name": "bone0"}
  ],
  "meshes": [{"primitives": [{
    "attributes": {"POSITION": 0, "NORMAL": 1, "JOINTS_0": 2, "WEIGHTS_0": 3},
    "indices": 4
  }]}],
  "skins": [{"joints": [1], "inverseBindMatrices": 5}],
  "animations": [{"samplers": [
    {"input": 6, "output": 7, "interpolation": "LINEAR"}
  ], "channels": [
    {"sampler": 0, "target": {"node": 1, "path": "translation"}}
  ]}],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3",
     "min": [-1,-1,0], "max": [1,1,0]},
    {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3"},
    {"bufferView": 2, "componentType": 5121, "count": 4, "type": "VEC4"},
    {"bufferView": 3, "componentType": 5126, "count": 4, "type": "VEC4"},
    {"bufferView": 4, "componentType": 5125, "count": 6, "type": "SCALAR"},
    {"bufferView": 5, "componentType": 5126, "count": 1, "type": "MAT4"},
    {"bufferView": 6, "componentType": 5126, "count": 2, "type": "SCALAR",
     "min": [0.0], "max": [1.0]},
    {"bufferView": 7, "componentType": 5126, "count": 2, "type": "VEC3"},
  ],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0,   "byteLength": 48},
    {"buffer": 0, "byteOffset": 48,  "byteLength": 48},
    {"buffer": 0, "byteOffset": 96,  "byteLength": 16},
    {"buffer": 0, "byteOffset": 112, "byteLength": 64},
    {"buffer": 0, "byteOffset": 176, "byteLength": 24},
    {"buffer": 0, "byteOffset": 200, "byteLength": 64},
    {"buffer": 0, "byteOffset": 264, "byteLength": 8},
    {"buffer": 0, "byteOffset": 272, "byteLength": 24},
  ],
  "buffers": [{"byteLength": 296}]
}

data = build_glb(j, bytes(buf))
with open("/tmp/opencode/gltftest/minimal_rigged.glb", "wb") as f:
    f.write(data)
print(f"WROTE {len(data)}B -> /tmp/opencode/gltftest/minimal_rigged.glb")
