#include "ModelLab.h"

#include <algorithm>
#include <cmath>

#include "Engine.h"
#include "GameState.h"
#include "HubScene.h"
#include "LevelSpawner.h"
#include "Prop.h"
#include "Unit.h"
#include "UnitTypes.h"

namespace {

constexpr float kFovY = 30.0f * math::PI / 180.0f;
constexpr float kNear = 0.1f;
constexpr float kFar  = 100.0f;
const math::Vec3 kEye(0.0f, 6.5f, 11.0f);
const math::Vec3 kLook(0.0f, 0.4f, 0.0f);
constexpr int   kGridW = 13;
constexpr int   kGridH = 5;
constexpr int   kUnitRow = 1;
constexpr int   kPropRow = 3;
constexpr float kCellHalf = 0.52f;
constexpr float kTurnDegPerSec = 45.0f;

const char* kModelRow   = "OWN MODEL PER TYPE";
const char* kClipsRow   = "FIVE CLIPS";
const char* kBonesRow   = "BONES <= 48";
const char* kFitRow     = "FITS ONE CELL";
const char* kPaletteRow = "PALETTE TEXTURE";
const char* kPropsRow   = "PROPS MODELED";
const char* kPlayRow    = "ALL CLIPS PLAY";

bool fits(const Mesh* m, float scale) {
    if (!m || m->positions.empty()) return false;
    for (const math::Vec3& p : m->positions)
        if (std::fabs(p.x * scale) > kCellHalf || std::fabs(p.z * scale) > kCellHalf) return false;
    return true;
}

const anim::SkeletalClip* clipAt(const unit::ClipSet& c, int i) {
    switch (i) {
        case 0: return c.idle;
        case 1: return c.walk;
        case 2: return c.attack;
        case 3: return c.hurt;
        default: return c.death;
    }
}

}

ModelLab::ModelLab(game::GameState& gs) : gs_(gs) {}
ModelLab::~ModelLab() = default;

void ModelLab::enter(Renderer& r) {
    r.setPickRoot(nullptr);
    assets_ = std::make_unique<game::LabAssets>(r.platform());
    models_.load(r.platform(), *assets_);
    assets_->upload();
    models_.buildCatalog(catalog_, *assets_);
    root_ = std::make_unique<SceneNode>();

    ui_ = std::make_unique<ui::UIRoot>();
    ui_->init();
    checks_ = std::make_unique<game::LabChecklist>(math::Vec2(16, 16));
    for (const char* row : {kModelRow, kClipsRow, kBonesRow, kFitRow, kPaletteRow, kPropsRow, kPlayRow})
        checks_->add(row);
    resetCamera(r);
    restart();
    buildUI(r);

    Renderer* rptr = &r;
    auto alive = alive_;
    ModelLab* self = this;
    subs_.push_back(EventBus::on<PointerDownEvent>([alive, self, rptr](const PointerDownEvent& e) {
        if (!*alive || !self->ui_) return;
        if (ui::UIButton* btn = self->ui_->hitTestButton(math::Vec2(e.x, e.y))) {
            btn->handleClick();
            rptr->cancelPan();
        }
    }));
}

void ModelLab::exit(Renderer& r) {
    *alive_ = false;
    for (auto id : subs_) EventBus::unsubscribe(id);
    subs_.clear();
    builder_.reset();
    units_.clear();
    props_.clear();
    grid_.reset();
    ui_.reset();
    panel_.reset();
    checks_.reset();
    root_.reset();
    catalog_.clear();
    models_ = {};
    assets_.reset();
}

void ModelLab::spawn() {
    builder_.reset();
    units_.clear();
    ids_.clear();
    props_.clear();
    grid_.reset();
    root_->clearChildren();
    grid_ = std::make_unique<grid::BattleGrid>(kGridW, kGridH, 1.0f);
    const Material* lit = assets_->material("lit");
    grid_->attachTo(root_.get(), assets_->mesh("plane"), assets_->model("plane"),
                    math::Vec3(0.62f, 0.66f, 0.52f), math::Vec3(0.25f, 0.25f, 0.28f), lit);
    builder_ = std::make_unique<game::EntityBuilder>(root_->addChild(), grid_.get());

    const auto& types = game::unitTypes();
    for (size_t i = 0; i < types.size(); ++i) {
        game::SpawnDef s;
        s.cellX = static_cast<int>(i) * 2 + 1;
        s.cellY = kUnitRow;
        if (game::Unit* u = builder_->spawnFromCatalog(catalog_, types[i].id, s)) {
            units_.push_back(u);
            ids_.push_back(types[i].id);
        }
    }

    level::Level lvl;
    for (int i = 0; i < game::UnitModels::kPropTypes; ++i) {
        level::Prop p;
        p.type = static_cast<uint8_t>(i);
        p.x = static_cast<uint8_t>(3 + 3 * i);
        p.y = kPropRow;
        p.color = math::Vec3(0.55f, 0.45f, 0.3f);
        lvl.props.push_back(p);
    }
    game::LevelSpawner::Visuals vis{assets_->mesh("cube"), assets_->model("cube"), lit};
    for (int i = 0; i < game::UnitModels::kPropTypes; ++i) {
        vis.propMesh[i] = assets_->mesh(models_.propMesh(i));
        vis.propModel[i] = assets_->model(models_.propMesh(i));
    }
    vis.propMaterial = models_.propMaterial();
    props_ = game::LevelSpawner::spawnProps(lvl, *builder_, *grid_, vis);
    applyTurn();
}

void ModelLab::restart() {
    step_ = 0;
    stepT_ = 0.0f;
    playOk_ = true;
    checked_ = false;
    clip_ = 0;
    turnDeg_ = 0.0f;
    if (checks_) checks_->reset();
    spawn();
}

void ModelLab::playClip(int i) {
    clip_ = i;
    for (game::Unit* u : units_) {
        SceneNode* v = u->visualNode();
        v->setSkeletalClip(clipAt(u->clips(), i));
        v->setTime(0.0f);
    }
}

void ModelLab::applyTurn() {
    const auto yaw = [](float deg) {
        return math::Mat4::rotation(math::Quat::fromAxisAngle(math::Vec3(0, 1, 0), math::radians(deg)));
    };
    for (game::Unit* u : units_) u->visualNode()->setLocal(yaw(turnDeg_));
    for (game::Prop* p : props_)
        if (!p->node()->children().empty()) p->node()->children()[0]->setLocal(yaw(turnDeg_));
}

void ModelLab::staticChecks() {
    const auto& types = game::unitTypes();
    bool own = models_.textured() && units_.size() == types.size();
    bool clips = !units_.empty();
    bool bones = true;
    bool fit = true;
    bool palette = models_.textured();
    for (size_t i = 0; i < units_.size(); ++i) {
        const game::UnitModels::Rig* rig = models_.rigFor(ids_[i]);
        const SceneNode* v = units_[i]->visualNode();
        const auto& d = v->drawables();
        const Mesh* mesh = rig ? assets_->mesh(rig->mesh) : nullptr;
        if (game::findUnitType(ids_[i])->model) {
            own = own && models_.ownModel(ids_[i]) && !d.empty() && d.front().mesh == mesh;
            palette = palette && !d.empty() && d.front().material && d.front().material->texture;
        }
        for (int c = 0; c < 5; ++c) clips = clips && clipAt(units_[i]->clips(), c);
        bones = bones && rig && !rig->skeleton.bones.empty() &&
                static_cast<int>(rig->skeleton.bones.size()) <= Shader::kMaxBones;
        fit = fit && fits(mesh, units_[i]->baseScale());
    }
    bool props = props_.size() == static_cast<size_t>(game::UnitModels::kPropTypes);
    for (int i = 0; props && i < game::UnitModels::kPropTypes; ++i) {
        const Mesh* m = assets_->mesh(models_.propMesh(i));
        const auto& kids = props_[i]->node()->children();
        props = m && kids.size() == 1 && !kids[0]->drawables().empty() && kids[0]->drawables().front().mesh == m;
        fit = fit && fits(m, 1.0f);
        palette = palette && props && kids[0]->drawables().front().material->texture;
    }
    checks_->check(kModelRow, own);
    checks_->check(kClipsRow, clips);
    checks_->check(kBonesRow, bones);
    checks_->check(kFitRow, fit);
    checks_->check(kPaletteRow, palette);
    checks_->check(kPropsRow, props);
    if (!own || !clips || !bones || !fit || !palette || !props)
        aout << "[modellab] own=" << own << " clips=" << clips << " bones=" << bones << " fit=" << fit
             << " palette=" << palette << " props=" << props << std::endl;
}

void ModelLab::autoDrive(float dt) {
    stepT_ += dt;
    if (step_ == 0) {
        if (stepT_ < 0.5f) return;
        staticChecks();
        playClip(0);
        ++step_;
        stepT_ = 0.0f;
        return;
    }
    if (step_ <= 5) {
        if (stepT_ >= 0.15f && !checked_) {
            checked_ = true;
            for (size_t i = 0; i < units_.size(); ++i) {
                const SceneNode* v = units_[i]->visualNode();
                const bool ok = v->skeletalClip() == clipAt(units_[i]->clips(), clip_) && v->hasBoneMatrices();
                if (!ok)
                    aout << "[modellab] " << ids_[i] << " not playing " << game::UnitModels::clipNames()[clip_]
                         << std::endl;
                playOk_ = playOk_ && ok;
            }
        }
        if (stepT_ < 0.9f) return;
        checked_ = false;
        if (step_ < 5) {
            playClip(step_);
        } else {
            checks_->check(kPlayRow, playOk_);
            playClip(0);
            aout << "[modellab] auto-drive done: " << checks_->passedCount() << "/" << checks_->size()
                 << " checks green" << std::endl;
        }
        ++step_;
        stepT_ = 0.0f;
    }
}

void ModelLab::resetCamera(Renderer& r) {
    Camera& cam = r.camera();
    cam.setPerspective(kFovY, cam.aspect(), kNear, kFar);
    cam.setTerrainHeightFn({});
    cam.setForward(math::Vec3(0, 0, -1));
    cam.setPitchLimits(Camera::kMinPitchDeg, Camera::kMaxPitchDeg);
    cam.setYawLimits(Camera::kMinYawDeg, Camera::kMaxYawDeg);
    cam.setDistanceLimits(3.0f, 25.0f, 12.8f);
    cam.setViewLookAt(kEye, kLook, math::Vec3(0, 1, 0));
    r.setClearColor(math::Vec3(0.36f, 0.52f, 0.70f));
    r.setFrameLight(math::Vec3(0.3f, 0.85f, 0.45f).normalized(), 0.4f);
}

void ModelLab::buildUI(Renderer& r) {
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
        math::Vec2(static_cast<float>(w) - 24.0f - rowW, 24.0f), math::Vec2(rowW, rowH), rowH * 0.15f, scale);
    panel_->addRow("CLIP", [this]() { return std::string(game::UnitModels::clipNames()[clip_]); },
                   [this]() { autoOn_ = false; playClip((clip_ + 1) % 5); });
    panel_->addRow("TURN", [this]() { return std::string(turnOn_ ? "ON" : "OFF"); },
                   [this]() { turnOn_ = !turnOn_; });
    panel_->addRow("AUTO", [this]() { return std::string(autoOn_ ? "ON" : "OFF"); },
                   [this]() { autoOn_ = !autoOn_; });
    panel_->addAction("RESTART CHECKS", [this]() { autoOn_ = true; restart(); });
    panel_->addAction("BACK", [rptr, gs]() {
        rptr->sceneManager().replace(std::make_unique<HubScene>(*gs));
    });
    root->addChild(panel_->build());
    root->addChild(checks_->build());
    ui_->setRoot(std::move(root));
}

void ModelLab::onResize(int w, int h, Renderer& r) {
    if (w < 1 || h < 1 || !ui_) return;
    r.camera().setPerspective(kFovY, static_cast<float>(w) / static_cast<float>(h), kNear, kFar);
    buildUI(r);
}

void ModelLab::update(float dt, Renderer& r) {
    if (turnOn_) {
        turnDeg_ = std::fmod(turnDeg_ + kTurnDegPerSec * dt, 360.0f);
        applyTurn();
    }
    if (root_) root_->update(dt);
    if (builder_) builder_->updateAll(dt);
    if (autoOn_) autoDrive(dt);
    if (panel_) panel_->refresh();
}

void ModelLab::render(Renderer& r) {
    r.renderSceneGraph(root_.get());
    r.renderUI(ui_.get());
}
