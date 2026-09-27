#!/usr/bin/env bash
# Host-build inspection of a .glb file for mobile suitability + rig validation.
# Usage: ./inspect_glb.sh <path-to.glb> [cellSize]

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$HERE/../.." && pwd)"
if [[ -d "$PROJECT_ROOT/engine-module/src/main/cpp/engine" ]]; then
    ENGINE_DIR="$PROJECT_ROOT/engine-module/src/main/cpp/engine"
else
    ENGINE_DIR="$PROJECT_ROOT/app/src/main/cpp/engine"
fi
BUILD_DIR="${BUILD_DIR:-/tmp/opencode/hostbuild}"
STAGE="$BUILD_DIR/engine"

GLB="${1:?usage: $0 <path-to.glb>}"
if [[ ! -f "$GLB" ]]; then echo "ERROR: $GLB not found"; exit 1; fi

mkdir -p "$STAGE"

# Stage engine headers + sources
cp -f "$ENGINE_DIR"/*.h "$STAGE/" 2>/dev/null || true
cp -f "$ENGINE_DIR/GltfLoader.cpp" "$STAGE/"
cp -f "$ENGINE_DIR/Math.cpp"        "$STAGE/"
cp -f "$ENGINE_DIR/Animation.cpp"   "$STAGE/"
cp -f "$ENGINE_DIR/Bone.cpp"        "$STAGE/" 2>/dev/null || true

# Override AndroidOut.h
cp -f "$HERE/stubs/AndroidOut.h"   "$STAGE/AndroidOut.h"
cp -f "$HERE/stubs/AndroidOut.cpp" "$BUILD_DIR/AndroidOut.cpp"

# Compile
g++ -std=c++17 -O0 -g \
    -I "$BUILD_DIR" \
    -I "$STAGE" \
    "$STAGE/GltfLoader.cpp" \
    "$STAGE/Math.cpp" \
    "$STAGE/Animation.cpp" \
    "$STAGE/Bone.cpp" \
    "$BUILD_DIR/AndroidOut.cpp" \
    "$HERE/inspect_glb.cpp" \
    -o "$BUILD_DIR/inspect_glb"

# Run
CELL="${2:-}"
echo ""
if [[ -n "$CELL" ]]; then
    "$BUILD_DIR/inspect_glb" "$GLB" "$CELL"
else
    "$BUILD_DIR/inspect_glb" "$GLB"
fi
