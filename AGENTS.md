# AGENTS.md — Not_A_Game (desktop)

Desktop (Linux) fork of the Android SRPG engine testbed at `/home/soo/notagame`.
Purpose: iterate on the engine and demo faster than on the emulator (60 FPS
vs ~10), then **merge the upgraded engine back** into the Android repo.

Active work: **[DEMO_PLAN.md](DEMO_PLAN.md)** — hub + lab scenes, bug fixes B1–B8.
Background docs (SLICES, VERIFICATION, TUNABLES, HISTORY) live in the Android repo.

---

## Build & run

Deps (Ubuntu 24.04): `sudo apt install cmake ninja-build libglfw3-dev libgles-dev libegl-dev`

```sh
cmake -S . -B build -G Ninja          # once (RelWithDebInfo default)
cmake --build build                   # incremental; new .cpp files picked up by glob
./build/not_a_game                    # runs all selfTests, then opens the window
./build/not_a_game --data ./save      # save dir (default ~/.local/share/notagame)
./build/not_a_game --assets ./assets  # asset dir (default <exe dir>/assets symlink)
scripts/rebuild_run.sh                # rm -rf build, configure, build, launch
scripts/rebuild_run.sh --asan -- --data ./save   # same in build-asan; args after -- go to game
```

Sanitizers: `cmake -S . -B build-asan -G Ninja -DNAG_SANITIZE=ON && cmake --build build-asan`

- Asserts are forced on in all build types (`-UNDEBUG`) — selfTests depend on them.
- A failing selfTest aborts at startup with the assert line on stderr.
- Logs (`aout`) go to stderr, prefixed `AO:`.
- Controls: left click/drag = tap/pan, wheel = zoom, Esc = quit.

Host-only selfTests (no GL) still work: `scripts/host_build/run_selftest.sh <Stem>`
(scripts point at old Android paths — fix as needed).

---

## Layout

```
src/engine/            engine (same files as notagame engine-module/src/main/cpp/engine)
  Platform.h           NEW: platform seam (asset read, data path, fb size, swap, quit)
  Audio_miniaudio.cpp  NEW: replaces Audio_oboe.cpp
src/game/              game (same as notagame app/src/main/cpp/game)
src/desktop/main.cpp   GLFW window, GLES 3.0 ctx via EGL, input → Renderer, selfTest list
assets/                same as notagame app/src/main/assets
third_party/           stb_image.h (v2.30), miniaudio.h (v0.11.25)
tools/gen_font.py      generates src/engine/FontData.h
art/                   source art, not shipped (robot1.blend, robot1.glb)
```

Game scenes: `TitleScene` → `HubScene` (lab list + BATTLE) → `BattleScene`.
Battle is split into `BattleData.h` (shared `battle::Data`),
`BattleContent` (assets/level), `BattleRules` (paths, attacks, pick, save),
`BattleInput` (taps), `BattleHUD` (DebugPanel menus), `BattleScene` (lifecycle).
Lab helpers: `DebugPanel` (row list), `LabChecklist` (green-on-pass rows),
`LabAssets` (lit/skinned/unlit shaders + materials, plane/cube meshes).

Conventions are the Android repo's (see its AGENTS.md): no new comments,
`math::` column-major, every module keeps its `selfTest()`, `engine/` never
includes `game/`.

GL target stays **GLES 3.0** (`#version 300 es`, `<GLES3/gl3.h>`) so shaders
and GL code stay identical to Android. Don't use desktop-only GL.

---

## Merge back to Android (notagame)

Forked from `notagame@9f7916d` + its uncommitted WIP (BattleScene unit removal,
rain spawn in Weather.cpp) as of 2026-09-26.

Platform-specific (do NOT copy verbatim; Android needs its own impl):

| Desktop | Android equivalent needed |
|---|---|
| `Platform.h` | copy as-is; add `AndroidPlatform` (AAssetManager read, internalDataPath, EGL swap/size, `GameActivity_finish`) |
| `Renderer(Platform&)`, `pointerDown/Move/Up`, `scroll` | Android `main.cpp` must translate motion events → these calls; pinch → `zoomBy` (pinch state was removed from Renderer) |
| `TextureAsset` (stb_image) | either vendor stb_image on Android too, or keep AImageDecoder behind `Platform` bytes |
| `Audio_miniaudio.cpp` | keep `Audio_oboe.cpp`, port to `Audio::init(const Platform*)` |
| `AndroidOut.cpp` (stderr) | restore `__android_log_print` under `#ifdef __ANDROID__` |
| `src/desktop/main.cpp` | stays desktop-only; sync selfTest list with Android `main.cpp` |

Everything else in `src/engine` and `src/game` is portable — diff and copy.

Fixes made here that upstream lacks (keep this list current):
- `SceneManager::selfTest` use-after-free (read counters of deleted scene).
- DEMO_PLAN B1–B8: per-draw blend state (particles/stars/flash), particle IBO
  sized to buffer capacity, attack event+timer share one hit flag, skinned
  shadow shader (`shadow_skinned.vert`) + full-hierarchy shadow mirror sync,
  emitter keeps emit alpha/size (frame-rate independent shrink), bones use
  glTF rest TRS (`Bone::restLocal`), default roster restores to unit max HP,
  `Shader::setBoneMatrices` clamps to `kMaxBones`.
- Battle assets go through `game::LabAssets`, which calls `releaseGL()` on
  exit (upstream leaked the registry VBO/IBO on every battle enter/RESET).
- Font: punctuation glyphs (`. : % = < > / ( ) , ? * ' _`) and lowercase →
  uppercase shapes. Regenerate with `python3 tools/gen_font.py > src/engine/FontData.h`
  (`tools/gen_font.py` is copied from upstream and extended).

Easiest merge path: in notagame, add the same `Platform` seam first, then
`diff -r src/engine ../notagame/engine-module/src/main/cpp/engine`.

---

## Session handoff

| Commit | Description |
|---|---|
| `d26319f` | import engine, game, assets from notagame@9f7916d |
| `fd221c3` | desktop port (GLFW + GLES3/EGL, stb_image, miniaudio) |
| `38b93fc` | carry over notagame WIP + DEMO_PLAN.md + this file |

Verified: 47 selfTests PASS; title + battle render at 60 FPS on Mesa radeonsi
(GLES 3.2); PulseAudio via miniaudio; level + 3 .glb load.

Done: P0 B1–B8, P1 (hub, DebugPanel, LabChecklist, battle split, LabAssets).
Next: P2 labs in DEMO_PLAN §6 order, starting with FxLab.
