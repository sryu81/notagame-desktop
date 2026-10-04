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
./build/not_a_game --start fxlab      # start scene: title hub levels battle autobattle renderlab animlab fxlab cameralab systemslab unitlab tacticslab levellab ailab uilab picklab modellab
./build/not_a_game --script f.txt     # scripted input: "<frame> tap|down|move|up|hover x y", "key code", "shot f.png", "quit"
scripts/verify/run_labs.sh [out]      # ASan build, every lab auto-drive, fails on red check or sanitizer report
```

Verification: run `scripts/verify/run_labs.sh` after engine/game changes. It
drives `scripts/verify/labs.txt` from the hub (framebuffer coordinates at
1280×720), checks each lab logs `auto-drive done: N/N`, battle loads, and no
ASan/UBSan report; screenshots and the log land in `build-asan/verify/`.
Screenshots are read from the back buffer in `DesktopPlatform::swapBuffers`.

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
tools/gen_checker.py   generates assets/checker.png (RenderLab texture check)
tools/gen_sfx.py       generates select/move/hit/miss/turn/music2.wav (48 kHz mono)
tools/gen_music.py     generates the background playlist music_01..03.wav (numpy)
tools/gen_test_rig.py  generates assets/test_rig.glb (15-joint box humanoid)
tools/gen_units.py     Blender (headless) → assets/{knight,horse,knight_mounted,archer,goblin,mage}.glb,
                       prop_{crate,rock,tree}.glb, knight_palette.png, art/units.blend
tools/unitgen/         generator package: geo (tubes, rings, domes, envelope weights), sd (SD
                       characters, 2.5 heads, ~10k tris), clips, props, palette, blender_io
art/                   source art, not shipped (robot1.blend, robot1.glb)
```

Game scenes: `TitleScene` → `HubScene` (lab list + BATTLE) →
`LevelSelectScene` (level, PLAY/AUTO) → `BattleScene(gs, levelId, auto)` →
`ResultScene` → Hub. Battle is split into `BattleData.h` (shared
`battle::Data`), `BattleContent` (level, rig, catalog, spawn, terrain,
camera), `BattleTurns` (turn loop, AI, player actions, battle checklist,
result, saves, picking), `BattleInput` (taps), `BattleHUD` (game UI +
SPEED/AUTO/SAVE/LOAD/BACK), `BattleScene` (lifecycle, environment).
Labs: `FxLab` (particles/sky/weather,
damage number, status icon, trail, 15 checks), `CameraLab` (orbit/limits/
ortho/forward/follow/pick/input, frame attack, pan clamp, portrait fit,
14 checks), `RenderLab` (texture, primitives,
skinning, shadows, depthWrite, bounds, bone clamp, 9 checks + opt-in heavy
mesh), `SystemsLab` (save slots, Saveable, Bag/Equip/Catalog, custom
calendar, SFX voices, music switch, volume buses, turn flow, win/lose,
result scene, mid-battle save, roster by id, 15 checks), `AnimLab`
(test rig clips, loop flags, B6, events, playback rate, walk during move,
foot IK on a ramp, 14 checks), `UnitLab` (skinned catalog units, facing,
base scale, walk in move, attack→hit→hurt, death, cube-rig fallback,
8 checks), `TacticsLab` (six rule scenarios: reach, occupancy, elevation,
sight, cover/damage, ranged + stun; design toggles; 9 checks), `LevelLab`
(.lvl v3 round trip, v2 compat, units by catalog, props, terrain vs
cellY, win condition; 6 checks), `AILab` (20 seeded AI-vs-AI battles:
legality, approach, attack in range, termination, determinism incl. a
live replay; metrics; 5 checks), `UILab` (legal-action menu, unit card,
turn-order bar, portrait layout; 4 checks), `PickLab` (body over cell,
cell pick, tolerance, corpse, intent, cycle, drag-no-tap, hover;
8 checks), `ModelLab` (every unit type + crate/rock/tree from
`game::UnitModels`: own model, five clips, bones <= 48, fits one cell,
palette texture, props modeled, all clips play; CLIP/TURN controls;
7 checks). Each auto-drives its checklist; game helpers
`ClipEvents` (events by clip name) and `FootIKRig` (skinned foot IK).
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
| `Audio_miniaudio.cpp` | keep `Audio_oboe.cpp`, port to `Audio::init(const Platform*)`; also implement `isOpen`, `activeVoiceCount`, `currentMusic`, `outputPeak` (store `audio::peakAbs` of each mixed buffer) |
| `AndroidOut.cpp` | shared; `__android_log_print` under `#ifdef __ANDROID__` (done) |
| `src/desktop/main.cpp`, `ScriptRunner`, `PngWriter` | stay desktop-only; sync selfTest list with Android `main.cpp` |

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
- `assets/stars.bin` regenerated: `make_stars.py` read HYG RA (hours) as
  degrees, squeezing all stars into a 24° wedge that was never on screen.
  Script now writes `assets/stars.bin` (or argv[1]).
- Battle rain uses `WeatherPalette.rainColor` (was hard-coded).
- `Camera::setForward` now rotates the orbit (was stored but ignored) and
  rebuilds eye/view; selfTest added.
- `BattleScene::exit` clears the camera terrain callback (it captured the
  freed `BattleGrid*`; any pan after leaving battle was a use-after-free).
- Renderer clears the shadow map once when a scene has no shadow root
  (lit surfaces sampled an unwritten or previous scene's map).
- `GameState` and `EntityRegistry` derive from `engine::Saveable`.
- UBSan clean: lightning RNG uses `uint32_t` (was signed overflow);
  `SceneManager::selfTest` no longer binds a reference to a null `Renderer`.
- `engine::foot_ik::solveTwoBone` rewritten: it bent around the wrong axis
  (a no-op for a straight leg) and echoed the target as `solvedFootPos`;
  selfTests now check forward kinematics.
- glTF loader: joint names (`Bone::name`, `Skeleton::find`), clip loop flag
  from the name (`anim::isLoopClipName`), joints sorted parent-first with
  vertex/track indices remapped (`gltf::sortJointsParentFirst`).
- Skeletal clips carry `events` and fire them like rigid clips
  (`anim::crossedEvents`); `AttackAction` takes the skeletal hit time from
  event id 1 (hard-coded 0.15 s removed; battle's code-built clip got the
  event). Event tracking resets when a node's clip changes.
- `SceneNode::setPoseModifier` (between sampling and bone matrices);
  `MoveAction::setWalkClip`.
- Shadow ortho box centres on the camera target (was fixed at the origin);
  `setShadowExtent`, `lightViewProjection()`, `shadowStats()` added.
- Units: `UnitDef` carries a skinned mesh/model/material + `modelYawDeg` +
  `baseScale`; `unit::spawn` builds a skinned child (`Unit::visualNode()`)
  when it has a skeleton and mesh, else the cube rig. `MoveAction` uses
  `Unit::baseScale()` (hard-coded 0.56 removed), plays `ClipSet::walk`
  itself and swings cube-rig legs only on cube rigs. `AttackAction` plays
  `ClipSet::attack`/`hurt` via the visual node; the timer hit path now plays
  the reaction too (was event path only). Death plays `ClipSet::death`,
  deselects, and `Unit::pickable()` is false. `unit::damage` = max(1, atk − def).
- `game/Rules`: `reachable`/`pathTo` (mov, walls, step limit, allies pass
  but can't stop, enemies block, optional diagonals), symmetric
  elevation-aware `lineOfSight` (replaces `BattleGrid::hasLineOfSight`),
  one `damage` formula (atk − def, high-ground bonus, cover %, variance),
  `canAttack`. `RangedAttackAction` (projectile node, hit on arrival);
  attacks take an optional on-hit status (melee default stays Poisoned);
  `turn::Manager` skips stunned units; `SceneNode::removeChild`.
  Hub rows shrink to fit the screen (update the tap rows in
  `scripts/verify/labs.txt` when the hub list changes).
- Levels: `.lvl` v3 (`level::save`, win condition, catalogId/unitId, props
  by cell; v2 still loads), `scripts/make_level.py` writes `level_01..03`
  (flat, hill, fort), `game/LevelSpawner` (grid, units by catalog, props).
  `primitives::makeCellTerrain` builds the terrain from `cellY`; battle
  draws it again and its default landscape is the level's own elevation.
  `EntityBuilder::spawnProp` keeps the prop scale when placing on a grid.
- `game/AI`: `ai::planTurn` (attack / move-to-attack / approach, seeded
  tie-breaks), `ai::simulate` headless seeded battles with per-turn
  legality records, `ai::turnLog` shared by sim and live replay.
- `game/BattleFlow` (win/lose per condition, idle-driven turn end,
  `BattleSnapshot` Saveable battle save replacing the fixed-struct
  `battle.sav`), `ResultScene`, `Unit::unitId` (saved; `Unit::load` goes
  through `setCell` so the grid stays in sync), roster keyed by id,
  `turn::Manager` rounds/untimed mode.
- Camera: `setTargetBounds`, `inView`, `frame`; `pan` horizontal scale
  fixed (was `aspect`× too fast).
- `game/BattleFx`: floating damage numbers, status icons, projectile trail.
- `game/GameUI` (`gui::`): responsive `Layout`, `legalActions`,
  `ActionMenu`, `UnitCard`, `TurnOrderBar`.
- P3 battle: data-driven from `.lvl` v3 + catalog (skinned `test_rig.glb`
  units), `game/Rules` + `game/AI` + `BattleFlow` turn loop, AUTO mode
  (AI vs AI to `ResultScene`), battle checklist (6 rows), per-level
  `Saveable` save. Upstream's `BattleRules.cpp`, fixed-struct save, LAND
  presets and tuning panels are removed. `GameState::restoreTo` brings
  fallen units back at 1 HP.
- `SceneNode::Drawable::castShadow` (default true); the shadow pass skips
  drawables with it off. Grid tiles, terrain mesh and lab ground planes
  only receive shadows.
- Background music is a playlist: `Audio::playPlaylist` (mixer advances
  to the next track at the end and wraps); `Renderer` loads
  `music_01.wav`.. and plays them at 0.3 (falls back to `music.wav`).
- Picking: `Renderer::pointerUp` emits `TapEvent` only when the pointer
  did not drag past the pan threshold; `Renderer::hover` → `HoverEvent`
  (desktop cursor with no button down). `game/Picker` (`pick::`): body
  capsule in screen space (nearest to camera) → unit on the cell under the
  tap (`pick::cellAt`, terrain heights) → closest body within 28 px;
  optional intent filter; repeated taps on one spot cycle overlapping
  units. Battle picks on `TapEvent` (attack mode prefers highlighted
  targets), shows a hover ring and the hovered unit's card.
- `game/UnitTypes`: single unit stat table (battle, LevelLab, AI roster).
  `ai::planTurn` optional goal cell (reach-cell levels); battles cap at 30
  rounds.
- Per-type unit models: `UnitType::model` names a glb (`knight.glb`,
  `knight_mounted.glb` for the new CAVALRY type, `archer.glb`, `goblin.glb`,
  `mage.glb`; nullptr → `test_rig.glb`, only SLINGER now).
  Battle loads one rig per glb (`Data::rigs`); palette-textured units use
  the palette material (skinned shader + `knight_palette.png`) with a white
  tint. Levels 1–2 swap the ally MAGE for CAVALRY. Loading lives in
  `game/UnitModels` (rigs per glb, palette materials, catalog at
  `kUnitScale` 0.5, prop meshes), shared by battle and ModelLab; the other
  labs keep test_rig and their own scales. Props: `LevelSpawner::Visuals`
  takes per-type prop meshes (box fallback when missing).
- Font: punctuation glyphs (`. : % = < > / ( ) , ? * ' _`) and lowercase →
  uppercase shapes. Regenerate with `python3 tools/gen_font.py > src/engine/FontData.h`
  (`tools/gen_font.py` is copied from upstream and extended).

Easiest merge path: in notagame, add the same `Platform` seam first, then
`diff -r src/engine ../notagame/engine-module/src/main/cpp/engine`.

Merged 2026-10-03 at desktop `99c77b3` → notagame branch `desktop-merge`
(`a662017`, not on its `main`; its pre-merge WIP is `stash@{0}` there).
After that, `src/engine` and the Android engine differ only in
`Audio_miniaudio.cpp` vs `Audio_oboe.cpp`; `AndroidOut.cpp` is shared
(`__ANDROID__` → logcat). Android build without network:
`$ANDROID_HOME/cmake/3.22.1/bin/ninja -C app/.cxx/Debug/<hash>/x86_64`;
APK: `./gradlew assembleDebug`; emulator: `Pixel_8` AVD (headless
`-no-window` works; screencap via `adb exec-out screencap -p`).

---

## Session handoff

| Commit | Description |
|---|---|
| `d26319f` | import engine, game, assets from notagame@9f7916d |
| `fd221c3` | desktop port (GLFW + GLES3/EGL, stb_image, miniaudio) |
| `38b93fc` | carry over notagame WIP + DEMO_PLAN.md + this file |

Verified: 47 selfTests PASS; title + battle render at 60 FPS on Mesa radeonsi
(GLES 3.2); PulseAudio via miniaudio; level + 3 .glb load.

Done: P0 B1–B8, P1 (hub, DebugPanel, LabChecklist, battle split, LabAssets),
P2.1–P2.5 labs, H1 scripted harness, P2b.1 UnitLab, P2b.2 TacticsLab,
P2b.3 AILab, P2b.4 LevelLab, P2b.5 F1/C1/X1, P2b.6 UILab, P3 battle.
Merged to Android (`desktop-merge` branch, emulator-verified).
Next: review/merge `desktop-merge` into notagame `main`; P4 engine gaps
as the battle needs them (DEMO_PLAN §3 P4).
