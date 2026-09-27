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

### P2 — Labs (each independent; any order)

| Lab | Exercises | Needs |
|---|---|---|
| **RenderLab** | `texMat` on cube/plane (A1 again); `makeUVSphere`/`makeCube` (G1–G2); skinned `unit_v2` with `skinMat` + `setSkeleton` (A2 really drawn); CPU `skinMesh` next to the GPU version for comparison (G4); shadow casters incl. skinned (B4, F7); shadow frustum following the camera target instead of ±12 at origin (X6); `depthWrite=false` drawable; bounding-sphere gizmo (G3) | B4 fix; frustum follow in `Renderer::computeLightVP` |
| **AnimLab** | Rigid clip + `AnimEvent`; skeletal idle/attack loaded **by name from the glb**; `setPlaybackRate` slider (Y5); event re-fire across loop (Y4); FootIK on a slope using `TileGrid::cellY` (F6); walk clip during a move (L3) | Real rigged low-poly humanoid glb (idle/walk/attack/hurt, named hip/knee/foot bones); `SkeletalClip` events (replace the hard-coded 0.15 s) |
| **FxLab** | Emitter jitter/shrink/gravity (all setters); rain with `WeatherPalette.rainColor`; all SkyConfig setters incl. the 4 unused ones and `setSkyConfig` rain/storm mixes; `StarRenderConfig` knobs at night (X4); lightning knobs; time-lapse | B1, B2, B5 fixes |
| **CameraLab** | `orbit`/yaw presets (±45°), `setPitchLimits`/`setYawLimits`, `setOrthographic` toggle, `setForward`, follow target, `pickAt` hit display on meshes (A4), PointerUp/Move and KeyDown consumers (F5) | Camera API only |
| **SystemsLab** | Save slots 1–3 through `Saveable` + `save::writeAll/loadAll` (F1, F2); `Bag` add/remove/countOf and `EquipSlots` equip/unequip shown in HUD; `Catalog` find/remove/all; `Clock(months, days)` custom calendar (C5); per-voice SFX triggers; music switch (F10, F11) | `GameState`/`EntityRegistry` derive from `engine::Saveable`; a few extra 48 kHz SFX wavs |

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

- glTF material + baseColor texture import (robot1 already carries JPEGs).
- Sun/ambient **colour** in the FrameUBO (`SunState.sunColor`/`ambientColor`
  are computed and then dropped); `duskSky`, `milkyWayTint`.
- Point lights (torch prop) — needs a light array UBO.
- WAV resampling (the loader rejects non-48 kHz); stereo pan per voice.
- Decide `MeshDataParser`: wire `.obj` into the registry or delete it.
- Arcade battle scene (AGENTS backlog #5) builds on AnimLab + BattleScene.

## 4. Assets needed

| Asset | For | Source idea |
|---|---|---|
| Low-poly rigged humanoid `.glb`, ≤10K tris, clips idle/walk/attack/hurt, named leg bones | AnimLab, RenderLab, BattleScene units | CC0: Quaternius / Kenney character packs; or Mixamo → Blender export |
| 2–3 prop `.glb` (tree, rock, crate) ≤2K tris | L1 props, shadow casters | Kenney nature/dungeon kits |
| SFX wav 48 kHz mono: select, move, hit, miss, turn | F10 | jsfxr/sfxr |
| Second music track | F11 per-scene music | any CC0 loop, 48 kHz |

## 5. Docs to fix alongside

- `VERIFICATION.md`: the A2/A3 rows are inaccurate (the goblin drew robot1
  with `litMat`, not the skinned path); F9 duplicates A4; §1.8 lists IK
  buttons that no longer exist; the X6 note says the grid is 10×10 (it is
  20×20). Re-key the gap IDs to the lab that closes them.
- `AGENTS.md`: the `SLICES_V2.md §ArcadeBattle` link points at a section
  that doesn't exist.
- `SLICES_V2.md:816-873`: the IK section is stale.

## 6. Suggested order

P0 (1 session) → P1 Hub + DebugPanel + LabChecklist (1) → FxLab (smallest,
verifies B1/B2/B5) → RenderLab → CameraLab → SystemsLab → P3 battle →
AnimLab once a rigged asset is in hand → P4 as needed.
