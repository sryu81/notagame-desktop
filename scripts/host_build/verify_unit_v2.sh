#!/usr/bin/env bash
# Verify unit_v2.glb: 3-bone skeleton, idle/attack clips, sampleSkeletal assertion.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$HERE/../.." && pwd)"
ENGINE_DIR="$PROJECT_ROOT/engine-module/src/main/cpp/engine"
GLB="$PROJECT_ROOT/app/src/main/assets/unit_v2.glb"
BUILD_DIR="${BUILD_DIR:-/tmp/opencode/unit_v2_verify}"
STAGE="$BUILD_DIR/engine"

mkdir -p "$STAGE"

cp -f "$ENGINE_DIR"/*.h     "$STAGE/"
cp -f "$ENGINE_DIR/GltfLoader.cpp" "$STAGE/"
cp -f "$ENGINE_DIR/Math.cpp"       "$STAGE/"
cp -f "$ENGINE_DIR/Animation.cpp"  "$STAGE/"
cp -f "$ENGINE_DIR/Bone.cpp"       "$STAGE/"
cp -f "$HERE/stubs/AndroidOut.h"   "$STAGE/AndroidOut.h"
cp -f "$HERE/stubs/AndroidOut.cpp" "$BUILD_DIR/AndroidOut.cpp"

# Regenerate .glb if missing
if [[ ! -f "$GLB" ]]; then
    echo "[verify_unit_v2] generating $GLB..."
    python3 "$HERE/../make_rigged_glb_v2.py"
fi

g++ -std=c++17 -O0 -g \
    -DUNIT_V2_GLB_PATH="\"$GLB\"" \
    -I "$BUILD_DIR" \
    -I "$STAGE" \
    "$STAGE/GltfLoader.cpp" \
    "$STAGE/Math.cpp" \
    "$STAGE/Animation.cpp" \
    "$STAGE/Bone.cpp" \
    "$BUILD_DIR/AndroidOut.cpp" \
    "$HERE/load_unit_v2.cpp" \
    -o "$BUILD_DIR/verify_unit_v2"

echo "[verify_unit_v2] running..."
"$BUILD_DIR/verify_unit_v2"
