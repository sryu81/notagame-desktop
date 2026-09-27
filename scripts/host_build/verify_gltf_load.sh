#!/usr/bin/env bash
# Host-build verification of gltf::load() with a real rigged .glb.
# Step 1: generate a minimal valid .glb (1 quad, 1 bone, 1 anim channel).
# Step 2: compile a host test program that loads it and asserts linear interp.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$HERE/../.." && pwd)"
ENGINE_DIR="$PROJECT_ROOT/app/src/main/cpp/engine"
BUILD_DIR="${BUILD_DIR:-/tmp/opencode/gltftest}"
STAGE="$BUILD_DIR/engine"
GLB="$BUILD_DIR/minimal_rigged.glb"

mkdir -p "$STAGE"

# Real engine headers first
cp -f "$ENGINE_DIR"/*.h "$STAGE/" 2>/dev/null || true
cp -f "$ENGINE_DIR/GltfLoader.cpp" "$STAGE/"
cp -f "$ENGINE_DIR/Math.cpp"        "$STAGE/"
cp -f "$ENGINE_DIR/Animation.cpp"    "$STAGE/"
cp -f "$ENGINE_DIR/Bone.cpp"        "$STAGE/" 2>/dev/null || true

# Override AndroidOut.h LAST so it wins
cp -f "$HERE/stubs/AndroidOut.h"   "$STAGE/AndroidOut.h"
cp -f "$HERE/stubs/AndroidOut.cpp" "$BUILD_DIR/AndroidOut.cpp"

# Generate .glb
python3 "$HERE/make_rigged_glb.py"

# Compile
g++ -std=c++17 -O0 -g \
    -I "$BUILD_DIR" \
    -I "$STAGE" \
    "$STAGE/GltfLoader.cpp" \
    "$STAGE/Math.cpp" \
    "$STAGE/Animation.cpp" \
    "$STAGE/Bone.cpp" \
    "$BUILD_DIR/AndroidOut.cpp" \
    "$HERE/load_real_glb.cpp" \
    -o "$BUILD_DIR/load_real"

# Run
echo "[host-build] running $BUILD_DIR/load_real against $GLB"
"$BUILD_DIR/load_real"
