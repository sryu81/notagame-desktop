#!/usr/bin/env bash
# Host-build verification of a selfTest() that lives in a single .cpp.
# Stubs out Android NDK headers; compiles with g++ on Linux; runs the binary.
#
# Usage:
#   ./run_selftest.sh <FileStem> [extra_dep.cpp ...]
# Examples:
#   ./run_selftest.sh GltfLoader
#   ./run_selftest.sh Math
#   ./run_selftest.sh EventBus
#
# The module's source is expected at:
#   app/src/main/cpp/engine/<FileStem>.cpp
#   app/src/main/cpp/engine/<FileStem>.h   (optional, copied if present)
# A host_main_<FileStem>.cpp next to this script must include the module
# header and invoke <namespace>::selfTest(). See host_main_GltfLoader.cpp
# for the template.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$HERE/../.." && pwd)"
ENGINE_DIR="$PROJECT_ROOT/engine-module/src/main/cpp/engine"
BUILD_DIR="${BUILD_DIR:-/tmp/opencode/hostbuild}"

STEM="${1:?usage: $0 <FileStem> [extra_dep.cpp ...]}"
shift
EXTRA_DEPS=("$@")

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/engine"
STAGE="$BUILD_DIR/engine"

# 1. Copy the module's source + all real engine + game headers first
GAME_DIR="$PROJECT_ROOT/app/src/main/cpp/game"
if [[ -f "$ENGINE_DIR/$STEM.cpp" ]]; then
    cp -f "$ENGINE_DIR/$STEM.cpp" "$STAGE/$STEM.cpp"
elif [[ -f "$GAME_DIR/$STEM.cpp" ]]; then
    cp -f "$GAME_DIR/$STEM.cpp" "$STAGE/$STEM.cpp"
fi
[[ -f "$ENGINE_DIR/$STEM.h" ]] && cp -f "$ENGINE_DIR/$STEM.h" "$STAGE/$STEM.h"
[[ -f "$GAME_DIR/$STEM.h" ]] && cp -f "$GAME_DIR/$STEM.h" "$STAGE/$STEM.h"
cp -f "$ENGINE_DIR"/*.h "$STAGE/" 2>/dev/null || true
[[ -d "$GAME_DIR" ]] && cp -f "$GAME_DIR"/*.h "$STAGE/" 2>/dev/null || true

# 2. Stage extra deps (e.g. Math.cpp when testing GltfLoader)
for dep in "${EXTRA_DEPS[@]}"; do
    if [[ "$dep" == *".cpp" ]]; then
        base="$(basename "$dep")"
        if [[ -f "$ENGINE_DIR/$base" ]]; then
            cp -f "$ENGINE_DIR/$base" "$STAGE/$base"
        elif [[ -f "$GAME_DIR/$base" ]]; then
            cp -f "$GAME_DIR/$base" "$STAGE/$base"
        fi
    fi
done

# 3. Stage stubs: AndroidOut.h (override), asset_manager.h (android/ path), AndroidOut.cpp
cp -f "$HERE/stubs/AndroidOut.h"   "$STAGE/AndroidOut.h"
cp -f "$HERE/stubs/AndroidOut.cpp" "$BUILD_DIR/AndroidOut.cpp"
mkdir -p "$BUILD_DIR/android"
cp -f "$HERE/stubs/asset_manager.h" "$BUILD_DIR/android/asset_manager.h"

# 4. Pick the host_main (host_main_<stem>.cpp next to this script)
HOST_MAIN="$HERE/host_main_${STEM}.cpp"
if [[ ! -f "$HOST_MAIN" ]]; then
    echo "ERROR: $HOST_MAIN not found"
    echo "Create one that calls <module-namespace>::selfTest() (see template)."
    exit 2
fi

# 5. Compile
echo "[host-build] staging: $STAGE"
echo "[host-build] module: $STEM"
echo "[host-build] extras: ${EXTRA_DEPS[*]:-<none>}"
g++ -std=c++17 -O0 -g \
    -I "$BUILD_DIR" \
    -I "$STAGE" \
    "$STAGE/$STEM.cpp" \
    "${EXTRA_DEPS[@]/#/$STAGE/}" \
    "$BUILD_DIR/AndroidOut.cpp" \
    "$HOST_MAIN" \
    -o "$BUILD_DIR/${STEM}_host"

# 6. Run
echo "[host-build] running $BUILD_DIR/${STEM}_host"
"$BUILD_DIR/${STEM}_host"
