#include "BattleData.h"

#include <cctype>
#include <cstdio>

#include <GLES3/gl3.h>

#include "ClipEvents.h"
#include "EntityBuilder.h"
#include "GameState.h"
#include "LevelSpawner.h"
#include "Unit.h"
#include "UnitDef.h"
#include "UnitTypes.h"

namespace battle {

namespace {

const math::Vec3 kLightDir = math::Vec3(0.4f, 0.8f, 0.5f).normalized();
constexpr float kAmbient  = 0.2f;
constexpr float kTileLift = 0.08f;
constexpr float kUnitScale = 0.5f;

const char* kLevelNames[] = {"FLAT", "HILL", "FORT"};

bool loadRig(Renderer& r) {
    std::vector<uint8_t> bytes;
    if (!r.platform().readAsset("test_rig.glb", bytes)) return false;
    gltf::LoadResult glb;
    if (!gltf::load(bytes.data(), bytes.size(), glb)) return false;
    d().skeleton = std::move(glb.skeleton);
    for (auto& [name, clip] : glb.clips) {
        game::applyClipEvents(name, clip);
        d().clips[name] = std::move(clip);
    }
    d().rigMesh = d().assets->addMesh("unit_rig", std::move(glb.mesh));
    return true;
}

const anim::SkeletalClip* clip(const char* name) {
    auto it = d().clips.find(name);
    return it == d().clips.end() ? nullptr : &it->second;
}

void buildCatalog(bool rig) {
    d().catalog.clear();
    const unit::ClipSet set{clip("Idle"), clip("Walk"), clip("Attack"), clip("Hurt"), clip("Death")};
    auto def = [rig, &set](const char* id, const char* name, unit::Faction f, unit::Stats st, math::Vec3 color) {
        unit::UnitDef u;
        u.name = name;
        u.faction = f;
        u.baseStats = st;
        u.baseColor = color;
        u.baseScale = kUnitScale;
        u.modelYawDeg = 180.0f;
        if (rig) {
            u.skeleton = &d().skeleton;
            u.mesh = d().assets->mesh(d().rigMesh);
            u.model = d().assets->model(d().rigMesh);
            u.material = d().assets->material("skinned");
            u.clips = set;
        }
        d().catalog.registerDef(id, u);
    };
    for (const auto& t : game::unitTypes()) def(t.id, t.name, t.faction, t.stats, t.color);
}

bool checkSpawn() {
    const auto& recs = d().level.units;
    if (d().units.size() != recs.size() || d().propCount != d().level.props.size()) return false;
    for (size_t i = 0; i < recs.size(); ++i) {
        const game::Unit* u = d().units[i];
        const auto& rec = recs[i];
        const unit::UnitDef* def = d().catalog.find(rec.catalogId);
        if (!def || u->unitId() != rec.unitId || static_cast<int>(u->cell().x) != rec.x ||
            static_cast<int>(u->cell().y) != rec.y || u->stats().maxHp != def->baseStats.maxHp ||
            u->stats().atk != def->baseStats.atk || u->faction() != def->faction)
            return false;
    }
    return true;
}

}

const char* levelName(int levelId) {
    return levelId >= 1 && levelId <= kLevelCount ? kLevelNames[levelId - 1] : "?";
}

void createContent(Renderer& r, game::GameState& gs) {
    d().assets = std::make_unique<game::LabAssets>(r.platform());
    d().litMat   = d().assets->material("lit");
    d().unlitMat = d().assets->material("unlit");
    const bool rig = loadRig(r);
    if (!rig) aout << "[battle] test_rig.glb missing; cube rigs" << std::endl;

    char file[32];
    snprintf(file, sizeof(file), "level_%02d.lvl", d().levelId);
    std::vector<uint8_t> bytes;
    if (!r.platform().readAsset(file, bytes) || !level::load(bytes.data(), bytes.size(), d().level)) {
        aout << "[level] failed to load " << file << std::endl;
        return;
    }
    const level::Level& lvl = d().level;
    aout << "[level] loaded " << file << " " << lvl.width << "x" << lvl.height << " units="
         << lvl.units.size() << " props=" << lvl.props.size() << " win=" << level::describe(lvl.win) << std::endl;
    d().terrainMesh = d().assets->addMesh("terrain", primitives::makeCellTerrain(
        lvl.width, lvl.height, lvl.cellSize, lvl.cellY));
    d().assets->upload();
    buildCatalog(rig);

    d().root = std::make_unique<SceneNode>();
    d().grid = game::LevelSpawner::makeGrid(lvl);
    d().grid->attachTerrainMesh(d().root.get(), d().assets->mesh(d().terrainMesh),
                                d().assets->model(d().terrainMesh), d().litMat);
    auto tiles = d().grid->attachTo(d().root.get(), d().assets->mesh("plane"), d().assets->model("plane"),
                                    math::Vec3(0.76f, 0.70f, 0.50f), math::Vec3(0.30f, 0.30f, 0.30f), d().litMat);
    d().grid->reTranslateTiles(tiles, kTileLift);

    d().registry = std::make_unique<game::EntityRegistry>();
    {
        game::EntityBuilder eb(d().root.get(), d().grid.get());
        const game::LevelSpawner::Visuals vis{d().assets->mesh("cube"), d().assets->model("cube"), d().litMat};
        d().propCount = game::LevelSpawner::spawnProps(lvl, eb, *d().grid, vis).size();
        game::LevelSpawner::spawnUnits(lvl, d().catalog, eb, vis);
        eb.populate(*d().registry);
    }
    d().units = d().registry->allUnits();
    d().names.clear();
    for (const auto& rec : lvl.units) {
        std::string n = rec.catalogId;
        for (char& c : n) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        d().names.push_back(n);
    }
    if (!gs.empty()) gs.restoreTo(*d().registry);
    d().spawnedOk = checkSpawn();

    d().turns = std::make_unique<turn::Manager>();
    d().turns->setUnits(d().units);
    d().actions = std::make_unique<action::Manager>();
    d().flow = std::make_unique<flow::BattleFlow>(*d().turns, *d().actions);
    d().rng = std::make_unique<ai::Rng>(static_cast<uint32_t>(d().levelId) * 7919u + 1u);
    d().projRoot = d().root->addChild();
    d().overlay = std::make_unique<SceneNode>();
    d().ring = std::make_unique<SceneNode>();
    d().ring->addDrawable({d().assets->mesh("plane"), d().assets->model("plane"),
                           math::Vec3(1.0f, 0.95f, 0.15f), d().unlitMat});
    d().ring->setLocal(math::Mat4::scale(math::Vec3(0.0f, 1.0f, 0.0f)));
    d().savePath = r.dataPath() + "/battle_" + std::to_string(d().levelId) + ".sav";

    Camera& cam = r.camera();
    cam.setPerspective(kCameraFovYRadians,
                       r.width() > 0 ? static_cast<float>(r.width()) / static_cast<float>(r.height()) : 1.0f,
                       kCameraNearPlane, kCameraFarPlane);
    cam.setForward(math::Vec3(0, 0, -1));
    cam.setPitchLimits(Camera::kMinPitchDeg, Camera::kMaxPitchDeg);
    cam.setYawLimits(Camera::kMinYawDeg, Camera::kMaxYawDeg);
    cam.setDistanceLimits(6.0f, 30.0f, 16.0f);
    cam.setViewLookAt(math::Vec3(0.0f, 10.0f, 13.0f), math::Vec3(0.0f, 0.0f, 0.0f), math::Vec3(0, 1, 0));
    const float hx = 0.5f * static_cast<float>(lvl.width) * lvl.cellSize;
    const float hz = 0.5f * static_cast<float>(lvl.height) * lvl.cellSize;
    cam.setTargetBounds(math::Vec2(-hx, -hz), math::Vec2(hx, hz));
    cam.setTerrainHeightFn([g = d().grid.get()](float x, float z) -> float {
        const math::Vec2 c = g->worldToCell(math::Vec3(x, 0.0f, z));
        const int cx = static_cast<int>(c.x), cz = static_cast<int>(c.y);
        return g->inBounds(cx, cz) ? g->cellY(cx, cz) : 0.0f;
    }, 0.5f);
    r.setFrameLight(kLightDir, kAmbient);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

} // namespace battle
