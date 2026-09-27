# DEMO_PLAN.md — Reshape the demo to exercise the engine

Status: **proposal** (2026-09-23). Source: full engine + game API audit.

> Moved from the Android repo (`notagame`) on 2026-09-26; this desktop repo is
> now where the plan is executed. Paths are updated to the desktop layout
> (`src/engine`, `src/game`, `src/desktop/main.cpp`, `assets/`). File:line
> references in `Renderer.cpp` predate the port and are approximate — search
> by symbol. "logcat" means stderr here.

## 1. Where we are

The engine surface is broad. The demo is one 2 000-line `BattleScene.cpp`
built around a single `Data` object. It exercises only part of the engine:

- **Live and visible:** clock, sky, sun, stars, weather, lightning, rain,
  camera (pan/zoom/FOV/pitch presets), UI buttons, audio volume buses,
  hit-burst particles, heightmap elevation.
- **Wired but dormant:** now that units are removed, these never run:
  turn loop, move/attack actions, picking of units, rigid and skeletal clips,
  anim events, camera follow, shadows (no casters), save (returns early on
  an empty registry), GameState roster.
- **Loaded but never drawn:** `robot1.glb` (36 MB, parsed on every enter
  and RESET), `giant.glb`, the `unit_v2.glb` mesh, `skinMat`, `texMat`,
  `cubeMesh`, the heightmap terrain mesh (attach call is commented out).
- **Never called:** props (`PropFactory`, `buildMeshNode`), the `.lvl`
  unit/prop records, the `.lvl` elevation (overwritten by the heightmap),
  `Saveable`/`save::writeAll`, `Bag`/`EquipSlots` mutation, FootIK,
  `buildSkinnedNode`, `spawn<T>`, CPU `skinMesh`, `MeshDataParser`,
  camera `orbit`/yaw/ortho/limits, input other than pointer-down, UI
  `setVisible`/`setEnabled`/text color, sky and star knobs beyond four,
  particle jitter.

### Bugs found by the audit (fix before any evaluation)

| # | Bug | Where |
|---|---|---|
| B1 | `UIRoot::render` ends with `glDisable(GL_BLEND)`. Blend is enabled only once at init, so from frame 2 particles, stars and the lightning flash render opaque. | `UIRoot.cpp:187`, `Renderer.cpp:395` |
| B2 | The particle IBO is filled for `count` particles but rebuilt only when the VBO grows. A later frame with a larger count reads past the IBO. Rain ramps up, so it triggers this. | `Renderer.cpp:174-195` |
| B3 | A rigid-clip attack deals damage twice. The event lambda applies damage but never sets `hitDelivered_`, so the timer path fires again. | `Action.cpp:252-280` |
| B4 | `shadow.vert` has no skinning, so skinned units cast bind-pose shadows. The shadow mirror syncs only the root `setLocal`, so animated child parts don't move their shadows. | `shadow.vert`, `EntityBuilder::createShadowMirror`, `BattleScene.cpp:~1945` |
| B5 | `Emitter::update` overwrites alpha with `1-t`, so the emit alpha (rain 0.5) is lost. `sizeShrinkFactor` is applied per frame, so shrink depends on frame rate. | `ParticleSystem.cpp` |
| B6 | Bones with no track sample to identity, not to their glTF rest TRS. | `Animation.cpp` `sampleSkeletal`, `GltfLoader.cpp` |
| B7 | Default roster hp=10 would clamp the 15-hp Knight on first `restoreTo`. | `src/desktop/main.cpp` (`initDefaultRoster`), `GameState.cpp` |
| B8 | `Shader::setBoneMatrices` never clamps `count` to `kMaxBones` (48). | `Shader.cpp` |

## 2. Principle

**One engine feature → one place where you can see it pass.**

Split the demo into a **hub** plus small **lab scenes**, one per subsystem.
Keep one **data-driven battle scene** as the integration test.

- Each lab is about 200–400 lines and uses the engine only through its
  public API. No lab reaches into another lab's state.
- Each lab shows an **on-screen checklist**: rows turn green when their
  condition is observed at runtime (e.g. "bone matrices uploaded",
  "shadow casters > 0", "anim event fired"). Evaluation then means looking
  at a screen, not reading logcat.
- The three-plus scenes also cover F3 (scene lifecycle) and F11 (music per
  scene).

```
TitleScene ──► HubScene ──┬─► RenderLab     materials, texture, skinning, shadows, lights
                          ├─► AnimLab       rigid clips, skeletal, events, rate, FootIK
                          ├─► FxLab         particles, rain, sky/stars/weather knobs, flash
                          ├─► CameraLab     orbit/yaw/pitch limits, ortho, follow, pan, pick
                          ├─► SystemsLab    save slots, Saveable, Bag/EquipSlots, Catalog, Clock calendar, audio SFX
                          └─► BattleScene   level-driven SRPG slice (integration)
                                   └─► ResultScene (victory/defeat) ──► Hub
```

## 3. Phases

### P0 — Hygiene and bug fixes (small, do first)

Status: done 2026-09-26. robot1.blend/.glb moved to `art/` (not shipped);
`make_level.py` writes `assets/level_01.lvl` by default (output identical);
`level::selfTest` runs at startup; battle enter/RESET ≈ 2–3 ms.

1. Fix B1–B8, adding a selfTest where host-testable (B2, B3, B5, B6, B8).
2. Move `robot1.blend` out of `assets/` (it shipped in the APK upstream).
3. Stop loading `robot1.glb` by default. Keep it as an opt-in "heavy mesh"
   stress row in RenderLab, or replace it with a decimated version (≤15K tris).
4. `make_level.py`: fix the hard-coded output path
   (`/home/soo/AndroidStudioProjects/...`) — output to `assets/`.
5. Call `level::selfTest` from `src/desktop/main.cpp`.

**Done when:** the rain in Storm is translucent and the flash is additive,
no attack double-hits, and battle enter/RESET takes under 1 s.

### P1 — Scaffolding

Status: done 2026-09-26. Battle HUD uses DebugPanel menus (one panel
open at a time); robot1.glb, giant.glb and the texture material are no
longer loaded by the battle scene.

1. **`HubScene`**: a button list of labs plus the battle scene. Replaces
   "tap anywhere" in TitleScene. BACK in every scene returns to Hub.
2. **`DebugPanel` helper** (game side): a declarative row list
   `{label, getter, setter/cycle}` laid out automatically, with panels
   toggled by `setVisible`. It replaces hand-laid-out buttons with
   duplicate labels ("RESET"×2, "DAWN"×2). It exercises UI `setVisible`,
   `setEnabled` and `UIText::setColor`.
3. **`LabChecklist` helper**: a named check list rendered in the corner,
   with `check("name", bool)` called each frame.
4. **Split `BattleScene.cpp`** into `BattleContent` (assets/level),
   `BattleInput`, `BattleHUD` and `BattleRules`. Drop the dead `Data`
   members (leg-rig IK state, unused clip templates, giant/unitV2 handles).
5. Shared `LabAssets`: shaders and materials loaded once per scene through
   `AssetRegistry`, using `find*` and `releaseGL` on exit (both unused today).

### P2 — Labs

Status: P2.1 FxLab, P2.2 CameraLab, P2.3 RenderLab done; P2.4–P2.5 open.

Every lab follows the same shape:

- Files `src/game/<Name>Lab.{h,cpp}`, 200–400 lines, one `Scene` subclass.
  Hub entry in `HubScene::entries` switches from `nullptr` to a factory.
- Uses `LabAssets` for shaders/materials, `DebugPanel` for knobs (left),
  `LabChecklist` for checks (top-left), BACK row returns to Hub.
- **Auto-drive:** a scripted timeline exercises every row with no input, so
  entering the lab and waiting ~30 s turns every row green. The DebugPanel
  is for manual poking on top of that.
- A check turns green only on an observed runtime value (a number compared
  against an expected value), never on "the code path ran".
- Engine changes land with a host selfTest; the lab only proves them on GPU.

**Done when (per lab):** hub entry enabled, all rows green in one
auto-drive run, ASan run clean, 49+ selfTests still pass.

#### P2.1 FxLab — particles, sky, weather (no engine changes)

Status: done 2026-09-26 — `src/game/FxLab.{h,cpp}`, 12/12 green in a 28 s
auto-drive under ASan. Found and fixed: `stars.bin` held RA in hours read
as degrees, so every star sat in a 24° wedge near +X and none were ever on
screen (`scripts/make_stars.py` fixed, asset regenerated). Camera sits low
(eye y=2, looking up) so sky, stars and rain fill the top half.

Scene: flat 20×20 grid, camera fixed at ISO, clock at 12:00.

| Row | Observed condition |
|---|---|
| `burst alpha fades` | a burst particle's alpha at t=0.5·life is 0.5·emit alpha (±0.05) (B5) |
| `rain keeps alpha` | Storm rain particle alpha ≤ 0.5 at spawn (B5) |
| `particle ibo grows` | rain `activeCount` passes 512 → 1024 with no GL error (B2) |
| `gravity knob` | burst velocity.y after 0.5 s changes sign with GRAV 0 vs −8 |
| `shrink fps-indep.` | size at t=0.5·life matches formula at 30 and 60 FPS dt |
| `rain color` | rain particle rgb == `WeatherPalette.rainColor` |
| `sky knobs` | each of the 8 `SkyConfig` fields is stored by its `Renderer` setter and changes `SunState` (`skyColor`, `ambient`, `ambientColor` or `starIntensity`; `ambientColor` is not rendered until P4) |
| `storm mix` | `stormSkyMix` 0 vs 1 changes skyColor at noon |
| `stars at night` | at 00:00 `starIntensity > 0` and ≥ 10 stars project inside the view (CPU copy of the `stars.vert` rotation); `starIntensity == 0` at 12:00 |
| `star knobs` | `StarRenderConfig.pointSize`/`brightnessScale` change → reapplied |
| `lightning` | ≥1 flash within `baseDelay + jitter`, flash decays to 0 |
| `blend state` | `glIsEnabled(GL_BLEND)` false after UI render (B1 regression) |

Auto-drive: Clear → Rain → Storm (10 s each), time-lapse 00:00 → 24:00 at
2 h/s, cycle each sky knob once.

#### P2.2 CameraLab — camera and input

Status: done 2026-09-26 — `src/game/CameraLab.{h,cpp}`, 11/11 green in a
7.4 s auto-drive under ASan. Input rows are auto-driven through the same
entry points the platform layer calls (`Renderer::pointerDown/Move/Up`,
`EventBus::emit<KeyDownEvent>`); taps pick/highlight cubes, keys 1–5 pick
presets (GLFW key codes; Android needs its own mapping). Engine fixes it
forced: `Camera::setForward` was a no-op (orbit offset ignored
`forward_`); battle's camera terrain callback outlived its grid
(heap-use-after-free on any later pan); scenes without a shadow root
sampled a stale/unwritten shadow map (everything ambient-only).

Scene: grid + 5 cubes at known cells as pick targets.

| Row | Observed condition |
|---|---|
| `orbit yaw` | `orbit(45, 0)` changes eye yaw by 45° (±0.5°) |
| `pitch limits` | `setPitchLimits(20, 60)` then orbit past → pitch clamped to 60 |
| `yaw limits` | same for yaw ±45° |
| `ortho toggle` | `setOrthographic` → projection m[15] == 1 |
| `set forward` | `setForward(+X)` → view forward ≈ +X |
| `follow target` | target tracks a moving cube within 1 frame |
| `pick hit` | `pickAt` at a cube's projected center returns that cube |
| `pick miss` | `pickAt` on empty sky → invalid |
| `pointer move` | ≥1 `PointerMoveEvent` during a drag and the pan moved the target > 0.1 |
| `pointer up` | `PointerUpEvent` after down |
| `key down` | `KeyDownEvent` for key 3 selects TOP (pitch 70° ±0.5°) |


#### P2.3 RenderLab — draw path

Status: done 2026-09-26 — `src/game/RenderLab.{h,cpp}`, 9/9 green in a
2.2 s auto-drive under ASan; R1 landed (`setShadowExtent`,
`lightViewProjection()`, `shadowStats()`). Pixel-read rows: the depth probe
reads a cube behind an occluder with `depthWrite` on (control, occluder
colour) and off (cube colour); the texture row reads 9 pixels across the
top face of a cube textured with `assets/checker.png`
(`tools/gen_checker.py`) and needs ≥ 4 light/dark alternations
(`android_robot.png` is flat green with an alpha mask, so it can't show
sampling). `test_rig.glb` not used yet (P2.5). Heavy mesh: opt-in button;
needs `robot1.glb` copied from `art/` into `assets/`.

Scene: plane + textured cube + UV sphere + `unit_v2` skinned + `test_rig`
(from P2.5 asset, optional) under a rotating sun.

Engine change **R1**: `Renderer::computeLightVP` centres the ortho box on
`camera().target()` with a configurable half-size
(`setShadowExtent(float)`), instead of ±12 at the origin.

| Row | Observed condition |
|---|---|
| `texture material` | cube drawn with a `checker.png` material (texture id ≠ 0); ≥ 4 light/dark alternations in 9 pixels read across its top face |
| `primitives` | cube 12 tris, sphere(1, 12, 24) tri count as expected |
| `skinned draw` | `unit_v2` node has bone matrices, uploaded count == bones |
| `cpu skin` | `skinMesh` output == Σ wᵢ·boneᵢ·bind for every vertex (±1e-4), and some vertex moved > 0.1 mm |
| `shadow casters` | shadow pass draws > 0 nodes, incl. ≥1 skinned (B4) |
| `shadow follows` | camera target moved to x = 20 → that point is inside the light frustum and the origin is outside (R1) |
| `depthWrite off` | pixel at a cube behind an occluder: occluder colour with `depthWrite` on (control), cube colour with it off |
| `bounding sphere` | gizmo radius == `boundingSphereRadius()` |
| `bone clamp` | 60-bone upload clamps to 48 (B8) |
| `heavy mesh` (opt-in) | robot1 or decimated mesh loads; frame time logged |

#### P2.4 SystemsLab — save, inventory, catalog, clock, audio

Engine/game changes **S1**: `GameState` and `EntityRegistry` derive from
`engine::Saveable`. **S2**: `tools/gen_sfx.py` synthesises 48 kHz mono
`select.wav move.wav hit.wav miss.wav turn.wav` and `music2.wav`.

| Row | Observed condition |
|---|---|
| `save slots` | write slots 1–3 via `save::writeAll`, read back equal |
| `slot isolation` | slot 2 edit does not change slot 1 on reload |
| `bag` | add 3/remove 1 → `countOf` == 2 |
| `equip` | equip Head → `get(Head)` set, second equip rejected, unequip returns item |
| `catalog` | `find`/`remove`/`all` sizes consistent |
| `custom calendar` | `Clock(4, 10)`: day 41 → year 2 |
| `sfx voices` | 5 SFX play overlapped, active voices ≥ 2 |
| `music switch` | `playMusic(music2)` replaces `music` |
| `volume buses` | master 0 → mixed output silent |

#### P2.5 AnimLab — clips, skeletal, events, IK

Decisions (2026-09-26): **procedural test rig first**, a real CC0 character
later (P4); loop flags and events follow a **clip name convention**.

**Asset: `tools/gen_test_rig.py` → `assets/test_rig.glb`.** A box humanoid
written directly as glTF, so every value is known and the checklist can
assert numbers.

- One mesh, one primitive, one material. One box per bone, every vertex
  weighted 1.0 to its bone (rigid). ~180 tris.
- 15 joints, parent-first order, all names set:

  | # | Joint | Parent | Rest offset from parent (m) |
  |---|---|---|---|
  | 0 | `Hips` | — | (0, 1.00, 0) |
  | 1 | `Spine` | Hips | (0, 0.25, 0) |
  | 2 | `Head` | Spine | (0, 0.35, 0) |
  | 3 | `UpperArm.L` | Spine | (0.22, 0.28, 0) |
  | 4 | `LowerArm.L` | UpperArm.L | (0, −0.28, 0) |
  | 5 | `Hand.L` | LowerArm.L | (0, −0.25, 0) |
  | 6–8 | `UpperArm.R`, `LowerArm.R`, `Hand.R` | mirror of 3–5 | x → −x |
  | 9 | `UpperLeg.L` | Hips | (0.12, 0, 0) |
  | 10 | `LowerLeg.L` | UpperLeg.L | (0, −0.45, 0) |
  | 11 | `Foot.L` | LowerLeg.L | (0, −0.45, 0) |
  | 12–14 | `UpperLeg.R`, `LowerLeg.R`, `Foot.R` | mirror of 9–11 | x → −x |

  Legs hang straight down −Y in rest (what `solveTwoBone` assumes); feet at
  y = 0.10; +Y up, faces +Z, armature node identity.
- Clips (linear keys, sampled at 30 fps):

  | Clip | Length | Motion | Notes |
  |---|---|---|---|
  | `Idle` | 2.0 s | Spine X ±3°, Head Y ±10° | arms **untracked** (B6 check) |
  | `Walk` | 1.0 s | UpperLeg X ±25° opposite phase, LowerLeg bend 0–40°, arms ±20°, Hips y bob 0.03 | in place |
  | `Attack` | 0.6 s | UpperArm.R X 0 → −100° (0.25) → −20° (0.35) → 0 | hit at 0.35 |
  | `Hurt` | 0.4 s | Spine X −15° at 0.1, back to 0 | |

**Clip name convention** (engine, `anim::clipTraits(name)`):

- Loop: `Idle`, `Walk`, `Run`, and any name ending `_loop`. Everything else
  is one-shot (`Attack`, `Hurt`, `Death`, `*_once`).
- Events: a game-side table keyed by clip name, applied after load
  (`src/game/ClipEvents.{h,cpp}`):

  | Clip | Events (time s → id) |
  |---|---|
  | `Attack` | 0.35 → `kHit` (1) |
  | `Walk` | 0.0 → `kFootL` (2), 0.5 → `kFootR` (3) |
  | `Hurt`, `Idle` | none |

  The generator and the table share these numbers; the generator prints
  them so a mismatch is visible.

**Engine changes** (each with a host selfTest on synthetic data):

| # | Change | Where |
|---|---|---|
| A1 | `Bone::name`; `Skeleton::find(name)` → index or −1 | `Bone.h`, `GltfLoader.cpp` `extractSkeleton` |
| A2 | `clip.loop` from `anim::clipTraits(name)` | `Animation.{h,cpp}`, `GltfLoader.cpp` `extractAnimations` |
| A3 | `SkeletalClip::events`; `SceneNode::update` fires them with the same crossing logic as rigid clips (shared helper) | `Animation.h`, `SceneNode.cpp` |
| A4 | `AttackAction` takes the skeletal hit time from `events` (id 1); drop the hard-coded 0.15 s | `Action.cpp` |
| A5 | Pose hook: `SceneNode::setPoseModifier(fn(std::vector<Mat4>& perBoneLocal))`, called between `sampleSkeletal` and `computeBoneMatrices` | `SceneNode.{h,cpp}` |
| A6 | Joints sorted parent-first on load (vertex joint indices + track indices remapped), so `computeBoneMatrices`' `assert(p < i)` can't fire on real exports | `GltfLoader.cpp` |
| A7 | Skeletal foot-IK adapter (game side): per leg, model-space hip/knee/foot from rest + sampled pose → `solveTwoBone` → overwrite UpperLeg/LowerLeg local rotation via A5 | `src/game/FootIKRig.{h,cpp}` |

**Scene:** HILL landscape (reuses battle heightmaps), rig on a 3×3 patch
that spans a slope, plus one rigid cube with the existing `attackClip`.

| Row | Observed condition |
|---|---|
| `rig loaded` | 15 bones, 4 clips, all 6 leg joints found by name (A1) |
| `bind pose` | no clip → all skin matrices ≈ identity |
| `loop flags` | `Idle`/`Walk` loop, `Attack`/`Hurt` don't (A2) |
| `untracked at rest` | during `Idle`, `UpperArm.L` model pos == rest (0.22, 1.53, 0) ±1e-4 (B6) |
| `idle wraps` | `Idle` time wraps ≥ 2× |
| `one-shot ends` | `Attack` holds last frame, then lab returns rig to `Idle` |
| `skeletal hit event` | `Attack` fires `kHit` once per play at 0.35 ± 1 frame (A3, A4) |
| `event re-fires` | `kFootL` fires once per `Walk` cycle over 3 cycles |
| `playback rate` | rate 0.5 → `Walk` period 2.0 s ±2 %; rate 2 → 0.5 s |
| `walk during move` | `MoveAction` plays `Walk`, back to `Idle` at the end |
| `foot IK on slope` | both feet' model y within 1 cm of `cellY` under them (A5, A7) |
| `knee plane` | IK knee bends toward +Z (hint), not sideways |
| `IK clamp` | unreachable target → `clamped`, leg straight |
| `rigid event` | cube `attackClip` fires id 1 once (existing path) |

Auto-drive: Idle 4 s → Attack ×2 → Walk 3 cycles → rate 0.5/2.0 →
move across 3 cells → IK on → drag target out of reach.

**Real character (later, P4):** a CC0 humanoid through a Blender pass
(merge to one mesh/primitive, ≤48 joints, apply transforms, rename clips to
the convention). The same AnimLab rows must pass with it swapped in; any
failure is a loader gap to fix, not a checklist change.

### P3 — Data-driven BattleScene (integration)

1. **`.lvl` v3**: add a `catalogId` string per unit, plus a prop list and
   an optional win condition. `make_level.py` emits `level_01..03`, with
   props on each.
2. Spawn units from `lvl.units` through `spawnFromCatalog`, and props
   through `PropFactory`/`buildMeshNode` (L1). Use `lvl.cellY` elevation;
   the heightmap preset becomes an override, and the terrain mesh is
   re-attached so the LAND button swap works.
3. `BattleScene(levelId)` constructor; the level is chosen from Hub (F3).
4. Rules: `stats.mov` limit on paths; a ranged action with a projectile
   (L4); stun applied by one skill (Y3); elevation LoS (F8); cell cover
   (L2).
5. Simple AI (F4): move toward nearest enemy, attack in range. It runs on
   the non-player turn instead of letting the timer expire.
6. Win/lose check → `ResultScene` → Hub. `GameState` keyed by unit id
   (not by registry index) and synced back on exit.

### P4 — Engine gaps (only after the labs show the need)

- Real CC0 humanoid swapped in for `test_rig.glb`; AnimLab rows must still pass.
- `solveTwoBone` for rigs whose rest legs are not straight down −Y.
- glTF material + baseColor texture import (robot1 already carries JPEGs).
- Sun/ambient **colour** in the FrameUBO (`SunState.sunColor`/`ambientColor`
  are computed and then dropped); `duskSky`, `milkyWayTint`.
- Point lights (torch prop) — needs a light array UBO.
- WAV resampling (the loader rejects non-48 kHz); stereo pan per voice.
- Decide `MeshDataParser`: wire `.obj` into the registry or delete it.
- Arcade battle scene (AGENTS backlog #5) builds on AnimLab + BattleScene.

## 4. Assets needed

| Asset | For | Source | Status |
|---|---|---|---|
| `test_rig.glb` (15-joint box humanoid, Idle/Walk/Attack/Hurt) | AnimLab, RenderLab | `tools/gen_test_rig.py` (P2.5) | to write |
| 48 kHz mono SFX: select, move, hit, miss, turn + `music2.wav` | SystemsLab, P3 | `tools/gen_sfx.py` (P2.4) | to write |
| Real CC0 rigged humanoid ≤10K tris, ≤48 joints | P4 / battle units | Quaternius or Kenney pack via Blender | later |
| 2–3 prop `.glb` (tree, rock, crate) ≤2K tris | RenderLab casters, P3 props | Kenney kits; `giant.glb` (485 tris) as stand-in | later |
| Decimated robot1 (≤15K tris) | RenderLab heavy-mesh row | Blender decimate | optional |

## 5. Docs to fix alongside

- `VERIFICATION.md`: the A2/A3 rows are inaccurate (the goblin drew robot1
  with `litMat`, not the skinned path); F9 duplicates A4; §1.8 lists IK
  buttons that no longer exist; the X6 note says the grid is 10×10 (it is
  20×20). Re-key the gap IDs to the lab that closes them.
- `AGENTS.md`: the `SLICES_V2.md §ArcadeBattle` link points at a section
  that doesn't exist.
- `SLICES_V2.md:816-873`: the IK section is stale.

## 6. Suggested order

1. P0 leftovers (items 2–5).
2. FxLab (no engine changes; proves B1/B2/B5 on GPU).
3. CameraLab (no engine changes).
4. AnimLab: `gen_test_rig.py` → A1–A6 with selfTests → A7 → lab.
5. RenderLab + R1 (reuses `test_rig.glb` as a skinned caster).
6. SystemsLab + S1/S2.
7. P3 battle, then P4 (real character, glTF materials, point lights).
