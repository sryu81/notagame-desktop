# scripts/host_build

Host-build verification recipes for engine self-tests. Lets you run a
module's `selfTest()` on the host with `g++` instead of rebuilding the
full Android APK (faster iteration, no emulator needed).

## Layout

- `stubs/` — iostream-backed replacements for Android NDK headers.
  - `AndroidOut.h` overrides `engine/AndroidOut.h` (writes to `std::cout`
    instead of logcat). Staged LAST so it wins the include race.
  - `AndroidOut.cpp` defines `std::ostream aout`.
  - `asset_manager.h` no-op stubs for `AAssetManager_open` etc.
- `run_selftest.sh <FileStem> [extra_dep.cpp ...]` — runs `<ns>::selfTest()`.
- `host_main_<FileStem>.cpp` — driver for each module (see
  `host_main_GltfLoader.cpp` for the template).
- `verify_gltf_load.sh` — for `GltfLoader` only: generates a minimal
  rigged .glb (1 quad, 1 bone, 1 anim channel), loads it, and asserts
  that `sampleSkeletal(clip, 0.5, 1)` produces `bone[0].T = (0.25, 0, 0)`.
- `make_rigged_glb.py` — used by `verify_gltf_load.sh`.
- `load_real_glb.cpp` — used by `verify_gltf_load.sh`.

## Quick start

```bash
# Run gltf::selfTest() — null/garbage/minimal GLB rejection cases
./scripts/host_build/run_selftest.sh GltfLoader Math.cpp Animation.cpp

# End-to-end: build a real .glb, load it, verify anim interp
./scripts/host_build/verify_gltf_load.sh
```

`run_selftest.sh` outputs to `/tmp/opencode/hostbuild/` by default. Override
with `BUILD_DIR=/your/path ./run_selftest.sh ...`.

## Adding a new module

1. Create `host_main_<FileStem>.cpp` next to this README. It should
   `#include "<FileStem>.h"` and call `<namespace>::selfTest()`.
2. Run `./run_selftest.sh <FileStem> [extra_dep.cpp ...]`.
3. List any extra `.cpp` deps the module needs (e.g. `GltfLoader`
   needs `Math.cpp` + `Animation.cpp`).

## Why this matters

Each slice ships with a `selfTest()` (per project conventions). Most
failures are CPU-only logic (mesh parsing, animation sampling, grid
pathfinding, etc.) and don't need GL. The host build reproduces them
in ~1 s on a desktop instead of ~2 min through Gradle.

## Limitations

The host build is for **CPU-only** self-tests. Anything that touches
`Renderer`, GL, Android `android_app*`, `android/asset_manager.h` real
APKs, or the touch/motion pipeline must still be tested on device.
