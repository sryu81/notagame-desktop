#include "BattleScene.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>

#include <GLES3/gl3.h>

#include "Action.h"
#include "BattleGrid.h"
#include "Climate.h"
#include "Engine.h"
#include "EntityBuilder.h"
#include "EntityRegistry.h"
#include "GameState.h"
#include "Path.h"
#include "TitleScene.h"
#include "Turn.h"
#include "Unit.h"
#include "UnitCatalog.h"
#include "UnitDef.h"
#include "UnitFactory.h"
namespace {

static constexpr float kCameraFovYRadians = 30.0f * math::PI / 180.0f;
static constexpr float kCameraNearPlane   = 0.1f;
static constexpr float kCameraFarPlane    = 100.0f;
static const math::Vec3 kCameraEye(0.0f, 7.39f, 9.83f);
static const math::Vec3 kCameraTarget(0.0f, 0.5f, 0.0f);
static const math::Vec3 kCameraUp(0.0f, 1.0f, 0.0f);
static constexpr float kCameraDistanceDefault = 12.0f;
static constexpr float kCameraDistanceMin     = 5.0f;
static constexpr float kCameraDistanceMax     = 20.0f;
static const math::Vec3 kLightDir = math::Vec3(0.4f, 0.8f, 0.5f).normalized();
static constexpr float kAmbient   = 0.2f;

static constexpr int kButtonSize   = 72;
static constexpr int kButtonW      = 144;
static constexpr int kButtonH      = 60;
static constexpr int kButtonMargin = 48;
static constexpr int kBannerHeight = 64;

struct SavedStatus { int32_t type; int32_t duration; int32_t magnitude; };
struct SavedUnit {
    int32_t faction;
    int32_t hp;
    float   cellX, cellY;
    int32_t statusCount;
    SavedStatus statuses[4];
};
struct SaveData {
    uint32_t magic;
    uint32_t version;
    uint32_t unitCount;
    int32_t  turnIndex;
    float    turnElapsed;
    SavedUnit units[16];
};
static constexpr uint32_t kSaveMagic   = 0x4E41474D;
static constexpr uint32_t kSaveVersion = 2;
static constexpr int      kMaxSavedUnits = 16;

struct Data {
    std::unique_ptr<asset::Registry> assets;
    asset::Handle hPlaneMesh = asset::kNull;
    const Material* litMat   = nullptr;
    const Material* unlitMat = nullptr;
    const Material* skinMat  = nullptr;
    const Material* texMat  = nullptr;
    std::shared_ptr<TextureAsset> robotTex;
    const Mesh*  cubeMesh  = nullptr;
    const Model* cubeModel = nullptr;
    const Mesh*  robotMesh  = nullptr;
    const Model* robotModel = nullptr;
    asset::Handle hRobotMesh = asset::kNull;
    const Mesh*  giantMesh  = nullptr;
    const Model* giantModel = nullptr;
    asset::Handle hGiantMesh = asset::kNull;
    const Mesh*  unitV2Mesh  = nullptr;
    const Model* unitV2Model = nullptr;
    asset::Handle hUnitV2Mesh = asset::kNull;

    std::unique_ptr<SceneNode> root;

    std::unique_ptr<game::EntityRegistry> registry;

    std::unique_ptr<grid::BattleGrid> grid;
    std::unique_ptr<turn::Manager> turnManager;
    std::unique_ptr<action::Manager> actionManager;
    anim::Clip playerSpinClipTemplate;
    anim::Clip animeClipTemplate;
    anim::Clip enemyBobClipTemplate;
    anim::Clip allyBobClipTemplate;
    game::UnitCatalog unitCatalog;
    std::deque<anim::Clip> unitClips;
    anim::Clip attackClip;
    anim::Clip hitReactionClip;
    anim::SkeletalClip skinnedAttackClip;
    anim::SkeletalClip skinnedHitReaction;
    anim::Skeleton skinnedSkeleton;
    anim::SkeletalClip skinnedClip;
    std::string savePath;

    particle::Emitter particleEmitter;
    particle::Emitter rainEmitter{1024};
    float rainAccum  = 0.0f;
    float burstTimer = 0.0f;

    std::unique_ptr<SceneNode> pathOverlay;
    std::unique_ptr<SceneNode> selectionIndicator;
    game::Unit* lastTurnUnit = nullptr;

    std::unique_ptr<ui::UIRoot> ui;
    ui::UIText*   turnBanner   = nullptr;
    ui::UIText*   fpsText      = nullptr;
    ui::UIButton* zoomInBtn    = nullptr;
    ui::UIButton* zoomOutBtn   = nullptr;
    ui::UIButton* resetBtn     = nullptr;
    ui::UIButton* saveBtn      = nullptr;
    ui::UIButton* exitBtn      = nullptr;
    ui::UIButton* backBtn      = nullptr;

    ui::UIText*   debugText    = nullptr;
    ui::UIButton* pauseBtn     = nullptr;
    ui::UIButton* plus1hBtn    = nullptr;
    ui::UIButton* dawnBtn      = nullptr;
    ui::UIButton* wxBtn        = nullptr;
    ui::UIText*   timeText     = nullptr;
    ui::UIButton* cameraBtn    = nullptr;
    ui::UIButton* camOrbitBtn  = nullptr;
    ui::UIButton* camFovBtn    = nullptr;
    ui::UIButton* camSensBtn   = nullptr;
    ui::UIButton* audioBtn     = nullptr;
    ui::UIButton* masterBtn    = nullptr;
    ui::UIButton* musicBtn     = nullptr;
    ui::UIButton* sfxBtn       = nullptr;
    ui::UIButton* playSfxBtn   = nullptr;
    ui::UIButton* particlesBtn    = nullptr;
    ui::UIButton* partGravityBtn  = nullptr;
    ui::UIButton* partUpVelBtn    = nullptr;
    ui::UIButton* partShrinkBtn   = nullptr;
    ui::UIButton* partBurstBtn    = nullptr;
    ui::UIButton* skyBtn         = nullptr;
    ui::UIButton* skyNightBtn    = nullptr;
    ui::UIButton* skyDawnBtn     = nullptr;
    ui::UIButton* skyNoonBtn     = nullptr;
    ui::UIButton* skyAmbientBtn  = nullptr;
    ui::UIButton* weatherBtn       = nullptr;
    ui::UIButton* wxFlashBtn       = nullptr;
    ui::UIButton* wxDecayBtn       = nullptr;
    ui::UIButton* wxIntervalBtn    = nullptr;
    ui::UIButton* wxJitterBtn      = nullptr;
    ui::UIButton* landBtn        = nullptr;
    ui::UIButton* resetSceneBtn  = nullptr;
    int landscapeIdx = 0;
    std::vector<asset::Handle> terrainHandles;
    const Mesh*  terrainMeshPtr  = nullptr;
    const Model* terrainModelPtr = nullptr;
    SceneNode*   terrainNode     = nullptr;
    std::vector<SceneNode*> tileNodes;
    float fpsAccum   = 0.0f;
    int   fpsFrames  = 0;
    int   speedPreset  = 2;
    int   wxPreset     = 0;
    bool  cameraPanelOpen  = false;
    int   camOrbitPreset   = 0;
    int   camFovPreset     = 1;
    int   camSensPreset    = 1;
    bool  audioPanelOpen   = false;
    int   masterPreset     = 4; // 1.0
    int   musicPreset      = 2; // 0.5
    int   sfxPreset        = 4; // 1.0
    bool  particlesPanelOpen = false;
    int   partGravityPreset  = 2; // -2.0
    int   partUpVelPreset    = 2; // 1.5
    int   partShrinkPreset   = 1; // 0.5
    bool  skyPanelOpen       = false;
    int   skyNightPreset     = 2; // 4.0
    int   skyDawnPreset      = 2; // 0.3
    int   skyNoonPreset      = 2; // 2.0
    int   skyAmbientPreset   = 2; // 0.3
    bool  weatherPanelOpen     = false;
    int   wxFlashPreset        = 2; // 1.0
    int   wxDecayPreset        = 2; // 6.0
    int   wxIntervalPreset     = 2; // 3.0s
    int   wxJitterPreset       = 2; // 7.0s

    float ikSolveMs    = 0.0f;
    bool  ikClamped    = false;
    int   boneCount    = 0;

    int   legAnimMode  = 0;
    bool  legFollowPlayer = false;
    float legTargetOverrideY = 0.0f;
    int   stressLegs   = 0;
    int   hintMode     = 0;
    float lastLoopLogTime = -1.0f;
    float animClock    = 0.0f;

    SceneNode* legRootPtr = nullptr;
    SceneNode* legHip  = nullptr;
    SceneNode* legKnee = nullptr;
    SceneNode* legFoot = nullptr;
    std::vector<SceneNode*> legRigRoots;
    std::vector<SceneNode*> legRigHips;
    std::vector<SceneNode*> legRigKnees;
    std::vector<math::Vec3>  legRigOffsets;
    int        legCellX = 2;
    int        legCellY = 2;
    float      legHipY    = 1.0f;
    float      legHipLen  = 0.75f;
    float      legKneeLen = 0.75f;
    math::Vec3 legIKHint  = math::Vec3(0, 0, 1);
    bool       legIKEnabled = true;

    std::vector<EventBus::SubscriptionId> subs;
    std::unique_ptr<SceneNode> shadowRoot_;

    bool  wasActionIdle_   = true;
    bool  camReturnActive_ = false;
    float camReturnTimer_  = 0.0f;
    math::Vec3 preMoveCamTarget_{0.0f};
    game::Unit* movingUnit_ = nullptr;

    engine::stars::StarField stars;
    engine::clock::Clock     clock;
    game::ClimateState      climate;
    game::ClimatePalettes   palettes;
    engine::sky::SunState    currentSun;
};

Data& d() {
    static Data data;
    return data;
}

static int g_pendingLandscapeIdx = -1;

// ---- helper fwd declarations ----
void setPathOverlay(const std::vector<math::Vec2>& path);
void refreshSelectionOverlay(Renderer& r);
void enqueueMove(game::Unit* u, std::vector<math::Vec2> path);
void enqueueAttack(game::Unit* attacker, game::Unit* target);
game::Unit* pickUnit(float x, float y, Renderer& r);
math::Vec2 cellAtScreen(float x, float y, Renderer& r);
bool isPlayerTurn();
game::Unit* selectedUnit();
void saveGame();
void loadGame(Renderer& r);
void createContent(Renderer& r, game::GameState& gs);
void createHUD(Renderer& r, game::GameState& gs);
void layoutHUD(int w, int h);

void setPathOverlay(const std::vector<math::Vec2>& path) {
    if (!d().pathOverlay) return;
    d().pathOverlay->clearChildren();
    if (path.empty() || !d().grid || !d().assets) return;
    const Mesh*  planeMesh  = d().assets->mesh(d().hPlaneMesh);
    const Model* planeModel = d().assets->model(d().hPlaneMesh);
    if (!planeMesh || !planeModel) return;
    for (const auto& cell : path) {
        if (!d().grid->inBounds(static_cast<int>(cell.x), static_cast<int>(cell.y))) continue;
        SceneNode* node = d().pathOverlay->addChild();
        const math::Vec3 wp = d().grid->cellToWorld(static_cast<int>(cell.x),
                                                     static_cast<int>(cell.y));
        node->setLocal(math::Mat4::translation(math::Vec3(wp.x, wp.y + 0.01f, wp.z))
                       * math::Mat4::scale(math::Vec3(d().grid->cellSize() * 0.96f, 1.0f,
                                                      d().grid->cellSize() * 0.96f)));
        node->addDrawable({planeMesh, planeModel, math::Vec3(0.20f, 0.90f, 0.30f), d().unlitMat});
    }
}

void refreshTimeText() {
    if (!d().timeText) return;
    const auto& t = d().clock.now();
    char spdBuf[16];
    if (d().clock.isPaused()) {
        snprintf(spdBuf, sizeof(spdBuf), "PAUSED");
    } else {
        float ts = d().clock.timeScale();
        if (ts == static_cast<float>(static_cast<int>(ts)))
            snprintf(spdBuf, sizeof(spdBuf), "%.0fx", ts);
        else
            snprintf(spdBuf, sizeof(spdBuf), "%.1fx", ts);
    }
    char tbuf[40];
    snprintf(tbuf, sizeof(tbuf), "Y%dM%dD%d %02d:%02d %s",
             t.year, t.month, t.day, t.hour, t.minute, spdBuf);
    d().timeText->setText(tbuf);
}

void refreshSelectionOverlay(Renderer& r) {
    if (!d().selectionIndicator || !d().grid) return;
    game::Unit* sel = nullptr;
    for (size_t i = 0; i < d().registry->size(); ++i) {
        auto* c = d().registry->unitAt(i);
        if (c && c->isSelected() && !c->isDead()) { sel = c; break; }
    }
    if (!sel) {
        d().selectionIndicator->setLocal(math::Mat4::scale(math::Vec3(0.0f, 1.0f, 0.0f)));
        return;
    }
    const math::Vec3 pos = d().grid->cellToWorld(
        static_cast<int>(sel->cell().x), static_cast<int>(sel->cell().y));
    d().selectionIndicator->setLocal(
        math::Mat4::translation(math::Vec3(pos.x, pos.y + 0.003f, pos.z))
        * math::Mat4::scale(math::Vec3(d().grid->cellSize() * 0.93f, 1.0f,
                                       d().grid->cellSize() * 0.93f)));
}

void enqueueMove(game::Unit* u, std::vector<math::Vec2> path) {
    if (!d().actionManager || !u || !d().grid) return;
    d().movingUnit_ = u;
    d().actionManager->enqueue(std::make_unique<action::MoveAction>(u, d().grid.get(), std::move(path)));
}

void enqueueAttack(game::Unit* attacker, game::Unit* target) {
    if (!d().actionManager || !attacker || !target) return;
    const int damage = std::max(1, attacker->stats().atk - target->stats().def);
    auto* node = attacker->node();
    auto* targetNode = target->node();
    bool isSkinned = (node && node->skeleton() != nullptr);
    bool targetSkinned = (targetNode && targetNode->skeleton() != nullptr);
    const anim::Clip* atkClip = isSkinned ? nullptr : &d().attackClip;
    const anim::SkeletalClip* skelAtkClip = isSkinned ? &d().skinnedAttackClip : nullptr;
    const anim::Clip* reactClip = targetSkinned ? nullptr : &d().hitReactionClip;
    const anim::SkeletalClip* skelReact = targetSkinned ? &d().skinnedHitReaction : nullptr;

    math::Vec3 targetPos(0.0f);
    if (d().grid)
        targetPos = d().grid->cellToWorld(static_cast<int>(target->cell().x),
                                          static_cast<int>(target->cell().y));
    auto action = std::make_unique<action::AttackAction>(
        attacker, target, damage, atkClip, reactClip, skelAtkClip, skelReact);
    action->setHitCallback([targetPos]() {
        d().particleEmitter.burst(targetPos, 12, 2.5f,
                                  math::Vec4(1.0f, 0.6f, 0.1f, 1.0f),
                                  0.25f, 0.5f);
    });
    d().actionManager->enqueue(std::move(action));
}

static void collectRigDrawables(SceneNode* node, std::vector<picking::HitShape>& shapes) {
    if (!node) return;
    for (const auto& d : node->drawables()) {
        if (d.mesh) {
            shapes.push_back(picking::fromMesh(*d.mesh, node->world()));
        }
    }
    for (const auto& c : node->children()) {
        collectRigDrawables(c.get(), shapes);
    }
}

game::Unit* pickUnit(float x, float y, Renderer& r) {
    const picking::Ray ray = r.pickingRay(x, y);
    game::Unit* best = nullptr;
    float bestDist = std::numeric_limits<float>::infinity();
    for (size_t i = 0; i < d().registry->size(); ++i) {
        auto* c = d().registry->unitAt(i);
        if (!c || c->isDead() || !c->node()) continue;
        std::vector<picking::HitShape> shapes;
        collectRigDrawables(c->node(), shapes);
        if (shapes.empty()) continue;
        const picking::Hit hit = picking::pickRay(ray, shapes);
        if (hit.valid && hit.distance < bestDist) { best = c; bestDist = hit.distance; }
    }
    return best;
}

math::Vec2 cellAtScreen(float x, float y, Renderer& r) {
    if (!d().grid) return math::Vec2(-1, -1);
    const picking::Ray ray = r.pickingRay(x, y);
    if (std::abs(ray.direction.y) < 1e-6f) return math::Vec2(-1, -1);
    const float t = -ray.origin.y / ray.direction.y;
    if (t < 0.0f) return math::Vec2(-1, -1);
    const math::Vec3 hitPos(ray.origin.x + ray.direction.x * t,
                            0.0f,
                            ray.origin.z + ray.direction.z * t);
    return d().grid->worldToCell(hitPos);
}

bool isPlayerTurn() {
    return d().turnManager && d().turnManager->isPlayerTurn();
}

std::vector<game::Unit*> charactersAll() {
    std::vector<game::Unit*> out;
    out.reserve(d().registry->size());
    for (size_t i = 0; i < d().registry->size(); ++i) {
        auto* c = d().registry->unitAt(i);
        if (c) out.push_back(c);
    }
    return out;
}

size_t charactersAliveCount() {
    size_t n = 0;
    for (size_t i = 0; i < d().registry->size(); ++i) {
        auto* c = d().registry->unitAt(i);
        if (c && !c->isDead()) ++n;
    }
    return n;
}

game::Unit* selectedUnit() {
    game::Unit* found = nullptr;
    for (size_t i = 0; i < d().registry->size(); ++i) {
        auto* c = d().registry->unitAt(i);
        if (c && c->isSelected() && !c->isDead()) { found = c; break; }
    }
    return found;
}

static anim::Clip* clipFor(game::Unit* u) {
    if (u == nullptr || u->node() == nullptr) return nullptr;
    return const_cast<anim::Clip*>(u->node()->clip());
}

void placeUnitAtCell(game::Unit* u) {
    if (u == nullptr || !d().grid || u->node() == nullptr) return;
    const math::Vec2 cell = u->cell();
    const math::Vec3 wp = d().grid->cellToWorld(static_cast<int>(cell.x),
                                                static_cast<int>(cell.y));
    const float unitY = wp.y;
    anim::Clip* clip = clipFor(u);
    if (clip != nullptr && !clip->keyframes.empty()) {
        const float baseY = clip->keyframes.front().translation.y;
        for (auto& kf : clip->keyframes) {
            const float dy = kf.translation.y - baseY;
            kf.translation.x = wp.x;
            kf.translation.y = unitY + dy;
            kf.translation.z = wp.z;
        }
        u->node()->setClip(clip);
    }
    u->node()->setLocal(math::Mat4::translation(math::Vec3(wp.x, unitY, wp.z))
                       * math::Mat4::scale(math::Vec3(0.56f)));
}

void saveGame() {
    if (d().savePath.empty() || d().registry->empty() || !d().turnManager) return;
    SaveData data = {};
    data.magic     = kSaveMagic;
    data.version   = kSaveVersion;
    data.turnIndex = d().turnManager->turnIndex();
    data.turnElapsed = d().turnManager->elapsed();
    int idx = 0;
    for (size_t i = 0; i < d().registry->size(); ++i) {
        if (idx >= kMaxSavedUnits) break;
        auto* c = d().registry->unitAt(i);
        if (!c) continue;
        SavedUnit& su = data.units[idx++];
        su.faction = static_cast<int32_t>(c->faction());
        su.hp = c->hp();
        su.cellX = c->cell().x;
        su.cellY = c->cell().y;
        const auto& sv = c->statuses();
        su.statusCount = static_cast<int32_t>(sv.size() < 4 ? sv.size() : 4);
        for (int j = 0; j < su.statusCount; ++j)
            su.statuses[j] = {static_cast<int32_t>(sv[j].type), sv[j].duration, sv[j].magnitude};
    }
    data.unitCount = static_cast<uint32_t>(idx);
    if (save::writeBytes(d().savePath.c_str(), &data, sizeof(data)))
        aout << "[save] saved " << data.unitCount << " units to " << d().savePath << std::endl;
    else
        aout << "[save] FAILED to write " << d().savePath << std::endl;
}

void loadGame(Renderer& r) {
    if (d().savePath.empty() || d().registry->empty() || !d().turnManager || !d().grid) return;
    SaveData data = {};
    if (!save::readBytes(d().savePath.c_str(), &data, sizeof(data))) {
        aout << "[save] read failed: " << d().savePath << std::endl;
        return;
    }
    if (data.magic != kSaveMagic || data.version != kSaveVersion) {
        aout << "[save] bad magic/version, ignoring" << std::endl;
        return;
    }
    if (data.unitCount == 0 || data.unitCount > kMaxSavedUnits) {
        aout << "[save] bad unit count " << data.unitCount << std::endl;
        return;
    }
    auto applyChar = [](game::Unit* c, const SavedUnit& su) {
        if (c == nullptr) return;
        int diff = su.hp - c->hp();
        if (diff < 0) c->takeDamage(-diff);
        else if (diff > 0) c->heal(diff);
        c->setCell(math::Vec2(su.cellX, su.cellY));
        placeUnitAtCell(c);
        c->clearStatuses();
        for (int i = 0; i < su.statusCount && i < 4; ++i)
            c->applyStatus({static_cast<unit::StatusType>(su.statuses[i].type),
                            su.statuses[i].duration, su.statuses[i].magnitude});
    };
    int n = static_cast<int>(d().registry->size());
    if (static_cast<uint32_t>(n) > data.unitCount) n = static_cast<int>(data.unitCount);
    for (int i = 0; i < n; ++i)
        applyChar(d().registry->unitAt(i), data.units[i]);
    d().turnManager->restore(data.turnIndex, data.turnElapsed);
    d().lastTurnUnit = d().turnManager->currentUnit();
    setPathOverlay({});
    aout << "[save] loaded " << data.unitCount << " units" << std::endl;
}

void createHUD(Renderer& r, game::GameState& gs) {
    d().ui = std::make_unique<ui::UIRoot>();
    d().ui->init();
    const int w = r.width()  > 0 ? r.width()  : 1080;
    const int h = r.height() > 0 ? r.height() : 1920;
    d().ui->setScreenSize(w, h);

    Renderer* rptr = &r;

    auto root = std::make_unique<ui::UIPanel>(math::Vec2(0,0), math::Vec2(0,0),
                                              math::Vec4(0,0,0,0));

    auto banner = std::make_unique<ui::UIText>(math::Vec2(0,0), "PLAYER TURN",
                                               math::Vec4(1,1,1,1), 5);
    d().turnBanner = banner.get();
    root->addChild(std::move(banner));

    auto fps = std::make_unique<ui::UIText>(math::Vec2(0,0), "FPS:--",
                                            math::Vec4(0.5f,1.0f,0.5f,1.0f), 3);
    d().fpsText = fps.get();
    root->addChild(std::move(fps));

    auto dbg = std::make_unique<ui::UIText>(math::Vec2(0,0), "anim:--- ik:---",
                                            math::Vec4(0.5f,0.9f,1.0f,1.0f), 2);
    d().debugText = dbg.get();
    root->addChild(std::move(dbg));

    auto zoomIn = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonSize), static_cast<float>(kButtonSize)),
        "ZOOM+", [rptr]() { aout << "[ui] zoom in" << std::endl; rptr->zoomBy(1.2f); });
    zoomIn->setTextScale(3);
    d().zoomInBtn = zoomIn.get();
    root->addChild(std::move(zoomIn));

    auto zoomOut = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonSize), static_cast<float>(kButtonSize)),
        "ZOOM-", [rptr]() { aout << "[ui] zoom out" << std::endl; rptr->zoomBy(1.0f / 1.2f); });
    zoomOut->setTextScale(3);
    d().zoomOutBtn = zoomOut.get();
    root->addChild(std::move(zoomOut));

    auto reset = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonSize), static_cast<float>(kButtonSize)),
        "RESET", [rptr]() { aout << "[ui] reset view" << std::endl; rptr->resetView(); });
    reset->setTextScale(3);
    d().resetBtn = reset.get();
    root->addChild(std::move(reset));

    auto save = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        "SAVE", []() {
            aout << "[ui] save" << std::endl;
            saveGame();
        });
    save->setTextScale(3);
    d().saveBtn = save.get();
    root->addChild(std::move(save));

    auto exitBtn = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        "EXIT", [rptr]() { aout << "[ui] exit" << std::endl; rptr->finish(); });
    exitBtn->setTextScale(3);
    d().exitBtn = exitBtn.get();
    root->addChild(std::move(exitBtn));

    auto backBtn = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        "BACK", [rptr, &gs]() { aout << "[ui] back to title" << std::endl;
            rptr->sceneManager().replace(std::make_unique<TitleScene>(gs)); });
    backBtn->setTextScale(3);
    d().backBtn = backBtn.get();
    root->addChild(std::move(backBtn));

    auto timeLbl = std::make_unique<ui::UIText>(math::Vec2(0,0), "Y10000M12D30 23:59 PAUSED",
                                                math::Vec4(0.95f, 0.95f, 0.70f, 1.0f), 5);
    d().timeText = timeLbl.get();
    root->addChild(std::move(timeLbl));
    refreshTimeText();

    static const float kSpeeds[]    = {0.0f, 0.5f, 1.0f, 2.0f, 4.0f};
    static const char* kSpeedLabels[] = {"PAUSE", "0.5x", "1x", "2x", "4x"};
    auto pause = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kSpeedLabels[2], []() {
            d().speedPreset = (d().speedPreset + 1) % 5;
            const float s = kSpeeds[d().speedPreset];
            if (s <= 0.0f) d().clock.pause();
            else           d().clock.resume();
            d().clock.setTimeScale(s > 0.0f ? s : 1.0f);
            if (d().pauseBtn) d().pauseBtn->setText(kSpeedLabels[d().speedPreset]);
            refreshTimeText();
            aout << "[time] " << kSpeedLabels[d().speedPreset] << std::endl;
        });
    pause->setTextScale(3);
    d().pauseBtn = pause.get();
    root->addChild(std::move(pause));

    auto plus1h = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        "+1H", []() {
            d().clock.addSeconds(3600.0);
            refreshTimeText();
            aout << "[time] +1h" << std::endl;
        });
    plus1h->setTextScale(3);
    d().plus1hBtn = plus1h.get();
    root->addChild(std::move(plus1h));

    auto dawn = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        "DAWN", []() {
            d().clock.setTimeOfDay(6, 0, 0);
            refreshTimeText();
            aout << "[time] dawn" << std::endl;
        });
    dawn->setTextScale(3);
    d().dawnBtn = dawn.get();
    root->addChild(std::move(dawn));

    static const engine::wx::Weather kWxList[] = {
        engine::wx::Weather::Clear, engine::wx::Weather::Rain, engine::wx::Weather::Storm};
    static const char* kWxLabels[] = {"CLR", "RAIN", "STORM"};
    auto wx = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kWxLabels[0], []() {
            d().wxPreset = (d().wxPreset + 1) % 3;
            int seed = static_cast<int>(d().clock.totalSeconds());
            d().climate.setWeather(kWxList[d().wxPreset], seed);
            if (d().wxBtn) d().wxBtn->setText(kWxLabels[d().wxPreset]);
            aout << "[atmos] wx=" << kWxLabels[d().wxPreset] << std::endl;
        });
    wx->setTextScale(3);
    d().wxBtn = wx.get();
    root->addChild(std::move(wx));

    auto cam = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        "CAMERA", []() {
            d().cameraPanelOpen = !d().cameraPanelOpen;
            if (d().cameraBtn) d().cameraBtn->setText(d().cameraPanelOpen ? "CAMERA:ON" : "CAMERA");
            aout << "[ui] camera panel " << (d().cameraPanelOpen ? "OPEN" : "CLOSED") << std::endl;
            if (d().ui) {
                const int w = d().ui->width();
                const int h = d().ui->height();
                layoutHUD(w, h);
            }
        });
    cam->setTextScale(3);
    d().cameraBtn = cam.get();
    root->addChild(std::move(cam));

    static const char* kOrbitLabels[] = {"ORBIT:ISO", "ORBIT:SIDE", "ORBIT:TOP"};
    static const float kOrbitPitch[]  = {35.0f, 15.0f, 70.0f};
    static const float kOrbitYaw[]    = {0.0f, 0.0f, 0.0f};
    auto camOrbit = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kOrbitLabels[0], [rptr]() {
            d().camOrbitPreset = (d().camOrbitPreset + 1) % 3;
            rptr->setOrbitSnap(kOrbitPitch[d().camOrbitPreset], kOrbitYaw[d().camOrbitPreset]);
            if (d().camOrbitBtn) d().camOrbitBtn->setText(kOrbitLabels[d().camOrbitPreset]);
            aout << "[ui] orbit preset=" << kOrbitLabels[d().camOrbitPreset] << std::endl;
        });
    camOrbit->setTextScale(3);
    d().camOrbitBtn = camOrbit.get();
    root->addChild(std::move(camOrbit));

    static const char* kFovLabels[] = {"FOV:30", "FOV:45", "FOV:60"};
    static const float kFovDeg[]   = {30.0f, 45.0f, 60.0f};
    auto camFov = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kFovLabels[d().camFovPreset], [rptr]() {
            d().camFovPreset = (d().camFovPreset + 1) % 3;
            rptr->setFovYRadians(math::radians(kFovDeg[d().camFovPreset]));
            if (d().camFovBtn) d().camFovBtn->setText(kFovLabels[d().camFovPreset]);
            aout << "[ui] fov=" << kFovDeg[d().camFovPreset] << "deg" << std::endl;
        });
    camFov->setTextScale(3);
    d().camFovBtn = camFov.get();
    root->addChild(std::move(camFov));

    static const char* kPanLabels[] = {"PAN:LO", "PAN:MED", "PAN:HI"};
    static const float kPanVal[]    = {0.5f, 1.0f, 2.0f};
    auto camSens = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kPanLabels[d().camSensPreset], [rptr]() {
            d().camSensPreset = (d().camSensPreset + 1) % 3;
            rptr->setPanSensitivity(kPanVal[d().camSensPreset]);
            if (d().camSensBtn) d().camSensBtn->setText(kPanLabels[d().camSensPreset]);
            aout << "[ui] pan_sens=" << kPanVal[d().camSensPreset] << std::endl;
        });
    camSens->setTextScale(3);
    d().camSensBtn = camSens.get();
    root->addChild(std::move(camSens));

    auto audio = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        "AUDIO", []() {
            d().audioPanelOpen = !d().audioPanelOpen;
            if (d().audioBtn) d().audioBtn->setText(d().audioPanelOpen ? "AUDIO:ON" : "AUDIO");
            aout << "[ui] audio panel " << (d().audioPanelOpen ? "OPEN" : "CLOSED") << std::endl;
            if (d().ui) {
                const int w = d().ui->width();
                const int h = d().ui->height();
                layoutHUD(w, h);
            }
        });
    audio->setTextScale(3);
    d().audioBtn = audio.get();
    root->addChild(std::move(audio));

    static const char* kVolLabels[] = {"VOL:0%", "VOL:25%", "VOL:50%", "VOL:75%", "VOL:100%"};
    static const float kVolVal[]    = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    auto master = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kVolLabels[d().masterPreset], [rptr]() {
            d().masterPreset = (d().masterPreset + 1) % 5;
            rptr->setMasterVolume(kVolVal[d().masterPreset]);
            if (d().masterBtn) d().masterBtn->setText(kVolLabels[d().masterPreset]);
            aout << "[ui] master=" << kVolVal[d().masterPreset] << std::endl;
        });
    master->setTextScale(3);
    d().masterBtn = master.get();
    root->addChild(std::move(master));

    auto music = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kVolLabels[d().musicPreset], [rptr]() {
            d().musicPreset = (d().musicPreset + 1) % 5;
            rptr->setMusicVolume(kVolVal[d().musicPreset]);
            if (d().musicBtn) d().musicBtn->setText(kVolLabels[d().musicPreset]);
            aout << "[ui] music=" << kVolVal[d().musicPreset] << std::endl;
        });
    music->setTextScale(3);
    d().musicBtn = music.get();
    root->addChild(std::move(music));

    auto sfx = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kVolLabels[d().sfxPreset], [rptr]() {
            d().sfxPreset = (d().sfxPreset + 1) % 5;
            rptr->setSfxVolume(kVolVal[d().sfxPreset]);
            if (d().sfxBtn) d().sfxBtn->setText(kVolLabels[d().sfxPreset]);
            aout << "[ui] sfx=" << kVolVal[d().sfxPreset] << std::endl;
        });
    sfx->setTextScale(3);
    d().sfxBtn = sfx.get();
    root->addChild(std::move(sfx));

    auto playSfx = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        "PLAY SFX", [rptr]() {
            rptr->playClickSfx();
            aout << "[ui] play click sfx (vol=" << rptr->sfxVolume() << ")" << std::endl;
        });
    playSfx->setTextScale(3);
    d().playSfxBtn = playSfx.get();
    root->addChild(std::move(playSfx));

    auto particles = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        "PARTICLES", []() {
            d().particlesPanelOpen = !d().particlesPanelOpen;
            if (d().particlesBtn) d().particlesBtn->setText(d().particlesPanelOpen ? "PARTICLES:ON" : "PARTICLES");
            aout << "[ui] particles panel " << (d().particlesPanelOpen ? "OPEN" : "CLOSED") << std::endl;
            if (d().ui) {
                const int w = d().ui->width();
                const int h = d().ui->height();
                layoutHUD(w, h);
            }
        });
    particles->setTextScale(3);
    d().particlesBtn = particles.get();
    root->addChild(std::move(particles));

    static const char* kGravLabels[] = {"GRAV:0", "GRAV:-1", "GRAV:-2", "GRAV:-4", "GRAV:-8"};
    static const float kGravVal[]    = {0.0f, -1.0f, -2.0f, -4.0f, -8.0f};
    auto partGravity = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kGravLabels[d().partGravityPreset], []() {
            d().partGravityPreset = (d().partGravityPreset + 1) % 5;
            d().particleEmitter.setGravity(math::Vec3(0.0f, kGravVal[d().partGravityPreset], 0.0f));
            if (d().partGravityBtn) d().partGravityBtn->setText(kGravLabels[d().partGravityPreset]);
            aout << "[ui] gravity=" << kGravVal[d().partGravityPreset] << std::endl;
        });
    partGravity->setTextScale(3);
    d().partGravityBtn = partGravity.get();
    root->addChild(std::move(partGravity));

    static const char* kUpVelLabels[] = {"UPV:0.5", "UPV:1.0", "UPV:1.5", "UPV:2.0", "UPV:3.0"};
    static const float kUpVelVal[]    = {0.5f, 1.0f, 1.5f, 2.0f, 3.0f};
    auto partUpVel = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kUpVelLabels[d().partUpVelPreset], []() {
            d().partUpVelPreset = (d().partUpVelPreset + 1) % 5;
            d().particleEmitter.setUpVelocityMult(kUpVelVal[d().partUpVelPreset]);
            if (d().partUpVelBtn) d().partUpVelBtn->setText(kUpVelLabels[d().partUpVelPreset]);
            aout << "[ui] upVel=" << kUpVelVal[d().partUpVelPreset] << std::endl;
        });
    partUpVel->setTextScale(3);
    d().partUpVelBtn = partUpVel.get();
    root->addChild(std::move(partUpVel));

    static const char* kShrinkLabels[] = {"SHRINK:0.2", "SHRINK:0.5", "SHRINK:0.8", "SHRINK:1.5"};
    static const float kShrinkVal[]    = {0.2f, 0.5f, 0.8f, 1.5f};
    auto partShrink = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kShrinkLabels[d().partShrinkPreset], []() {
            d().partShrinkPreset = (d().partShrinkPreset + 1) % 4;
            d().particleEmitter.setSizeShrinkFactor(kShrinkVal[d().partShrinkPreset]);
            if (d().partShrinkBtn) d().partShrinkBtn->setText(kShrinkLabels[d().partShrinkPreset]);
            aout << "[ui] shrink=" << kShrinkVal[d().partShrinkPreset] << std::endl;
        });
    partShrink->setTextScale(3);
    d().partShrinkBtn = partShrink.get();
    root->addChild(std::move(partShrink));

    auto partBurst = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        "BURST!", []() {
            d().particleEmitter.burst(
                math::Vec3(0, 0.5f, 0), 16, 2.5f,
                math::Vec4(0.4f, 0.9f, 1.0f, 1.0f), 0.4f, 1.5f);
            d().burstTimer = 0.0f;
            aout << "[ui] manual burst (16 particles)" << std::endl;
        });
    partBurst->setTextScale(3);
    d().partBurstBtn = partBurst.get();
    root->addChild(std::move(partBurst));

    auto sky = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        "SKY", []() {
            d().skyPanelOpen = !d().skyPanelOpen;
            if (d().skyBtn) d().skyBtn->setText(d().skyPanelOpen ? "SKY:ON" : "SKY");
            aout << "[ui] sky panel " << (d().skyPanelOpen ? "OPEN" : "CLOSED") << std::endl;
            if (d().ui) {
                const int w = d().ui->width();
                const int h = d().ui->height();
                layoutHUD(w, h);
            }
        });
    sky->setTextScale(3);
    d().skyBtn = sky.get();
    root->addChild(std::move(sky));

    static const char* kSkyNightLabels[] = {"NIGHT:2", "NIGHT:3", "NIGHT:4", "NIGHT:6", "NIGHT:8"};
    static const float kSkyNightVal[]    = {2.0f, 3.0f, 4.0f, 6.0f, 8.0f};
    auto skyNight = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kSkyNightLabels[d().skyNightPreset], [rptr]() {
            d().skyNightPreset = (d().skyNightPreset + 1) % 5;
            rptr->setSkyNightBlendScale(kSkyNightVal[d().skyNightPreset]);
            if (d().skyNightBtn) d().skyNightBtn->setText(kSkyNightLabels[d().skyNightPreset]);
            aout << "[ui] skyNightScale=" << kSkyNightVal[d().skyNightPreset] << std::endl;
        });
    skyNight->setTextScale(3);
    d().skyNightBtn = skyNight.get();
    root->addChild(std::move(skyNight));

    static const char* kSkyDawnLabels[] = {"DAWN:0.15", "DAWN:0.25", "DAWN:0.30", "DAWN:0.40", "DAWN:0.50"};
    static const float kSkyDawnVal[]    = {0.15f, 0.25f, 0.30f, 0.40f, 0.50f};
    auto skyDawn = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kSkyDawnLabels[d().skyDawnPreset], [rptr]() {
            d().skyDawnPreset = (d().skyDawnPreset + 1) % 5;
            rptr->setSkyDawnWidth(kSkyDawnVal[d().skyDawnPreset]);
            if (d().skyDawnBtn) d().skyDawnBtn->setText(kSkyDawnLabels[d().skyDawnPreset]);
            aout << "[ui] skyDawnWidth=" << kSkyDawnVal[d().skyDawnPreset] << std::endl;
        });
    skyDawn->setTextScale(3);
    d().skyDawnBtn = skyDawn.get();
    root->addChild(std::move(skyDawn));

    static const char* kSkyNoonLabels[] = {"NOON:1.0", "NOON:1.5", "NOON:2.0", "NOON:3.0", "NOON:4.0"};
    static const float kSkyNoonVal[]    = {1.0f, 1.5f, 2.0f, 3.0f, 4.0f};
    auto skyNoon = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kSkyNoonLabels[d().skyNoonPreset], [rptr]() {
            d().skyNoonPreset = (d().skyNoonPreset + 1) % 5;
            rptr->setSkyNoonScale(kSkyNoonVal[d().skyNoonPreset]);
            if (d().skyNoonBtn) d().skyNoonBtn->setText(kSkyNoonLabels[d().skyNoonPreset]);
            aout << "[ui] skyNoonScale=" << kSkyNoonVal[d().skyNoonPreset] << std::endl;
        });
    skyNoon->setTextScale(3);
    d().skyNoonBtn = skyNoon.get();
    root->addChild(std::move(skyNoon));

    static const char* kSkyAmbientLabels[] = {"AMBIENT:0.1", "AMBIENT:0.2", "AMBIENT:0.3", "AMBIENT:0.4", "AMBIENT:0.5"};
    static const float kSkyAmbientVal[]    = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    auto skyAmbient = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kSkyAmbientLabels[d().skyAmbientPreset], [rptr]() {
            d().skyAmbientPreset = (d().skyAmbientPreset + 1) % 5;
            rptr->setSkyAmbientBaseline(kSkyAmbientVal[d().skyAmbientPreset]);
            if (d().skyAmbientBtn) d().skyAmbientBtn->setText(kSkyAmbientLabels[d().skyAmbientPreset]);
            aout << "[ui] skyAmbientBaseline=" << kSkyAmbientVal[d().skyAmbientPreset] << std::endl;
        });
    skyAmbient->setTextScale(3);
    d().skyAmbientBtn = skyAmbient.get();
    root->addChild(std::move(skyAmbient));

    auto weather = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        "WEATHER", []() {
            d().weatherPanelOpen = !d().weatherPanelOpen;
            if (d().weatherBtn) d().weatherBtn->setText(d().weatherPanelOpen ? "WEATHER:ON" : "WEATHER");
            aout << "[ui] weather panel " << (d().weatherPanelOpen ? "OPEN" : "CLOSED") << std::endl;
            if (d().ui) {
                const int w = d().ui->width();
                const int h = d().ui->height();
                layoutHUD(w, h);
            }
        });
    weather->setTextScale(3);
    d().weatherBtn = weather.get();
    root->addChild(std::move(weather));

    static const char* kWxFlashLabels[] = {"FLASH:0.5", "FLASH:0.75", "FLASH:1.0", "FLASH:1.5", "FLASH:2.0"};
    static const float kWxFlashVal[]    = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f};
    auto wxFlash = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kWxFlashLabels[d().wxFlashPreset], []() {
            d().wxFlashPreset = (d().wxFlashPreset + 1) % 5;
            d().climate.lightning.peakIntensity = kWxFlashVal[d().wxFlashPreset];
            if (d().wxFlashBtn) d().wxFlashBtn->setText(kWxFlashLabels[d().wxFlashPreset]);
            aout << "[ui] wxPeakIntensity=" << kWxFlashVal[d().wxFlashPreset] << std::endl;
        });
    wxFlash->setTextScale(3);
    d().wxFlashBtn = wxFlash.get();
    root->addChild(std::move(wxFlash));

    static const char* kWxDecayLabels[] = {"DECAY:2", "DECAY:4", "DECAY:6", "DECAY:10", "DECAY:16"};
    static const float kWxDecayVal[]    = {2.0f, 4.0f, 6.0f, 10.0f, 16.0f};
    auto wxDecay = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kWxDecayLabels[d().wxDecayPreset], []() {
            d().wxDecayPreset = (d().wxDecayPreset + 1) % 5;
            d().climate.lightning.flashDecay = kWxDecayVal[d().wxDecayPreset];
            if (d().wxDecayBtn) d().wxDecayBtn->setText(kWxDecayLabels[d().wxDecayPreset]);
            aout << "[ui] wxFlashDecay=" << kWxDecayVal[d().wxDecayPreset] << std::endl;
        });
    wxDecay->setTextScale(3);
    d().wxDecayBtn = wxDecay.get();
    root->addChild(std::move(wxDecay));

    static const char* kWxIntervalLabels[] = {"INTERVAL:1", "INTERVAL:2", "INTERVAL:3", "INTERVAL:5", "INTERVAL:8"};
    static const float kWxIntervalVal[]    = {1.0f, 2.0f, 3.0f, 5.0f, 8.0f};
    auto wxInterval = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kWxIntervalLabels[d().wxIntervalPreset], []() {
            d().wxIntervalPreset = (d().wxIntervalPreset + 1) % 5;
            d().climate.lightning.baseDelay = kWxIntervalVal[d().wxIntervalPreset];
            d().climate.lightning.nextDelay = -1.0f;
            d().climate.lightning.timer     = 0.0f;
            if (d().wxIntervalBtn) d().wxIntervalBtn->setText(kWxIntervalLabels[d().wxIntervalPreset]);
            aout << "[ui] wxBaseDelay=" << kWxIntervalVal[d().wxIntervalPreset] << "s" << std::endl;
        });
    wxInterval->setTextScale(3);
    d().wxIntervalBtn = wxInterval.get();
    root->addChild(std::move(wxInterval));

    static const char* kWxJitterLabels[] = {"JITTER:0", "JITTER:2", "JITTER:7", "JITTER:12", "JITTER:20"};
    static const float kWxJitterVal[]    = {0.0f, 2.0f, 7.0f, 12.0f, 20.0f};
    auto wxJitter = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kWxJitterLabels[d().wxJitterPreset], []() {
            d().wxJitterPreset = (d().wxJitterPreset + 1) % 5;
            d().climate.lightning.jitterRange = kWxJitterVal[d().wxJitterPreset];
            d().climate.lightning.nextDelay = -1.0f;
            d().climate.lightning.timer     = 0.0f;
            if (d().wxJitterBtn) d().wxJitterBtn->setText(kWxJitterLabels[d().wxJitterPreset]);
            aout << "[ui] wxJitterRange=" << kWxJitterVal[d().wxJitterPreset] << "s" << std::endl;
        });
    wxJitter->setTextScale(3);
    d().wxJitterBtn = wxJitter.get();
    root->addChild(std::move(wxJitter));

    static const char* kLandLabels[] = {"LAND:FLAT", "LAND:HILL", "LAND:VALLEY", "LAND:RIDGE", "LAND:CLIFF"};
    auto land = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        kLandLabels[d().landscapeIdx], []() {
            d().landscapeIdx = (d().landscapeIdx + 1) % 5;
            if (d().grid && d().landscapeIdx < static_cast<int>(d().terrainHandles.size())) {
                d().terrainMeshPtr  = d().assets->mesh (d().terrainHandles[d().landscapeIdx]);
                d().terrainModelPtr = d().assets->model(d().terrainHandles[d().landscapeIdx]);
                d().grid->applyElevationFromMesh(*d().terrainMeshPtr);
                d().grid->reTranslateTiles(d().tileNodes);
                if (d().terrainNode && !d().terrainNode->drawables().empty()) {
                    auto& dr = d().terrainNode->drawables().front();
                    dr.mesh  = d().terrainMeshPtr;
                    dr.model = d().terrainModelPtr;
                }
                for (size_t i = 0; i < d().registry->size(); ++i) {
                    auto* c = d().registry->unitAt(i);
                    if (c) placeUnitAtCell(c);
                }
            }
            if (d().landBtn) d().landBtn->setText(kLandLabels[d().landscapeIdx]);
            aout << "[ui] landscape=" << kLandLabels[d().landscapeIdx]
                 << " (units re-keyframed to new cellY)" << std::endl;
        });
    land->setTextScale(3);
    d().landBtn = land.get();
    root->addChild(std::move(land));

    auto resetScene = std::make_unique<ui::UIButton>(math::Vec2(0,0),
        math::Vec2(static_cast<float>(kButtonW), static_cast<float>(kButtonH)),
        "RESET", [rptr, &gs]() {
            g_pendingLandscapeIdx = d().landscapeIdx;
            aout << "[ui] reset scene (landscape=" << g_pendingLandscapeIdx << ")" << std::endl;
            rptr->sceneManager().replace(std::make_unique<BattleScene>(gs));
        });
    resetScene->setTextScale(3);
    d().resetSceneBtn = resetScene.get();
    root->addChild(std::move(resetScene));

    d().ui->setRoot(std::move(root));
    layoutHUD(w, h);
}

void layoutHUD(int w, int h) {
    if (!d().ui) return;
    const float margin  = static_cast<float>(kButtonMargin);
    const float btnSize = static_cast<float>(kButtonSize);
    const float btnW    = static_cast<float>(kButtonW);
    const float btnH    = static_cast<float>(kButtonH);
    const float bannerH = static_cast<float>(kBannerHeight);

    if (d().turnBanner) {
        d().turnBanner->setPosition(math::Vec2(
            (static_cast<float>(w) - d().turnBanner->size().x) * 0.5f,
            margin));
    }
    if (d().fpsText) {
        d().fpsText->setPosition(math::Vec2(
            static_cast<float>(w) - margin - d().fpsText->size().x,
            margin));
    }
    if (d().debugText) {
        d().debugText->setPosition(math::Vec2(
            static_cast<float>(w) - margin - d().debugText->size().x,
            margin + d().fpsText->size().y + 4.0f));
    }
    float by = static_cast<float>(h) - margin - btnSize;
    (void)by; // ZOOM-/+RESET moved into the camera panel (right column)
    const float colGap  = btnW + 12.0f;
    const float rx      = static_cast<float>(w) - margin - btnW;
    const float lx      = rx - colGap;
    float ry = static_cast<float>(h) - margin - btnH;
    const float rightColGap = btnH + 12.0f;
    // Right column: toggle panels + action buttons
    if (d().skyBtn)       { d().skyBtn->setPosition(math::Vec2(rx, ry));       ry -= rightColGap; }
    if (d().particlesBtn){ d().particlesBtn->setPosition(math::Vec2(rx, ry)); ry -= rightColGap; }
    if (d().audioBtn)    { d().audioBtn->setPosition(math::Vec2(rx, ry));    ry -= rightColGap; }
    if (d().cameraBtn)   { d().cameraBtn->setPosition(math::Vec2(rx, ry));   ry -= rightColGap; }
    if (d().saveBtn)     { d().saveBtn->setPosition(math::Vec2(rx, ry));     ry -= rightColGap; }
    if (d().exitBtn)     { d().exitBtn->setPosition(math::Vec2(rx, ry));     ry -= rightColGap; }
    if (d().backBtn)     { d().backBtn->setPosition(math::Vec2(rx, ry));     ry -= rightColGap; }
    // Left column: environment + scene
    float ly = static_cast<float>(h) - margin - btnH;
    if (d().weatherBtn)  { d().weatherBtn->setPosition(math::Vec2(lx, ly));  ly -= rightColGap; }
    if (d().landBtn)     { d().landBtn->setPosition(math::Vec2(lx, ly));     ly -= rightColGap; }
    if (d().resetSceneBtn) { d().resetSceneBtn->setPosition(math::Vec2(lx, ly)); }
    if (d().timeText) {
        d().timeText->setPosition(math::Vec2(
            (static_cast<float>(w) - d().timeText->size().x) * 0.5f,
            margin + d().turnBanner->size().y + 4.0f));
    }
    {
        const float timeRowY = margin + d().turnBanner->size().y
                             + d().timeText->size().y + 12.0f;
        const float rowW = btnW * 4.0f + margin * 0.5f * 3.0f;
        const float rowX0 = (static_cast<float>(w) - rowW) * 0.5f;
        float bx = rowX0;
        if (d().pauseBtn)  { d().pauseBtn->setPosition(math::Vec2(bx, timeRowY));  bx += btnW + margin * 0.5f; }
        if (d().plus1hBtn) { d().plus1hBtn->setPosition(math::Vec2(bx, timeRowY)); bx += btnW + margin * 0.5f; }
        if (d().dawnBtn)   { d().dawnBtn->setPosition(math::Vec2(bx, timeRowY));   bx += btnW + margin * 0.5f; }
        if (d().wxBtn)     { d().wxBtn->setPosition(math::Vec2(bx, timeRowY));     }
    }
    {
        // Camera panel: 4 rows below the time row, hidden when closed.
        const float timeRowY = margin + d().turnBanner->size().y
                             + d().timeText->size().y + 12.0f;
        const float rowW   = btnW * 4.0f + margin * 0.5f * 3.0f;
        const float rowX0  = (static_cast<float>(w) - rowW) * 0.5f;
        const float panelY0 = d().cameraPanelOpen
            ? (timeRowY + btnH + 24.0f)
            : -1000.0f;
        const float slotGap = btnW + margin * 0.5f;
        // Row 1: ORBIT (col 0)
        if (d().camOrbitBtn) d().camOrbitBtn->setPosition(math::Vec2(rowX0, panelY0));
        // Row 2: ZOOM- / ZOOM+ / RESET (cols 0,1,2)
        if (d().zoomInBtn)   d().zoomInBtn->setPosition(math::Vec2(rowX0 + slotGap * 1.0f, panelY0 + btnH + 24.0f));
        if (d().zoomOutBtn)  d().zoomOutBtn->setPosition(math::Vec2(rowX0 + slotGap * 0.0f, panelY0 + btnH + 24.0f));
        if (d().resetBtn)    d().resetBtn->setPosition(math::Vec2(rowX0 + slotGap * 2.0f, panelY0 + btnH + 24.0f));
        // Row 3: FOV (col 0)
        if (d().camFovBtn)   d().camFovBtn->setPosition(math::Vec2(rowX0, panelY0 + (btnH + 24.0f) * 2.0f));
        // Row 4: SENS (col 0)
        if (d().camSensBtn)  d().camSensBtn->setPosition(math::Vec2(rowX0, panelY0 + (btnH + 24.0f) * 3.0f));
    }
    {
        // Audio panel: 4 rows below the time row, hidden when closed.
        // Each panel sits at the same Y when open; only one panel is open at a
        // time, so no overlap. This keeps all 4 panels on-screen regardless of
        // how many are below in the stack.
        const float timeRowY = margin + d().turnBanner->size().y
                             + d().timeText->size().y + 12.0f;
        const float rowW   = btnW * 4.0f + margin * 0.5f * 3.0f;
        const float rowX0  = (static_cast<float>(w) - rowW) * 0.5f;
        const float audioY0 = d().audioPanelOpen
            ? (timeRowY + btnH + 24.0f)
            : -1000.0f;
        const float rowGap = btnH + 24.0f;
        // Row 1: MASTER
        if (d().masterBtn)  d().masterBtn->setPosition(math::Vec2(rowX0, audioY0));
        // Row 2: MUSIC
        if (d().musicBtn)   d().musicBtn->setPosition(math::Vec2(rowX0, audioY0 + rowGap));
        // Row 3: SFX
        if (d().sfxBtn)     d().sfxBtn->setPosition(math::Vec2(rowX0, audioY0 + rowGap * 2.0f));
        // Row 4: PLAY SFX
        if (d().playSfxBtn) d().playSfxBtn->setPosition(math::Vec2(rowX0, audioY0 + rowGap * 3.0f));
    }
    {
        // Particles panel: 4 rows below the time row, hidden when closed.
        const float timeRowY = margin + d().turnBanner->size().y
                             + d().timeText->size().y + 12.0f;
        const float rowW   = btnW * 4.0f + margin * 0.5f * 3.0f;
        const float rowX0  = (static_cast<float>(w) - rowW) * 0.5f;
        const float partY0 = d().particlesPanelOpen
            ? (timeRowY + btnH + 24.0f)
            : -1000.0f;
        const float rowGap = btnH + 24.0f;
        // Row 1: GRAVITY
        if (d().partGravityBtn) d().partGravityBtn->setPosition(math::Vec2(rowX0, partY0));
        // Row 2: UP-VEL
        if (d().partUpVelBtn)   d().partUpVelBtn->setPosition(math::Vec2(rowX0, partY0 + rowGap));
        // Row 3: SHRINK
        if (d().partShrinkBtn)  d().partShrinkBtn->setPosition(math::Vec2(rowX0, partY0 + rowGap * 2.0f));
        // Row 4: BURST!
        if (d().partBurstBtn)   d().partBurstBtn->setPosition(math::Vec2(rowX0, partY0 + rowGap * 3.0f));
    }
    {
        // Sky panel: 4 rows below the time row, hidden when closed.
        const float timeRowY = margin + d().turnBanner->size().y
                             + d().timeText->size().y + 12.0f;
        const float rowW   = btnW * 4.0f + margin * 0.5f * 3.0f;
        const float rowX0  = (static_cast<float>(w) - rowW) * 0.5f;
        const float skyY0 = d().skyPanelOpen
            ? (timeRowY + btnH + 24.0f)
            : -1000.0f;
        const float rowGap = btnH + 24.0f;
        // Row 1: NIGHT
        if (d().skyNightBtn)   d().skyNightBtn->setPosition(math::Vec2(rowX0, skyY0));
        // Row 2: DAWN
        if (d().skyDawnBtn)    d().skyDawnBtn->setPosition(math::Vec2(rowX0, skyY0 + rowGap));
        // Row 3: NOON
        if (d().skyNoonBtn)    d().skyNoonBtn->setPosition(math::Vec2(rowX0, skyY0 + rowGap * 2.0f));
        // Row 4: AMBIENT
        if (d().skyAmbientBtn) d().skyAmbientBtn->setPosition(math::Vec2(rowX0, skyY0 + rowGap * 3.0f));
    }
    {
        // Weather panel: 4 rows below the time row, hidden when closed.
        const float timeRowY = margin + d().turnBanner->size().y
                             + d().timeText->size().y + 12.0f;
        const float rowW   = btnW * 4.0f + margin * 0.5f * 3.0f;
        const float rowX0  = (static_cast<float>(w) - rowW) * 0.5f;
        const float wxY0 = d().weatherPanelOpen
            ? (timeRowY + btnH + 24.0f)
            : -1000.0f;
        const float rowGap = btnH + 24.0f;
        // Row 1: FLASH (peak intensity)
        if (d().wxFlashBtn)    d().wxFlashBtn->setPosition(math::Vec2(rowX0, wxY0));
        // Row 2: DECAY (flashDecay)
        if (d().wxDecayBtn)    d().wxDecayBtn->setPosition(math::Vec2(rowX0, wxY0 + rowGap));
        // Row 3: INTERVAL (baseDelay)
        if (d().wxIntervalBtn) d().wxIntervalBtn->setPosition(math::Vec2(rowX0, wxY0 + rowGap * 2.0f));
        // Row 4: JITTER (jitterRange)
        if (d().wxJitterBtn)   d().wxJitterBtn->setPosition(math::Vec2(rowX0, wxY0 + rowGap * 3.0f));
    }
}

void createContent(Renderer& r, game::GameState& gs) {


    d().assets = std::make_unique<asset::Registry>();
    const asset::Handle hPlane = d().assets->addMesh("plane", primitives::makePlane(1.0f));
    d().hPlaneMesh = hPlane;

    {
        const Platform& amgr = r.platform();

        std::string litVS = shader::loadSource(amgr, "shaders/lit.vert");
        std::string litFS = shader::loadSource(amgr, "shaders/lit.frag");
        auto litShader = std::unique_ptr<Shader>(Shader::loadShader(litVS, litFS));
        assert(litShader);
        litShader->bindUniformBlock("FrameData", 0);
        const asset::Handle hLit = d().assets->addShader("lit", std::move(litShader));

        std::string skinVS = shader::loadSource(amgr, "shaders/skinned.vert");
        auto skinnedShader = std::unique_ptr<Shader>(Shader::loadShader(skinVS, litFS));
        assert(skinnedShader);
        skinnedShader->bindUniformBlock("FrameData", 0);
        const asset::Handle hSkinned = d().assets->addShader("skinned", std::move(skinnedShader));

        std::string unlitVS = shader::loadSource(amgr, "shaders/unlit.vert");
        std::string unlitFS = shader::loadSource(amgr, "shaders/unlit.frag");
        auto unlitShader = std::unique_ptr<Shader>(Shader::loadShader(unlitVS, unlitFS));
        assert(unlitShader);
        unlitShader->bindUniformBlock("FrameData", 0);
        const asset::Handle hUnlit = d().assets->addShader("unlit", std::move(unlitShader));

        const asset::Handle hLitMat     = d().assets->addMaterial("lit",     {d().assets->shader(hLit),     nullptr});
        const asset::Handle hSkinMat    = d().assets->addMaterial("skinned", {d().assets->shader(hSkinned), nullptr});
        const asset::Handle hUnlitMat   = d().assets->addMaterial("unlit",   {d().assets->shader(hUnlit),   nullptr});
        d().litMat   = d().assets->material(hLitMat);
        d().unlitMat = d().assets->material(hUnlitMat);
        d().skinMat  = d().assets->material(hSkinMat);

        // skeleton + idle clip loaded from unit_v2.glb in mesh-loading section below

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

        d().skinnedHitReaction.loop = false;
        d().skinnedHitReaction.tracks.push_back({0, {
            {0.0f,  math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)},
            {0.05f, math::Vec3(0.0f), math::Quat::identity(), math::Vec3(0.6f)},
            {0.15f, math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)}}});
        d().skinnedHitReaction.tracks.push_back({1, {
            {0.0f,  math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)}}});

        auto tex = TextureAsset::loadAsset(r.platform(), "android_robot.png");
        assert(tex);
        d().robotTex = tex;
        const asset::Handle hTexMat = d().assets->addMaterial("tex", {d().assets->shader(hLit), d().robotTex.get()});
        d().texMat = d().assets->material(hTexMat);
    }

    // Set up unit animation clips
    const math::Vec3 kUnitScale(1.0f);
    d().playerSpinClipTemplate.loop = true;
    d().playerSpinClipTemplate.keyframes = {
        {0.0f, math::Vec3(0.0f), math::Quat::identity(), kUnitScale},
        {1.0f, math::Vec3(0.0f), math::Quat::identity(), kUnitScale}};

    d().enemyBobClipTemplate.loop = true;
    d().enemyBobClipTemplate.keyframes = {
        {0.0f, math::Vec3(0.0f, 0.0f, 0.0f), math::Quat::identity(), kUnitScale},
        {1.0f, math::Vec3(0.0f, 0.3f, 0.0f), math::Quat::identity(), kUnitScale}};

    d().allyBobClipTemplate.loop = true;
    d().allyBobClipTemplate.keyframes = {
        {0.0f, math::Vec3(0.0f), math::Quat::identity(), kUnitScale},
        {2.0f, math::Vec3(0.0f, 0.2f, 0.0f), math::Quat::identity(), kUnitScale}};

    d().animeClipTemplate.loop = true;
    d().animeClipTemplate.keyframes = {
        {0.0f, math::Vec3(0.0f), math::Quat::identity(), kUnitScale},
        {1.0f, math::Vec3(0.0f), math::Quat::identity(), kUnitScale}};

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

    d().terrainHandles.clear();
    for (const auto& shape : kShapes) {
        Mesh hm = primitives::makeHeightmap(terrainW, terrainH, terrainCS, shape.yAt);
        d().terrainHandles.push_back(d().assets->addMesh(shape.label, std::move(hm)));
    }

    // Create cube mesh for unit rigs
    const asset::Handle hCube = d().assets->addMesh("cube", primitives::makeCube(1.0f));

    // Load robot1.glb
    {
        std::vector<uint8_t> assetBytes;
        const bool asset = r.platform().readAsset("robot1.glb", assetBytes);
        if (asset) {
            const uint8_t* bytes = assetBytes.data();
            size_t size = assetBytes.size();
            gltf::LoadResult glb;
            if (gltf::load(bytes, size, glb)) {
                size_t vcnt = glb.mesh.positions.size();
                size_t icnt = glb.mesh.indices.size();
                d().hRobotMesh = d().assets->addMesh("robot1", std::move(glb.mesh));
                aout << "[glb] loaded robot1.glb: "
                     << vcnt << " verts "
                     << icnt << " indices"
                     << std::endl;
            } else {
                aout << "[glb] failed to load robot1.glb" << std::endl;
            }
        } else {
            aout << "[glb] robot1.glb not found in assets" << std::endl;
        }
    }

    // Load giant.glb
    {
        std::vector<uint8_t> assetBytes;
        const bool asset = r.platform().readAsset("giant.glb", assetBytes);
        if (asset) {
            const uint8_t* bytes = assetBytes.data();
            size_t size = assetBytes.size();
            gltf::LoadResult glb;
            if (gltf::load(bytes, size, glb)) {
                size_t vcnt = glb.mesh.positions.size();
                size_t icnt = glb.mesh.indices.size();
                d().hGiantMesh = d().assets->addMesh("giant", std::move(glb.mesh));
                aout << "[glb] loaded giant.glb: "
                     << vcnt << " verts "
                     << icnt << " indices"
                     << std::endl;
            } else {
                aout << "[glb] failed to load giant.glb" << std::endl;
            }
        } else {
            aout << "[glb] giant.glb not found in assets" << std::endl;
        }
    }

    // Load unit_v2.glb — A2: real rigged mesh, 3-bone skeleton, idle+attack clips
    {
        std::vector<uint8_t> assetBytes;
        const bool asset = r.platform().readAsset("unit_v2.glb", assetBytes);
        if (asset) {
            const uint8_t* bytes = assetBytes.data();
            size_t size = assetBytes.size();
            gltf::LoadResult glb;
            if (gltf::load(bytes, size, glb)) {
                d().skinnedSkeleton = std::move(glb.skeleton);
                for (auto& [name, clip] : glb.clips)
                    if (name == "idle") d().skinnedClip = clip;
                d().hUnitV2Mesh = d().assets->addMesh("unit_v2", std::move(glb.mesh));
                aout << "[glb] unit_v2.glb: bones=" << d().skinnedSkeleton.bones.size()
                     << " clips=" << glb.clips.size() << std::endl;
            } else {
                aout << "[glb] failed to load unit_v2.glb" << std::endl;
            }
        } else {
            aout << "[glb] unit_v2.glb not found in assets" << std::endl;
        }
    }

    d().assets->upload(Shader::kPositionAttrib, Shader::kUVAttrib, Shader::kNormalAttrib,
                    Shader::kBoneIndicesAttrib, Shader::kBoneWeightsAttrib);

    d().cubeMesh  = d().assets->mesh(hCube);
    d().cubeModel = d().assets->model(hCube);
    d().robotMesh  = d().hRobotMesh != asset::kNull ? d().assets->mesh(d().hRobotMesh) : nullptr;
    d().robotModel = d().hRobotMesh != asset::kNull ? d().assets->model(d().hRobotMesh) : nullptr;
    d().giantMesh  = d().hGiantMesh != asset::kNull ? d().assets->mesh(d().hGiantMesh) : nullptr;
    d().giantModel = d().hGiantMesh != asset::kNull ? d().assets->model(d().hGiantMesh) : nullptr;
    d().unitV2Mesh  = d().hUnitV2Mesh != asset::kNull ? d().assets->mesh(d().hUnitV2Mesh)  : nullptr;
    d().unitV2Model = d().hUnitV2Mesh != asset::kNull ? d().assets->model(d().hUnitV2Mesh) : nullptr;

    const Mesh*  gridMesh   = d().assets->mesh(hPlane);
    const Model* planeModel = d().assets->model(hPlane);

    d().grid = std::make_unique<grid::BattleGrid>(lvl.width, lvl.height, lvl.cellSize);
    for (uint16_t y = 0; y < lvl.height; ++y) {
        for (uint16_t x = 0; x < lvl.width; ++x) {
            const size_t i = static_cast<size_t>(y) * lvl.width + x;
            if (lvl.terrain[i] == 0) d().grid->setPassable(x, y, false);
            d().grid->setCellY(x, y, lvl.cellY[i]);
        }
    }

    const int initialIdx = (g_pendingLandscapeIdx >= 0) ? g_pendingLandscapeIdx : 0;
    g_pendingLandscapeIdx = -1;
    d().landscapeIdx = initialIdx;
    d().terrainMeshPtr  = d().assets->mesh (d().terrainHandles[d().landscapeIdx]);
    d().terrainModelPtr = d().assets->model(d().terrainHandles[d().landscapeIdx]);
    d().grid->applyElevationFromMesh(*d().terrainMeshPtr);
    // d().terrainNode = d().grid->attachTerrainMesh(d().root.get(), d().terrainMeshPtr,
    //                                               d().terrainModelPtr, d().litMat);

    d().tileNodes = d().grid->attachTo(d().root.get(), gridMesh, planeModel,
                    math::Vec3(0.76f, 0.70f, 0.50f), math::Vec3(0.30f, 0.30f, 0.30f),
                    d().litMat);

    // Shadow root for all unit shadows
    d().shadowRoot_ = std::make_unique<SceneNode>();
    d().registry = std::make_unique<game::EntityRegistry>();

    // A3: populate unit catalog — stats/faction/colors are catalog data; mesh/placement are overrides
    {
        unit::UnitDef kd;
        kd.name = "Knight"; kd.faction = unit::Faction::Player;
        kd.baseStats = {15, 6, 4, 4, 1};
        kd.baseColor = {0.9f, 0.3f, 0.2f}; kd.selectedColor = {1.5f, 0.9f, 0.2f};
        d().unitCatalog.registerDef("knight", kd);

        unit::UnitDef md;
        md.name = "Mage"; md.faction = unit::Faction::Ally;
        md.baseStats = {12, 8, 2, 3, 2};
        md.baseColor = {0.3f, 0.5f, 0.9f}; md.selectedColor = {0.5f, 0.9f, 1.5f};
        d().unitCatalog.registerDef("mage", md);

        unit::UnitDef gd;
        gd.name = "Goblin"; gd.faction = unit::Faction::Enemy;
        gd.baseStats = {10, 5, 3, 3, 1};
        gd.baseColor = {0.2f, 0.7f, 0.2f}; gd.selectedColor = {1.0f, 0.2f, 0.2f};
        if (!d().skinnedSkeleton.bones.empty()) gd.skeleton = &d().skinnedSkeleton;
        d().unitCatalog.registerDef("goblin", gd);
    }

    {
        game::EntityBuilder eb(d().root.get(), d().grid.get(), d().shadowRoot_.get(),
                               &d().unitClips);

        eb.populate(*d().registry);

        if (!gs.empty()) gs.restoreTo(*d().registry);
    }

    d().turnManager = std::make_unique<turn::Manager>();
    d().turnManager->setUnits(charactersAll());

    d().savePath = r.dataPath() + "/battle.sav";
    if (save::fileExists(d().savePath.c_str())) loadGame(r);

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

} // namespace

void BattleScene::enter(Renderer& r) {
    createContent(r, gs_);
    r.setPickRoot(d().root.get());
    d().burstTimer = 5.0f;
    {
        particle::Emitter::Config rc;
        rc.gravity            = math::Vec3(0.0f, -8.0f, 0.0f);
        rc.upVelocityMult     = 0.0f;
        rc.sizeJitter         = 0.0f;
        rc.lifetimeJitterMin  = 0.6f;
        rc.lifetimeJitterRange = 0.4f;
        rc.sizeShrinkFactor   = 0.0f;
        d().rainEmitter.setConfig(rc);
    }
    d().palettes = game::defaultPalettes();
    d().clock.setDateTime(1, 1, 1, 6, 0, 0);
    d().clock.setTimeScale(1.0f);
    d().climate = game::ClimateState{};
    d().climate.clock = &d().clock;
    {
        std::vector<uint8_t> assetBytes;
        const bool asset = r.platform().readAsset("stars.bin", assetBytes);
        if (asset) {
            const uint8_t* bytes = assetBytes.data();
            const size_t   size  = assetBytes.size();
            if (engine::stars::loadStars(bytes, size, d().stars))
                aout << "[stars] loaded " << d().stars.stars.size() << " stars" << std::endl;
            else
                aout << "[stars] failed to parse stars.bin" << std::endl;
        } else {
            aout << "[stars] stars.bin not in assets" << std::endl;
        }
    }
    d().currentSun = engine::sky::computeSunState(
        d().climate.dayPhase(), d().palettes.sky,
        d().climate.current, d().palettes.weather,
        r.skyConfig(),
        d().climate.stars.skyAxis);
    r.setClearColor(d().currentSun.skyColor);
    r.setFrameLight(d().currentSun.lightDir, d().currentSun.ambient);
    createHUD(r, gs_);
    d().lastTurnUnit = d().turnManager ? d().turnManager->currentUnit() : nullptr;

    Renderer* rptr = &r;

    d().subs.push_back(EventBus::on<PointerDownEvent>([rptr](const PointerDownEvent& e) {
        if (d().ui) {
            if (ui::UIButton* btn = d().ui->hitTestButton(math::Vec2(e.x, e.y))) {
                btn->handleClick();
                rptr->cancelPan();
                return;
            }
        }
        const auto ray = rptr->pickingRay(e.x, e.y);
        const auto hit = rptr->pickAt(e.x, e.y);
        aout << "[pick] tap (" << e.x << ", " << e.y << ") ray origin=("
             << ray.origin.x << ", " << ray.origin.y << ", " << ray.origin.z
             << ") dir=(" << ray.direction.x << ", " << ray.direction.y
             << ", " << ray.direction.z << ")";
        if (hit.valid) {
            aout << " HIT index=" << hit.shapeIndex << " dist=" << hit.distance;
        } else {
            aout << " miss";
        }
        aout << std::endl;

        aout << "[pick] units=" << d().registry->size()
             << " alive=" << charactersAliveCount() << std::endl;
        game::Unit* picked = pickUnit(e.x, e.y, *rptr);
        game::Unit* sel = selectedUnit();

        if (picked != nullptr && picked != sel &&
            sel != nullptr &&
            sel->faction() == unit::Faction::Player &&
            picked->faction() == unit::Faction::Enemy &&
            isPlayerTurn()) {
            if (sel->isStunned()) {
                aout << "[action] stunned, cannot attack" << std::endl;
                return;
            }
            const math::Vec2 a = sel->cell();
            const math::Vec2 b = picked->cell();
            const int cheb = std::max(static_cast<int>(std::abs(a.x - b.x)),
                                      static_cast<int>(std::abs(a.y - b.y)));
            if (cheb > sel->stats().range) {
                aout << "[action] out of range (cheb=" << cheb
                     << " > range=" << sel->stats().range << ")" << std::endl;
                return;
            }
            if (!d().grid->hasLineOfSight(a, b)) {
                aout << "[action] LoS blocked from (" << a.x << "," << a.y
                     << ") to (" << b.x << "," << b.y << ")" << std::endl;
                return;
            }
            enqueueAttack(sel, picked);
            setPathOverlay({});
            aout << "[action] attack from (" << a.x << "," << a.y << ") to ("
                 << b.x << "," << b.y << ")" << std::endl;
            return;
        }

        if (picked != nullptr) {
            if (picked->isSelected()) {
                picked->setSelected(false);
            } else {
                for (size_t i = 0; i < d().registry->size(); ++i) {
                    auto* o = d().registry->unitAt(i);
                    if (o && o != picked) o->setSelected(false);
                }
                picked->setSelected(true);
                math::Vec3 worldPos = d().grid->cellToWorld(
                    static_cast<int>(picked->cell().x),
                    static_cast<int>(picked->cell().y));
                rptr->camera().setTarget(worldPos);
            }
            setPathOverlay({});
            aout << "[select] unit cell=(" << picked->cell().x << ", " << picked->cell().y
                 << ") faction=" << static_cast<int>(picked->faction())
                 << " selected=" << (picked->isSelected() ? "true" : "false") << std::endl;
            return;
        }

        if (sel != nullptr) {
            if (!isPlayerTurn()) {
                aout << "[path] not your turn" << std::endl;
                return;
            }
            if (sel->isStunned()) {
                aout << "[path] stunned, cannot move" << std::endl;
                return;
            }
            if (!d().grid) return;
            const math::Vec2 start = sel->cell();
            const math::Vec2 goal  = cellAtScreen(e.x, e.y, *rptr);
            aout << "[path] from (" << start.x << ", " << start.y << ") to ("
                 << goal.x << ", " << goal.y << ")" << std::endl;
            if (goal.x < 0.0f || goal.y < 0.0f) {
                aout << "[path] miss: tap is off the grid" << std::endl;
                return;
            }
            if (goal.x == start.x && goal.y == start.y) {
                aout << "[path] same cell, no path" << std::endl;
                setPathOverlay({});
                return;
            }
            if (d().actionManager && !d().actionManager->isIdle()) {
                aout << "[path] action in progress, ignored" << std::endl;
                return;
            }
            const auto path = path::findPath(*d().grid, start, goal);
            if (path.empty()) {
                aout << "[path] no path" << std::endl;
                setPathOverlay({});
                return;
            }
            aout << "[path] " << path.size() << " cells:";
            for (const auto& c : path) aout << " (" << c.x << "," << c.y << ")";
            aout << std::endl;
            setPathOverlay(path);
            enqueueMove(sel, path);
            sel->setSelected(false);
            return;
        }

        for (size_t i = 0; i < d().registry->size(); ++i) {
            auto* c = d().registry->unitAt(i);
            if (c) c->setSelected(false);
        }
        setPathOverlay({});
        aout << "[select] deselect all" << std::endl;
    }));
}

void BattleScene::exit(Renderer& r) {
    r.setPickRoot(nullptr);
    for (auto id : d().subs) EventBus::unsubscribe(id);
    if (d().registry && !d().registry->empty()) gs_.captureFrom(*d().registry);
    if (d().registry) d().registry->clear();
    d() = Data{};
}

void BattleScene::update(float dt, Renderer& r) {
    // Time semantics:
    //   gameDt = clock.gameDt(dt)  — respects pause + timeScale. Drives
    //     the in-world simulation: scene graph (character idle anims,
    //     leg IK targets), and anything that should freeze on pause.
    //   userDt = dt                 — real wall-clock. Drives
    //     player-facing things: turn timer, action duration budget,
    //     lightning flash, particles.
    // The Clock itself consumes userDt and applies timeScale internally
    // for its own tick — gameDt is for the rest of the world.
    const float userDt = dt;
    const float gameDt = d().clock.gameDt(dt);

    d().clock.tick(userDt);
    d().climate.update(userDt);
    d().climate.applyToRenderer(r);
    d().currentSun = engine::sky::computeSunState(
        d().climate.dayPhase(), d().palettes.sky,
        d().climate.current, d().palettes.weather,
        r.skyConfig(),
        d().climate.stars.skyAxis);
    r.setClearColor(d().currentSun.skyColor);
    r.setFrameLight(d().currentSun.lightDir, d().currentSun.ambient);

    if (d().root)          d().root->update(gameDt);
    if (d().actionManager) d().actionManager->update(userDt);
    if (d().turnManager)   d().turnManager->update(userDt);

    // Camera follow during unit move
    {
        const bool isIdle = !d().actionManager || d().actionManager->isIdle();
        if (d().wasActionIdle_ && !isIdle) {
            d().preMoveCamTarget_ = r.camera().target();
            d().camReturnActive_  = false;
            d().camReturnTimer_   = 0.0f;
        }
        if (!isIdle && d().movingUnit_ && d().movingUnit_->node()) {
            const math::Mat4 w = d().movingUnit_->node()->world();
            r.camera().setTarget(math::Vec3(w.m[12], w.m[13], w.m[14]));
        }
        if (!d().wasActionIdle_ && isIdle) {
            d().preMoveCamTarget_.y = 0.5f;
            d().camReturnActive_ = true;
            d().camReturnTimer_  = 0.0f;
            d().movingUnit_      = nullptr;
        }
        if (d().camReturnActive_) {
            d().camReturnTimer_ += userDt;
            const float t = std::min(d().camReturnTimer_ / 0.5f, 1.0f);
            const math::Vec3 cur = r.camera().target();
            r.camera().setTarget(cur + (d().preMoveCamTarget_ - cur) * t);
            if (t >= 1.0f) d().camReturnActive_ = false;
        }
        d().wasActionIdle_ = isIdle;
    }

    {
        int maxBones = 0;
        for (size_t i = 0; i < d().registry->size(); ++i) {
            auto* c = d().registry->unitAt(i);
            if (c && c->node() && c->node()->skeleton()) {
                maxBones = std::max(maxBones,
                    static_cast<int>(c->node()->skeleton()->bones.size()));
            }
        }
        d().boneCount = maxBones;
    }

    {
        const float cycle = 3.0f;
        const float tNow  = d().animClock;
        const float tPrev = d().lastLoopLogTime;
        const int   cycleNow  = static_cast<int>(tNow  / cycle);
        const int   cyclePrev = static_cast<int>((tPrev < 0.0f ? 0.0f : tPrev) / cycle);
        if (cycleNow > cyclePrev && cyclePrev >= 0) {
            for (size_t i = 0; i < d().registry->size(); ++i) {
                auto* c = d().registry->unitAt(i);
                if (!c || !c->node() || !c->node()->skeleton()) continue;
                const auto& bm = c->node()->boneMatrices();
                if (bm.size() < 1) continue;
                const math::Mat4& m0 = bm[0];
                aout << "[anim] LOOP cycle=" << cycleNow
                     << " bone[0].T=(" << m0.m[12] << "," << m0.m[13] << "," << m0.m[14] << ")"
                     << " t=" << tNow << std::endl;
            }
            d().lastLoopLogTime = tNow;
        } else if (d().lastLoopLogTime < 0.0f) {
            d().lastLoopLogTime = tNow;
        }
    }

    d().animClock += gameDt;
    {
        if (d().legFollowPlayer) {
            game::Unit* player = nullptr;
            for (size_t i = 0; i < d().registry->size(); ++i) {
                auto* c = d().registry->unitAt(i);
                if (c && c->faction() == unit::Faction::Player && !c->isDead())
                    player = c;
            }
            if (player) {
                d().legCellX = static_cast<int>(player->cell().x);
                d().legCellY = static_cast<int>(player->cell().y);
            }
        }
        const int prevCellX = d().legCellX;
        const int prevCellY = d().legCellY;
        if (d().legAnimMode == 1 && !d().legFollowPlayer) {
            const float t = d().animClock;
            d().legCellX = static_cast<int>(5.0f + 3.0f * std::sin(t * (2.0f * math::PI) / 4.0f));
        }
        if ((d().legCellX != prevCellX || d().legCellY != prevCellY) && !d().legRigRoots.empty()) {
            const math::Vec3 newCellWorld = d().grid->cellToWorld(d().legCellX, d().legCellY);
            for (size_t i = 0; i < d().legRigRoots.size() && i < d().legRigOffsets.size(); ++i) {
                if (!d().legRigRoots[i]) continue;
                const auto& off = d().legRigOffsets[i];
                d().legRigRoots[i]->setLocal(math::Mat4::translation(
                    math::Vec3(newCellWorld.x + off.x,
                              newCellWorld.y + d().legHipY,
                              newCellWorld.z + off.z)));
            }
        }
    }

    if (!d().legRigHips.empty() && d().grid) {
        const math::Vec3 cellWorld = d().grid->cellToWorld(d().legCellX, d().legCellY);
        const float tgtY = (d().legAnimMode == 2) ? d().legTargetOverrideY : cellWorld.y;

        const auto tFrame0 = std::chrono::steady_clock::now();
        bool anyClamped = false;
        const size_t solveN = d().stressLegs ? d().legRigHips.size() : 1;
        for (size_t i = 0; i < solveN && i < d().legRigRoots.size() && i < d().legRigHips.size(); ++i) {
            SceneNode* root = d().legRigRoots[i];
            SceneNode* hip  = d().legRigHips[i];
            SceneNode* knee = d().legRigKnees[i];
            if (!root || !hip || !knee) continue;

            const math::Mat4 lw = root->world();
            const math::Vec3 legRootPos(lw.m[12], lw.m[13], lw.m[14]);
            const auto& off = d().legRigOffsets[i];
            const math::Vec3 targetWorld(cellWorld.x + off.x, tgtY, cellWorld.z + off.z);
            const math::Vec3 targetModel = targetWorld - legRootPos;

            if (d().legIKEnabled) {
                const auto rIK = engine::foot_ik::solveTwoBone(
                    math::Vec3(0, 0, 0),
                    d().legHipLen,
                    d().legKneeLen,
                    targetModel,
                    d().legIKHint);
                anyClamped = anyClamped || rIK.clamped;
                hip->setLocal(math::Mat4::rotation(rIK.hipRotation));
                knee->setLocal(math::Mat4::translation(math::Vec3(0, -d().legHipLen, 0))
                             * math::Mat4::rotation(rIK.kneeRotation));
            } else {
                hip->setLocal(math::Mat4::identity());
                knee->setLocal(math::Mat4::translation(math::Vec3(0, -d().legHipLen, 0)));
            }
        }
        const auto tFrame1 = std::chrono::steady_clock::now();
        d().ikSolveMs = std::chrono::duration<float, std::milli>(tFrame1 - tFrame0).count();
        d().ikClamped = anyClamped;
        if (d().legRigHips.size() > 0 && d().legHip == nullptr) {
            d().legHip  = d().legRigHips[0];
            d().legKnee = d().legRigKnees[0];
        }
    }

    game::Unit* cur = d().turnManager ? d().turnManager->currentUnit() : nullptr;
    if (cur != d().lastTurnUnit) {
        if (cur) {
            int oldHp = cur->hp();
            cur->tickStatuses();
            if (cur->hp() < oldHp && d().grid) {
                math::Vec3 pos = d().grid->cellToWorld(
                    static_cast<int>(cur->cell().x),
                    static_cast<int>(cur->cell().y));
                d().particleEmitter.burst(pos, 6, 1.5f,
                                          math::Vec4(0.2f, 0.9f, 0.2f, 0.8f),
                                          0.2f, 0.8f);
            }
        }
        d().lastTurnUnit = cur;
    }

    d().burstTimer += dt;
    if (d().burstTimer >= 5.0f) {
        d().burstTimer = 0.0f;
        d().particleEmitter.burst(
            math::Vec3(0, 0.5f, 0), 8, 2.0f,
            math::Vec4(1, 0.6f, 0.1f, 1), 0.3f, 1.5f);
    }

    d().particleEmitter.update(userDt);

    // X3: rain particles — emit continuously when Rain/Storm
    {
        const engine::wx::Weather wx = d().climate.current;
        if (wx == engine::wx::Weather::Rain || wx == engine::wx::Weather::Storm) {
            const float rate = (wx == engine::wx::Weather::Storm) ? 120.0f : 60.0f;
            d().rainAccum += rate * userDt;
            const int count = static_cast<int>(d().rainAccum);
            d().rainAccum -= static_cast<float>(count);
            const math::Vec3 camPos = r.camera().eye();
            const math::Vec4 rainColor(0.5f, 0.75f, 1.0f, 0.5f);
            for (int i = 0; i < count; ++i) {
                const math::Vec3 pos = engine::wx::rainSpawnPos(camPos);
                d().rainEmitter.emit(pos, math::Vec3(0.0f, -5.0f, 0.0f),
                                     rainColor, 0.04f, 1.0f);
            }
        } else {
            d().rainEmitter.clear();
            d().rainAccum = 0.0f;
        }
        d().rainEmitter.update(userDt);
    }

    {
        for (size_t i = 0; i < d().registry->size(); ++i) {
            SceneNode* sn = d().registry->shadowAt(i);
            auto* c = d().registry->unitAt(i);
            if (sn && c && c->node())
                game::EntityBuilder::syncShadowMirror(c->node(), sn);
        }
    }

    refreshSelectionOverlay(r);

    if (d().turnManager && d().turnBanner) {
        char label[40];
        const float dur = d().turnManager->currentDuration();
        const float elapsed = d().turnManager->elapsed();
        const float remain = dur - elapsed;
        const char* tod = "DAY";
        const float h = d().currentSun.sunHeight;
        if (h < -0.1f) tod = "NIGHT";
        else if (h < 0.2f) tod = "DAWN";
        else if (h > 0.8f) tod = "NOON";
        else if (d().currentSun.lightDir.x < -0.7f) tod = "DUSK";
        switch (d().turnManager->currentFaction()) {
            case unit::Faction::Player:
                snprintf(label, sizeof(label), "PLAYER  %s", tod);
                break;
            case unit::Faction::Enemy:
                snprintf(label, sizeof(label), "ENEMY %.1fs  %s", remain, tod);
                break;
            case unit::Faction::Ally:
                snprintf(label, sizeof(label), "ALLY %.1fs  %s", remain, tod);
                break;
            case unit::Faction::Neutral:
                snprintf(label, sizeof(label), "WAITING  %s", tod);
                break;
        }
        if (d().turnBanner->text() != label) d().turnBanner->setText(label);
    }

    d().fpsAccum += dt;
    d().fpsFrames++;
    if (d().fpsAccum >= 0.5f && d().fpsText) {
        int fps = static_cast<int>(d().fpsFrames / d().fpsAccum + 0.5f);
        char buf[32];
        snprintf(buf, sizeof(buf), "FPS:%d", fps);
        d().fpsText->setText(buf);

        if (d().timeText) refreshTimeText();

        if (d().debugText) {
            float evT = action::lastEventTime();
            char dbuf[64];
            snprintf(dbuf, sizeof(dbuf),
                     "anim:b=%d ev=%.3f  ik:N=%d ms=%.3f cl=%d",
                     d().boneCount, evT,
                     static_cast<int>(d().legRigHips.size()),
                     d().ikSolveMs, d().ikClamped ? 1 : 0);
            d().debugText->setText(dbuf);
        }

        d().fpsAccum = 0.0f;
        d().fpsFrames = 0;
    }
}

void BattleScene::render(Renderer& r) {
    r.renderSceneGraph(d().root.get());
    if (d().pathOverlay) r.renderSceneGraph(d().pathOverlay.get());
    if (d().selectionIndicator) r.renderSceneGraph(d().selectionIndicator.get());
    r.renderParticles(d().particleEmitter);
    r.renderParticles(d().rainEmitter);
    r.renderStars(d().stars, d().currentSun.starIntensity, d().climate.dayPhase());
    if (d().climate.current == engine::wx::Weather::Storm) {
        const engine::sky::WeatherPalette& wp = d().palettes.weather;
        r.renderLightningFlash(wp.lightningColor, d().climate.lightning.flashIntensity);
    }
    r.renderUI(d().ui.get());
}

void BattleScene::onResize(int w, int h, Renderer& r) {
    if (w < 1 || h < 1) return;
    r.camera().setPerspective(kCameraFovYRadians,
        static_cast<float>(w) / static_cast<float>(h),
        kCameraNearPlane, kCameraFarPlane);
    if (d().ui) d().ui->setScreenSize(w, h);
}

SceneNode* BattleScene::shadowRoot() const {
    return d().shadowRoot_.get();
}
