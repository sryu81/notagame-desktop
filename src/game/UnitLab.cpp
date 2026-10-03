#include "UnitLab.h"

#include <algorithm>
#include <cmath>

#include "ClipEvents.h"
#include "Engine.h"
#include "GameState.h"
#include "HubScene.h"
#include "Rules.h"
#include "Unit.h"
#include "UnitDef.h"

namespace {

constexpr float kFovY = 30.0f * math::PI / 180.0f;
constexpr float kNear = 0.1f;
constexpr float kFar  = 100.0f;
const math::Vec3 kEye(0.0f, 6.5f, 10.0f);
const math::Vec3 kLook(0.0f, 0.4f, 0.5f);
constexpr int   kGridSize = 8;
constexpr float kRigYawDeg = 180.0f;
const math::Vec2 kKnightCell(1.0f, 4.0f);
const math::Vec2 kKnightEnd(4.0f, 4.0f);
const math::Vec2 kGoblinCell(5.0f, 4.0f);
const math::Vec2 kGolemCell(1.0f, 2.0f);

const char* kSkinnedSpawn = "SKINNED SPAWN";
const char* kFacing       = "FACING";
const char* kNoStrayScale = "NO STRAY SCALE";
const char* kWalkInMove   = "WALK IN MOVE";
const char* kAttackChain  = "ATTACK CHAIN";
const char* kHurtReaction = "HURT REACTION";
const char* kDeath        = "DEATH";
const char* kCubeFallback = "CUBE RIG FALLBACK";

bool nearMat(const math::Mat4& a, const math::Mat4& b, float eps) {
    for (int i = 0; i < 16; ++i) if (std::abs(a.m[i] - b.m[i]) > eps) return false;
    return true;
}

bool unitScale(const math::Mat4& m) {
    for (int c = 0; c < 3; ++c) {
        const math::Vec3 axis(m.m[c * 4], m.m[c * 4 + 1], m.m[c * 4 + 2]);
        if (std::abs(axis.length() - 1.0f) > 1e-3f) return false;
    }
    return true;
}

}

UnitLab::UnitLab(game::GameState& gs) : gs_(gs) {}
UnitLab::~UnitLab() = default;

bool UnitLab::loadRig(Renderer& r) {
    std::vector<uint8_t> bytes;
    if (!r.platform().readAsset("test_rig.glb", bytes)) return false;
    gltf::LoadResult glb;
    if (!gltf::load(bytes.data(), bytes.size(), glb)) return false;
    skel_ = std::move(glb.skeleton);
    for (auto& [name, c] : glb.clips) {
        game::applyClipEvents(name, c);
        clips_[name] = std::move(c);
    }
    hRig_ = assets_->addMesh("test_rig", std::move(glb.mesh));
    return true;
}

const anim::SkeletalClip* UnitLab::clip(const char* name) const {
    auto it = clips_.find(name);
    return it == clips_.end() ? nullptr : &it->second;
}

void UnitLab::buildCatalog() {
    catalog_.clear();
    unit::ClipSet set{clip("Idle"), clip("Walk"), clip("Attack"), clip("Hurt"), clip("Death")};

    auto skinned = [&](const char* name, unit::Faction f, unit::Stats st, math::Vec3 color) {
        unit::UnitDef d;
        d.name = name;
        d.faction = f;
        d.baseStats = st;
        d.baseColor = color;
        if (rigLoaded_) {
            d.skeleton = &skel_;
            d.mesh = assets_->mesh(hRig_);
            d.model = assets_->model(hRig_);
            d.material = assets_->material("skinned");
            d.clips = set;
        }
        d.modelYawDeg = kRigYawDeg;
        return d;
    };
    catalog_.registerDef("knight", skinned("KNIGHT", unit::Faction::Player, {15, 6, 4, 4, 1},
                                           math::Vec3(0.85f, 0.55f, 0.30f)));
    catalog_.registerDef("goblin", skinned("GOBLIN", unit::Faction::Enemy, {10, 5, 3, 3, 1},
                                           math::Vec3(0.30f, 0.70f, 0.30f)));

    unit::UnitDef golem;
    golem.name = "GOLEM";
    golem.faction = unit::Faction::Neutral;
    golem.baseStats = {12, 4, 4, 2, 1};
    golem.baseColor = math::Vec3(0.55f, 0.55f, 0.65f);
    golem.baseScale = 0.56f;
    catalog_.registerDef("golem", golem);
}

void UnitLab::enter(Renderer& r) {
    r.setPickRoot(nullptr);
    assets_ = std::make_unique<game::LabAssets>(r.platform());
    rigLoaded_ = loadRig(r);
    assets_->upload();
    buildCatalog();
    buildScene();

    ui_ = std::make_unique<ui::UIRoot>();
    ui_->init();
    checks_ = std::make_unique<game::LabChecklist>(math::Vec2(16, 16));
    for (const char* row : {kSkinnedSpawn, kFacing, kNoStrayScale, kWalkInMove, kAttackChain,
                            kHurtReaction, kDeath, kCubeFallback})
        checks_->add(row);
    resetCamera(r);
    buildUI(r);
    restartAutoDrive();

    Renderer* rptr = &r;
    auto alive = alive_;
    UnitLab* self = this;
    subs_.push_back(EventBus::on<PointerDownEvent>([alive, self, rptr](const PointerDownEvent& e) {
        if (!*alive || !self->ui_) return;
        if (ui::UIButton* btn = self->ui_->hitTestButton(math::Vec2(e.x, e.y))) {
            btn->handleClick();
            rptr->cancelPan();
        }
    }));
}

void UnitLab::exit(Renderer& r) {
    *alive_ = false;
    for (auto id : subs_) EventBus::unsubscribe(id);
    subs_.clear();
    if (actions_) actions_->clear();
    actions_.reset();
    builder_.reset();
    ui_.reset();
    panel_.reset();
    checks_.reset();
    ring_.reset();
    root_.reset();
    grid_.reset();
    catalog_.clear();
    assets_.reset();
}

void UnitLab::buildScene() {
    root_ = std::make_unique<SceneNode>();
    grid_ = std::make_unique<grid::BattleGrid>(kGridSize, kGridSize, 1.0f);
    grid_->attachTo(root_.get(), assets_->mesh("plane"), assets_->model("plane"),
                    math::Vec3(0.62f, 0.66f, 0.52f), math::Vec3(0.30f, 0.30f, 0.30f),
                    assets_->material("lit"));
    unitsRoot_ = root_->addChild();

    ring_ = std::make_unique<SceneNode>();
    ring_->addDrawable({assets_->mesh("plane"), assets_->model("plane"),
                        math::Vec3(1.0f, 0.95f, 0.15f), assets_->material("unlit")});
    ring_->setLocal(math::Mat4::scale(math::Vec3(0.0f, 1.0f, 0.0f)));

    actions_ = std::make_unique<action::Manager>();
}

void UnitLab::spawnUnits() {
    if (actions_) actions_->clear();
    builder_.reset();
    unitsRoot_->clearChildren();
    builder_ = std::make_unique<game::EntityBuilder>(unitsRoot_, grid_.get());

    auto at = [this](const char* id, math::Vec2 cell) {
        game::SpawnDef s;
        s.cellX = static_cast<int>(cell.x);
        s.cellY = static_cast<int>(cell.y);
        s.mesh = assets_->mesh("cube");
        s.model = assets_->model("cube");
        s.material = assets_->material("lit");
        return builder_->spawnFromCatalog(catalog_, id, s);
    };
    knight_ = at("knight", kKnightCell);
    goblin_ = at("goblin", kGoblinCell);
    golem_ = at("golem", kGolemCell);
    const math::Mat4 faceWest = math::Mat4::rotation(
        math::Quat::fromAxisAngle(math::Vec3(0, 1, 0), math::PI * 0.5f));
    goblin_->node()->setLocal(goblin_->node()->local() * faceWest);
}

void UnitLab::resetCamera(Renderer& r) {
    Camera& cam = r.camera();
    cam.setPerspective(kFovY, cam.aspect(), kNear, kFar);
    cam.setTerrainHeightFn({});
    cam.setForward(math::Vec3(0, 0, -1));
    cam.setPitchLimits(Camera::kMinPitchDeg, Camera::kMaxPitchDeg);
    cam.setYawLimits(Camera::kMinYawDeg, Camera::kMaxYawDeg);
    cam.setDistanceLimits(4.0f, 20.0f, 11.9f);
    cam.setViewLookAt(kEye, kLook, math::Vec3(0, 1, 0));
    r.setClearColor(math::Vec3(0.40f, 0.62f, 0.85f));
    r.setFrameLight(math::Vec3(0.3f, 0.85f, 0.45f).normalized(), 0.35f);
}

void UnitLab::restartAutoDrive() {
    step_ = 0;
    stepT_ = 0.0f;
    scaleOk_ = true;
    sawWalk_ = false;
    sawAttack_ = false;
    lastHp_ = 0;
    hpChanges_ = 0;
    hitFrame_ = -1;
    hurtFrame_ = -1;
    ringShown_ = false;
    heldFrames_ = 0;
    deathPose_.clear();
    spawnUnits();
    if (checks_) checks_->reset();
}

void UnitLab::buildUI(Renderer& r) {
    const int w = r.width() > 0 ? r.width() : 1280;
    const int h = r.height() > 0 ? r.height() : 720;
    ui_->setScreenSize(w, h);
    const float rowH = std::clamp(static_cast<float>(h) / 18.0f, 36.0f, 72.0f);
    const float rowW = rowH * 5.5f;
    const int scale = rowH >= 56.0f ? 3 : 2;

    auto root = std::make_unique<ui::UIPanel>(math::Vec2(0, 0),
        math::Vec2(static_cast<float>(w), static_cast<float>(h)), math::Vec4(0, 0, 0, 0));

    Renderer* rptr = &r;
    game::GameState* gs = &gs_;
    panel_ = std::make_unique<game::DebugPanel>(
        math::Vec2(static_cast<float>(w) - 24.0f - rowW, 24.0f),
        math::Vec2(rowW, rowH), rowH * 0.15f, scale);
    panel_->addRow("AUTO", [this]() { return std::string(autoOn_ ? "ON" : "OFF"); },
                   [this]() { autoOn_ = !autoOn_; });
    panel_->addAction("RESTART CHECKS", [this]() { autoOn_ = true; restartAutoDrive(); });
    panel_->addAction("BACK", [rptr, gs]() {
        rptr->sceneManager().replace(std::make_unique<HubScene>(*gs));
    });
    root->addChild(panel_->build());
    root->addChild(checks_->build());
    ui_->setRoot(std::move(root));
}

void UnitLab::onResize(int w, int h, Renderer& r) {
    if (w < 1 || h < 1 || !ui_) return;
    r.camera().setPerspective(kFovY, static_cast<float>(w) / static_cast<float>(h), kNear, kFar);
    buildUI(r);
}

void UnitLab::updateRing() {
    const game::Unit* sel = nullptr;
    for (game::Unit* u : builder_->unitsAll())
        if (u->isSelected() && !u->isDead()) { sel = u; break; }
    if (!sel) {
        ring_->setLocal(math::Mat4::scale(math::Vec3(0.0f, 1.0f, 0.0f)));
        return;
    }
    const math::Vec3 p = grid_->cellToWorld(static_cast<int>(sel->cell().x),
                                            static_cast<int>(sel->cell().y));
    ring_->setLocal(math::Mat4::translation(math::Vec3(p.x, p.y + 0.003f, p.z)) *
                    math::Mat4::scale(math::Vec3(0.93f, 1.0f, 0.93f)));
}

void UnitLab::observe() {
    ++frame_;
    if (!knight_ || !goblin_) return;
    const SceneNode* kv = knight_->visualNode();
    const SceneNode* gv = goblin_->visualNode();
    if (step_ == 1 || step_ == 2) {
        if (!unitScale(kv->world())) scaleOk_ = false;
    }
    if (step_ == 1 && !actions_->isIdle() && kv->skeletalClip() == clip("Walk")) sawWalk_ = true;
    if (step_ == 2) {
        if (kv->skeletalClip() == clip("Attack")) sawAttack_ = true;
        if (goblin_->hp() != lastHp_) {
            ++hpChanges_;
            if (hitFrame_ < 0) hitFrame_ = frame_;
            lastHp_ = goblin_->hp();
        }
        if (hurtFrame_ < 0 && gv->skeletalClip() == clip("Hurt")) hurtFrame_ = frame_;
    }
    const anim::SkeletalClip* death = clip("Death");
    if (step_ == 3 && death && goblin_->isDead() && gv->skeletalClip() == death &&
        gv->time() >= anim::duration(*death) + 0.05f) {
        if (deathPose_.empty()) {
            deathPose_ = gv->boneMatrices();
        } else if (deathPose_.size() == gv->boneMatrices().size()) {
            bool same = true;
            for (size_t i = 0; i < deathPose_.size(); ++i)
                same = same && nearMat(deathPose_[i], gv->boneMatrices()[i], 1e-6f);
            if (same) ++heldFrames_;
        }
    }
}

void UnitLab::autoDrive(float dt) {
    stepT_ += dt;
    auto next = [this]() { ++step_; stepT_ = 0.0f; };
    switch (step_) {
        case 0: {
            if (stepT_ < 0.1f) break;
            bool spawnOk = rigLoaded_;
            for (game::Unit* u : {knight_, goblin_}) {
                const SceneNode* v = u->visualNode();
                spawnOk = spawnOk && u->isSkinned() && v != u->node() &&
                          v->skeleton()->bones.size() == 15 &&
                          v->skeletalClip() == clip("Idle") && v->hasBoneMatrices();
            }
            checks_->check(kSkinnedSpawn, spawnOk);
            checks_->check(kCubeFallback, golem_ && !golem_->isSkinned() &&
                                          golem_->visualNode() == golem_->node() &&
                                          golem_->node()->children().size() >= 6 &&
                                          unitScale(golem_->node()->world() *
                                                    math::Mat4::scale(math::Vec3(1.0f / 0.56f))));
            knight_->setSelected(true);
            actions_->enqueue(std::make_unique<action::MoveAction>(
                knight_, grid_.get(),
                std::vector<math::Vec2>{kKnightCell, {2, 4}, {3, 4}, kKnightEnd}, 0.4f));
            next();
            break;
        }
        case 1: {
            if (stepT_ < 0.2f || !actions_->isIdle()) break;
            const SceneNode* kv = knight_->visualNode();
            const math::Vec3 fwd = kv->world().transformDirection(math::Vec3(0, 0, 1)).normalized();
            const float angle = std::acos(std::clamp(fwd.dot(math::Vec3(1, 0, 0)), -1.0f, 1.0f));
            if (angle > math::radians(2.0f))
                aout << "[unitlab] facing off by " << angle * 180.0f / math::PI << " deg" << std::endl;
            checks_->check(kFacing, angle <= math::radians(2.0f));
            checks_->check(kWalkInMove, sawWalk_ && kv->skeletalClip() == clip("Idle") &&
                                        knight_->cell().x == kKnightEnd.x &&
                                        knight_->cell().y == kKnightEnd.y);
            lastHp_ = goblin_->hp();
            actions_->enqueue(std::make_unique<action::AttackAction>(
                knight_, goblin_, rules::damage(knight_->stats(), goblin_->stats())));
            next();
            break;
        }
        case 2: {
            if (stepT_ < 0.2f || !actions_->isIdle()) break;
            const int dmg = rules::damage(knight_->stats(), goblin_->stats());
            checks_->check(kNoStrayScale, scaleOk_ && unitScale(knight_->visualNode()->world()));
            checks_->check(kAttackChain, sawAttack_ && hpChanges_ == 1 &&
                                         goblin_->hp() == goblin_->maxHp() - dmg &&
                                         knight_->visualNode()->skeletalClip() == clip("Idle"));
            if (hurtFrame_ < 0 || hitFrame_ < 0 || hurtFrame_ - hitFrame_ > 1)
                aout << "[unitlab] hit frame " << hitFrame_ << " hurt frame " << hurtFrame_ << std::endl;
            checks_->check(kHurtReaction, hitFrame_ >= 0 && hurtFrame_ >= hitFrame_ &&
                                          hurtFrame_ - hitFrame_ <= 1);
            knight_->setSelected(false);
            goblin_->setSelected(true);
            updateRing();
            ringShown_ = ring_->local().m[0] > 0.0f;
            actions_->enqueue(std::make_unique<action::AttackAction>(knight_, goblin_, goblin_->hp()));
            next();
            break;
        }
        case 3:
            if (stepT_ < 1.3f || !actions_->isIdle()) break;
            checks_->check(kDeath, ringShown_ && goblin_->isDead() && heldFrames_ >= 2 &&
                                   goblin_->visualNode()->skeletalClip() == clip("Death") &&
                                   !goblin_->pickable() && !goblin_->isSelected() &&
                                   ring_->local().m[0] == 0.0f);
            aout << "[unitlab] auto-drive done: " << checks_->passedCount() << "/"
                 << checks_->size() << " checks green" << std::endl;
            next();
            break;
        default:
            break;
    }
}

void UnitLab::update(float dt, Renderer& r) {
    if (root_) root_->update(dt);
    if (actions_) actions_->update(dt);
    if (builder_) builder_->updateAll(dt);
    updateRing();
    observe();
    if (autoOn_) autoDrive(dt);
    if (panel_) panel_->refresh();
}

void UnitLab::render(Renderer& r) {
    r.renderSceneGraph(root_.get());
    r.renderSceneGraph(ring_.get());
    r.renderUI(ui_.get());
}
