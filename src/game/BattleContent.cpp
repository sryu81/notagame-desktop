#include "BattleData.h"

#include <cassert>
#include <cmath>
#include <functional>

#include <GLES3/gl3.h>

#include "EntityBuilder.h"
#include "GameState.h"
#include "Unit.h"
#include "UnitDef.h"

namespace battle {

namespace {

const math::Vec3 kCameraEye(0.0f, 7.39f, 9.83f);
const math::Vec3 kCameraTarget(0.0f, 0.5f, 0.0f);
const math::Vec3 kCameraUp(0.0f, 1.0f, 0.0f);
constexpr float kCameraDistanceDefault = 12.0f;
constexpr float kCameraDistanceMin     = 5.0f;
constexpr float kCameraDistanceMax     = 20.0f;
const math::Vec3 kLightDir = math::Vec3(0.4f, 0.8f, 0.5f).normalized();
constexpr float kAmbient   = 0.2f;
constexpr float kTileLift  = 0.08f;

void createClips() {
    d().attackClip.loop = false;
    d().attackClip.keyframes = {
        {0.0f,  math::Vec3(0.0f),      math::Quat::identity(), math::Vec3(0.56f)},
        {0.15f, math::Vec3(0.4f, 0.0f, 0.0f), math::Quat::identity(), math::Vec3(0.56f)},
        {0.5f,  math::Vec3(0.0f),      math::Quat::identity(), math::Vec3(0.56f)}};
    d().attackClip.events.push_back({0.15f, 1});

    d().hitReactionClip.loop = false;
    d().hitReactionClip.keyframes = {
        {0.0f,   math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)},
        {0.05f,  math::Vec3(0.0f), math::Quat::identity(), math::Vec3(0.6f)},
        {0.15f,  math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)}};

    d().skinnedAttackClip.loop = false;
    d().skinnedAttackClip.tracks.push_back({0, {
        {0.0f,  math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)},
        {0.15f, math::Vec3(0.0f), math::Quat::fromAxisAngle(math::Vec3(1,0,0), 0.4f), math::Vec3(1.0f)},
        {0.5f,  math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)}}});
    d().skinnedAttackClip.tracks.push_back({1, {
        {0.0f,  math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)}}});
    d().skinnedAttackClip.events.push_back({0.15f, 1});

    d().skinnedHitReaction.loop = false;
    d().skinnedHitReaction.tracks.push_back({0, {
        {0.0f,  math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)},
        {0.05f, math::Vec3(0.0f), math::Quat::identity(), math::Vec3(0.6f)},
        {0.15f, math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)}}});
    d().skinnedHitReaction.tracks.push_back({1, {
        {0.0f,  math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)}}});
}

void loadSkinnedUnit(Renderer& r) {
    std::vector<uint8_t> assetBytes;
    if (!r.platform().readAsset("unit_v2.glb", assetBytes)) {
        aout << "[glb] unit_v2.glb not found in assets" << std::endl;
        return;
    }
    gltf::LoadResult glb;
    if (!gltf::load(assetBytes.data(), assetBytes.size(), glb)) {
        aout << "[glb] failed to load unit_v2.glb" << std::endl;
        return;
    }
    d().skinnedSkeleton = std::move(glb.skeleton);
    aout << "[glb] unit_v2.glb: bones=" << d().skinnedSkeleton.bones.size()
         << " clips=" << glb.clips.size() << std::endl;
}

} // namespace

int& pendingLandscapeIdx() {
    static int idx = -1;
    return idx;
}

void setLandscape(int idx) {
    if (!d().grid || idx < 0 || idx >= static_cast<int>(d().terrainHandles.size())) return;
    d().landscapeIdx = idx;
    const std::vector<float>& ys = d().landscapeCellY[idx];
    const int w = d().grid->width();
    for (int y = 0; y < d().grid->height(); ++y)
        for (int x = 0; x < w; ++x) d().grid->setCellY(x, y, ys[static_cast<size_t>(y) * w + x]);
    d().grid->reTranslateTiles(d().tileNodes, kTileLift);
    if (d().terrainNode && !d().terrainNode->drawables().empty()) {
        auto& dr = d().terrainNode->drawables().front();
        dr.mesh = d().assets->mesh(d().terrainHandles[idx]);
        dr.model = d().assets->model(d().terrainHandles[idx]);
    }
    for (size_t i = 0; i < d().registry->size(); ++i) {
        auto* c = d().registry->unitAt(i);
        if (c) placeUnitAtCell(c);
    }
}

void createContent(Renderer& r, game::GameState& gs) {
    d().assets = std::make_unique<game::LabAssets>(r.platform());
    d().litMat   = d().assets->material("lit");
    d().unlitMat = d().assets->material("unlit");
    createClips();

    d().root = std::make_unique<SceneNode>();

    level::Level lvl;
    {
        std::vector<uint8_t> assetBytes;
        const bool asset = r.platform().readAsset("level_01.lvl", assetBytes);
        if (!asset) { aout << "[level] failed to open level_01.lvl" << std::endl; return; }
        const uint8_t* bytes = assetBytes.data();
        size_t size = assetBytes.size();
        bool ok = level::load(bytes, size, lvl);
        if (!ok) { aout << "[level] failed to parse level_01.lvl" << std::endl; return; }
        aout << "[level] loaded " << lvl.width << "x" << lvl.height
             << " cells=" << lvl.terrain.size()
             << " units=" << lvl.units.size()
             << " props=" << lvl.props.size() << std::endl;
    }

    const int terrainW = static_cast<int>(lvl.width);
    const int terrainH = static_cast<int>(lvl.height);
    const float terrainCS = lvl.cellSize;

    struct HeightmapShape {
        const char* label;
        std::function<float(int, int)> yAt;
    };
    const std::vector<HeightmapShape> kShapes = {
        {"FLAT", [](int, int) { return 0.0f; }},
        {"HILL", [terrainW, terrainH](int x, int z) {
            const float cx = static_cast<float>(terrainW) * 0.5f;
            const float cz = static_cast<float>(terrainH) * 0.5f;
            const float r2max = (cx * cx) + (cz * cz);
            const float dx = static_cast<float>(x) - cx;
            const float dz = static_cast<float>(z) - cz;
            const float t = 1.0f - ((dx * dx) + (dz * dz)) / r2max;
            return (t > 0.0f ? t : 0.0f) * 1.5f;
        }},
        {"VALLEY", [terrainW, terrainH](int x, int z) {
            const float cx = static_cast<float>(terrainW) * 0.5f;
            const float cz = static_cast<float>(terrainH) * 0.5f;
            const float dx = static_cast<float>(x) - cx;
            const float dz = static_cast<float>(z) - cz;
            const float d = std::sqrt((dx * dx) + (dz * dz));
            const float maxD = std::sqrt((cx * cx) + (cz * cz));
            const float t = 1.0f - (d / maxD);
            return -t * 1.0f;
        }},
        {"RIDGE", [](int x, int) {
            return std::sin(static_cast<float>(x) * 2.0f * 3.14159265f / 3.0f) * 0.6f;
        }},
        {"CLIFF", [terrainW](int x, int) {
            return (static_cast<float>(x) < static_cast<float>(terrainW) * 0.5f) ? -0.5f : 1.0f;
        }},
    };

    d().landscapeCellY.clear();
    d().landscapeCellY.push_back(lvl.cellY);
    for (const auto& shape : kShapes) {
        const Mesh hm = primitives::makeHeightmap(terrainW, terrainH, terrainCS, shape.yAt);
        grid::BattleGrid sampler(terrainW, terrainH, terrainCS);
        sampler.applyElevationFromMesh(hm);
        std::vector<float> ys(static_cast<size_t>(terrainW) * terrainH);
        for (int y = 0; y < terrainH; ++y)
            for (int x = 0; x < terrainW; ++x) ys[static_cast<size_t>(y) * terrainW + x] = sampler.cellY(x, y);
        d().landscapeCellY.push_back(std::move(ys));
    }
    for (size_t i = 0; i < d().landscapeCellY.size(); ++i) {
        const char* label = i == 0 ? "LEVEL" : kShapes[i - 1].label;
        d().terrainHandles.push_back(d().assets->addMesh(
            label, primitives::makeCellTerrain(terrainW, terrainH, terrainCS, d().landscapeCellY[i])));
    }
    assert(static_cast<int>(d().terrainHandles.size()) == kLandscapeCount);

    loadSkinnedUnit(r);
    d().assets->upload();

    const Mesh*  gridMesh   = d().assets->mesh("plane");
    const Model* planeModel = d().assets->model("plane");

    d().grid = std::make_unique<grid::BattleGrid>(lvl.width, lvl.height, lvl.cellSize);
    for (uint16_t y = 0; y < lvl.height; ++y) {
        for (uint16_t x = 0; x < lvl.width; ++x) {
            const size_t i = static_cast<size_t>(y) * lvl.width + x;
            if (lvl.terrain[i] == 0) d().grid->setPassable(x, y, false);
            d().grid->setCellY(x, y, lvl.cellY[i]);
        }
    }

    d().terrainNode = d().grid->attachTerrainMesh(d().root.get(), d().assets->mesh(d().terrainHandles[0]),
                                                  d().assets->model(d().terrainHandles[0]), d().litMat);
    d().tileNodes = d().grid->attachTo(d().root.get(), gridMesh, planeModel,
                    math::Vec3(0.76f, 0.70f, 0.50f), math::Vec3(0.30f, 0.30f, 0.30f),
                    d().litMat);

    d().shadowRoot = std::make_unique<SceneNode>();
    d().registry = std::make_unique<game::EntityRegistry>();

    {
        unit::UnitDef kd;
        kd.name = "Knight"; kd.faction = unit::Faction::Player;
        kd.baseScale = 0.56f;
        kd.baseStats = {15, 6, 4, 4, 1};
        kd.baseColor = {0.9f, 0.3f, 0.2f}; kd.selectedColor = {1.5f, 0.9f, 0.2f};
        d().unitCatalog.registerDef("knight", kd);

        unit::UnitDef md;
        md.name = "Mage"; md.faction = unit::Faction::Ally;
        md.baseScale = 0.56f;
        md.baseStats = {12, 8, 2, 3, 2};
        md.baseColor = {0.3f, 0.5f, 0.9f}; md.selectedColor = {0.5f, 0.9f, 1.5f};
        d().unitCatalog.registerDef("mage", md);

        unit::UnitDef gd;
        gd.name = "Goblin"; gd.faction = unit::Faction::Enemy;
        gd.baseScale = 0.56f;
        gd.baseStats = {10, 5, 3, 3, 1};
        gd.baseColor = {0.2f, 0.7f, 0.2f}; gd.selectedColor = {1.0f, 0.2f, 0.2f};
        if (!d().skinnedSkeleton.bones.empty()) gd.skeleton = &d().skinnedSkeleton;
        d().unitCatalog.registerDef("goblin", gd);
    }

    {
        game::EntityBuilder eb(d().root.get(), d().grid.get(), d().shadowRoot.get(),
                               &d().unitClips);
        eb.populate(*d().registry);
        if (!gs.empty()) gs.restoreTo(*d().registry);
    }

    const int initialIdx = pendingLandscapeIdx() >= 0 ? pendingLandscapeIdx() : 0;
    pendingLandscapeIdx() = -1;
    setLandscape(initialIdx);

    d().turnManager = std::make_unique<turn::Manager>();
    d().turnManager->setUnits(charactersAll());

    d().savePath = r.dataPath() + "/battle.sav";
    if (save::fileExists(d().savePath.c_str())) loadGame();

    d().actionManager = std::make_unique<action::Manager>();
    d().actionManager->setOnComplete([]() {
        setPathOverlay({});
        if (d().turnManager && d().turnManager->isPlayerTurn()) d().turnManager->endTurn();
    });

    d().pathOverlay = std::make_unique<SceneNode>();

    {
        auto ind = std::make_unique<SceneNode>();
        ind->addDrawable({gridMesh, planeModel, math::Vec3(1.0f, 0.95f, 0.15f), d().unlitMat});
        ind->setLocal(math::Mat4::scale(math::Vec3(0.0f, 1.0f, 0.0f)));
        d().selectionIndicator = std::move(ind);
    }

    r.camera().setPerspective(
        kCameraFovYRadians,
        r.width() > 0 ? static_cast<float>(r.width()) / static_cast<float>(r.height()) : 1.0f,
        kCameraNearPlane, kCameraFarPlane);
    r.camera().setViewLookAt(kCameraEye, kCameraTarget, kCameraUp);
    r.camera().setDistanceLimits(kCameraDistanceMin, kCameraDistanceMax, kCameraDistanceDefault);
    r.camera().setTerrainHeightFn(
        [g = d().grid.get()](float x, float z) -> float {
            math::Vec2 cell = g->worldToCell(math::Vec3(x, 0.0f, z));
            int cx = static_cast<int>(cell.x);
            int cz = static_cast<int>(cell.y);
            if (!g->inBounds(cx, cz)) return 0.0f;
            return g->cellY(cx, cz);
        },
        0.5f);

    r.setFrameLight(kLightDir, kAmbient);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

} // namespace battle
