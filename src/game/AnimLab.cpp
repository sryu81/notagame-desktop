#include "AnimLab.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <GLES3/gl3.h>

#include "ClipEvents.h"
#include "Engine.h"
#include "GameState.h"
#include "HubScene.h"
#include "Unit.h"

namespace {

constexpr float kFovY = 30.0f * math::PI / 180.0f;
constexpr float kNear = 0.1f;
constexpr float kFar  = 100.0f;
const math::Vec3 kEye(3.2f, 2.4f, 6.0f);
const math::Vec3 kLook(0.3f, 0.9f, 0.0f);
constexpr float kSlopeDeg = 14.0f;
constexpr int   kGridSize = 12;
constexpr float kFootHeight = 0.10f;
constexpr float kFrame = 1.0f / 30.0f;
const math::Vec2 kStartCell(5.0f, 6.0f);
const math::Vec2 kEndCell(8.0f, 6.0f);

const char* kRigLoaded  = "RIG LOADED";
const char* kBindPose   = "BIND POSE";
const char* kLoopFlags  = "LOOP FLAGS";
const char* kUntracked  = "UNTRACKED AT REST";
const char* kIdleWraps  = "IDLE WRAPS";
const char* kOneShot    = "ONE-SHOT ENDS";
const char* kSkelHit    = "SKELETAL HIT EVENT";
const char* kRefire     = "EVENT RE-FIRES";
const char* kRate       = "PLAYBACK RATE";
const char* kWalkMove   = "WALK DURING MOVE";
const char* kIKSlope    = "FOOT IK ON SLOPE";
const char* kKneePlane  = "KNEE PLANE";
const char* kIKClamp    = "IK CLAMP";
const char* kRigidEvent = "RIGID EVENT";

float groundY(float x, float) { return x * std::tan(math::radians(kSlopeDeg)); }

math::Vec3 translationOf(const math::Mat4& m) { return math::Vec3(m.m[12], m.m[13], m.m[14]); }

bool nearMat(const math::Mat4& a, const math::Mat4& b, float eps) {
    for (int i = 0; i < 16; ++i) if (std::abs(a.m[i] - b.m[i]) > eps) return false;
    return true;
}

const math::Quat kFaceBack = math::Quat::fromAxisAngle(math::Vec3(0, 1, 0), math::PI);

}

AnimLab::AnimLab(game::GameState& gs) : gs_(gs) {}
AnimLab::~AnimLab() = default;

bool AnimLab::loadRig(Renderer& r) {
    std::vector<uint8_t> bytes;
    if (!r.platform().readAsset("test_rig.glb", bytes)) return false;
    gltf::LoadResult glb;
    if (!gltf::load(bytes.data(), bytes.size(), glb)) return false;
    skel_ = std::move(glb.skeleton);
    for (auto& [name, clip] : glb.clips) {
        game::applyClipEvents(name, clip);
        clips_[name] = std::move(clip);
    }
    bindModel_.clear();
    for (const auto& b : skel_.bones) bindModel_.push_back(b.invBindPose.inverted());
    hRig_ = assets_->addMesh("test_rig", std::move(glb.mesh));
    return true;
}

void AnimLab::enter(Renderer& r) {
    r.setPickRoot(nullptr);
    assets_ = std::make_unique<game::LabAssets>(r.platform());
    rigLoaded_ = loadRig(r);
    assets_->upload();
    buildScene();

    ui_ = std::make_unique<ui::UIRoot>();
    ui_->init();
    checks_ = std::make_unique<game::LabChecklist>(math::Vec2(16, 16));
    for (const char* row : {kRigLoaded, kBindPose, kLoopFlags, kUntracked, kIdleWraps, kOneShot,
                            kSkelHit, kRefire, kRate, kWalkMove, kIKSlope, kKneePlane, kIKClamp,
                            kRigidEvent})
        checks_->add(row);
    resetCamera(r);
    buildUI(r);
    restartAutoDrive();

    Renderer* rptr = &r;
    auto alive = alive_;
    AnimLab* self = this;
    subs_.push_back(EventBus::on<PointerDownEvent>([alive, self, rptr](const PointerDownEvent& e) {
        if (!*alive || !self->ui_) return;
        if (ui::UIButton* btn = self->ui_->hitTestButton(math::Vec2(e.x, e.y))) {
            btn->handleClick();
            rptr->cancelPan();
        }
    }));
}

void AnimLab::exit(Renderer& r) {
    *alive_ = false;
    for (auto id : subs_) EventBus::unsubscribe(id);
    subs_.clear();
    ik_.detach();
    if (actions_) actions_->clear();
    actions_.reset();
    ui_.reset();
    panel_.reset();
    checks_.reset();
    unit_.reset();
    root_.reset();
    grid_.reset();
    assets_.reset();
}

void AnimLab::buildScene() {
    const Material* lit = assets_->material("lit");
    const Material* skinned = assets_->material("skinned");
    const Mesh* plane = assets_->mesh("plane");
    const Model* planeModel = assets_->model("plane");
    const Mesh* cube = assets_->mesh("cube");
    const Model* cubeModel = assets_->model("cube");

    root_ = std::make_unique<SceneNode>();
    SceneNode* ramp = root_->addChild();
    const float c = std::cos(math::radians(kSlopeDeg));
    ramp->setLocal(math::Mat4::rotationZ(math::radians(kSlopeDeg)) *
                   math::Mat4::scale(math::Vec3(static_cast<float>(kGridSize) / c, 1.0f,
                                                static_cast<float>(kGridSize))));
    ramp->addDrawable({plane, planeModel, math::Vec3(0.55f, 0.60f, 0.48f), lit});

    grid_ = std::make_unique<grid::BattleGrid>(kGridSize, kGridSize, 1.0f);
    for (int y = 0; y < kGridSize; ++y)
        for (int x = 0; x < kGridSize; ++x) {
            const math::Vec3 w = grid_->cellToWorld(x, y);
            grid_->setCellY(x, y, groundY(w.x, w.z));
        }

    hold_.loop = true;
    hold_.keyframes = {{0.0f, math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)},
                       {1.0f, math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)}};
    unit_ = std::make_unique<game::Unit>(root_.get(), unit::Faction::Player, unit::Stats{10, 5, 3, 4, 1});
    unit_->setCell(kStartCell);
    unit_->node()->setClip(&hold_);
    rigNode_ = unit_->node()->addChild();
    rigNode_->setLocal(math::Mat4::rotation(kFaceBack));
    if (rigLoaded_) {
        rigNode_->setSkeleton(&skel_);
        rigNode_->addDrawable({assets_->mesh(hRig_), assets_->model(hRig_),
                               math::Vec3(0.85f, 0.55f, 0.30f), skinned});
        ik_.bind(skel_);
        ik_.attach(rigNode_, groundY, kFootHeight);
    }
    AnimLab* self = this;
    rigNode_->setAnimEventCallback([self](int id) {
        if (id == game::kEventHit) self->hitTimes_.push_back(self->t_ - self->playStart_);
        if (id == game::kEventFootL) {
            ++self->footL_;
            self->footLTimes_.push_back(self->t_);
        }
    });
    resetRigPose();

    cubeAttack_.loop = false;
    cubeAttack_.keyframes = {
        {0.0f,  math::Vec3(-2.5f, 0.4f + groundY(-2.5f, 0.0f), -1.5f), math::Quat::identity(), math::Vec3(0.5f)},
        {0.15f, math::Vec3(-2.1f, 0.4f + groundY(-2.1f, 0.0f), -1.5f), math::Quat::identity(), math::Vec3(0.5f)},
        {0.5f,  math::Vec3(-2.5f, 0.4f + groundY(-2.5f, 0.0f), -1.5f), math::Quat::identity(), math::Vec3(0.5f)}};
    cubeAttack_.events.push_back({0.15f, 1});
    cube_ = root_->addChild();
    cube_->addDrawable({cube, cubeModel, math::Vec3(0.3f, 0.5f, 0.9f), lit});
    cube_->setClip(&cubeAttack_);
    cube_->setTime(10.0f);
    cube_->setAnimEventCallback([self](int id) { if (id == 1) ++self->rigidHits_; });

    actions_ = std::make_unique<action::Manager>();
}

void AnimLab::resetRigPose() {
    const math::Vec3 w = grid_->cellToWorld(static_cast<int>(kStartCell.x), static_cast<int>(kStartCell.y));
    unit_->setCell(kStartCell);
    unit_->node()->setAnimOffset(w);
    unit_->node()->setAnimRotOffset(kFaceBack);
}

void AnimLab::playClip(const char* name, float rate) {
    auto it = clips_.find(name);
    if (it == clips_.end() || !rigNode_) return;
    clipName_ = name;
    rate_ = rate;
    rigNode_->setSkeletalClip(&it->second);
    rigNode_->setTime(0.0f);
    rigNode_->setPlaybackRate(rate);
    prevWrapped_ = -1.0f;
    if (panel_) panel_->refresh();
}

void AnimLab::resetCamera(Renderer& r) {
    Camera& cam = r.camera();
    cam.setPerspective(kFovY, cam.aspect(), kNear, kFar);
    cam.setTerrainHeightFn({});
    cam.setForward(math::Vec3(0, 0, -1));
    cam.setPitchLimits(Camera::kMinPitchDeg, Camera::kMaxPitchDeg);
    cam.setYawLimits(Camera::kMinYawDeg, Camera::kMaxYawDeg);
    cam.setDistanceLimits(3.0f, 15.0f, 6.8f);
    cam.setViewLookAt(kEye, kLook, math::Vec3(0, 1, 0));
    r.setClearColor(math::Vec3(0.45f, 0.70f, 0.95f));
    r.setFrameLight(math::Vec3(0.3f, 0.85f, 0.45f).normalized(), 0.35f);
}

void AnimLab::restartAutoDrive() {
    step_ = 0;
    stepT_ = 0.0f;
    idleWraps_ = 0;
    untrackedOk_ = untrackedBad_ = 0;
    hitTimes_.clear();
    playStarts_.clear();
    footL_ = 0;
    footLTimes_.clear();
    rigidHits_ = 0;
    heldFrames_ = 0;
    sawWalkInMove_ = false;
    slowRateOk_ = false;
    walkStarted_ = false;
    ik_.setEnabled(false);
    ik_.clearOverride(0);
    if (actions_) actions_->clear();
    if (grid_ && unit_) resetRigPose();
    if (checks_) checks_->reset();
}

void AnimLab::buildUI(Renderer& r) {
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
    panel_->addAction("RESTART CHECKS", [this]() { autoOn_ = true; t_ = 0.0f; restartAutoDrive(); });
    panel_->addRow("CLIP", [this]() { return clipName_; }, [this]() {
        static const char* order[] = {"Idle", "Walk", "Attack", "Hurt"};
        int i = 0;
        while (i < 4 && clipName_ != order[i]) ++i;
        playClip(order[(i + 1) % 4], rate_);
    });
    panel_->addRow("RATE", [this]() {
                       char buf[16]; snprintf(buf, sizeof(buf), "%gX", rate_); return std::string(buf);
                   },
                   [this]() {
                       rate_ = rate_ >= 2.0f ? 0.25f : rate_ * 2.0f;
                       if (rigNode_) rigNode_->setPlaybackRate(rate_);
                   });
    panel_->addRow("FOOT IK", [this]() { return std::string(ik_.enabled() ? "ON" : "OFF"); },
                   [this]() { ik_.setEnabled(!ik_.enabled()); });
    panel_->addAction("WALK 3 CELLS", [this]() {
        if (!actions_ || !actions_->isIdle()) return;
        resetRigPose();
        auto mv = std::make_unique<action::MoveAction>(
            unit_.get(), grid_.get(),
            std::vector<math::Vec2>{kStartCell, {6, 6}, {7, 6}, kEndCell}, 0.6f);
        auto it = clips_.find("Walk");
        if (it != clips_.end()) mv->setWalkClip(&it->second, rigNode_);
        actions_->enqueue(std::move(mv));
    });
    panel_->addAction("RESET POSE", [this]() { resetRigPose(); playClip("Idle"); });
    panel_->addAction("BACK", [rptr, gs]() {
        rptr->sceneManager().replace(std::make_unique<HubScene>(*gs));
    });
    root->addChild(panel_->build());
    root->addChild(checks_->build());
    ui_->setRoot(std::move(root));
}

void AnimLab::onResize(int w, int h, Renderer& r) {
    if (w < 1 || h < 1 || !ui_) return;
    r.camera().setPerspective(kFovY, static_cast<float>(w) / static_cast<float>(h), kNear, kFar);
    buildUI(r);
}

math::Mat4 AnimLab::jointModel(int joint) const {
    return rigNode_->boneMatrices()[joint] * bindModel_[joint];
}

math::Vec3 AnimLab::jointWorld(int joint) const {
    return rigNode_->world().transformPoint(translationOf(jointModel(joint)));
}

void AnimLab::checkStatic() {
    int legJoints = 0;
    for (const char* n : {"UpperLeg.L", "LowerLeg.L", "Foot.L", "UpperLeg.R", "LowerLeg.R", "Foot.R"})
        if (skel_.find(n) >= 0) ++legJoints;
    checks_->check(kRigLoaded, rigLoaded_ && skel_.bones.size() == 15 && clips_.size() == 5 &&
                               legJoints == 6);
    if (!rigLoaded_) return;

    std::vector<math::Mat4> rest;
    for (const auto& b : skel_.bones) rest.push_back(b.restLocal);
    const auto skin = anim::computeBoneMatrices(skel_, rest);
    bool identity = true;
    for (const auto& m : skin) identity = identity && nearMat(m, math::Mat4::identity(), 1e-4f);
    checks_->check(kBindPose, identity);

    auto loops = [this](const char* n) { auto it = clips_.find(n); return it != clips_.end() && it->second.loop; };
    auto has = [this](const char* n) { return clips_.count(n) > 0; };
    checks_->check(kLoopFlags, has("Idle") && has("Walk") && has("Attack") && has("Hurt") &&
                               loops("Idle") && loops("Walk") && !loops("Attack") && !loops("Hurt"));
}

void AnimLab::observe(float dt) {
    if (!rigLoaded_ || rigNode_->boneMatrices().empty()) return;
    const anim::SkeletalClip* clip = rigNode_->skeletalClip();
    if (!clip) return;
    const float wrapped = anim::wrapTime(*clip, rigNode_->time());

    if (clipName_ == "Idle") {
        if (prevWrapped_ >= 0.0f && wrapped < prevWrapped_) ++idleWraps_;
        const int arm = skel_.find("UpperArm.L");
        const int spine = skel_.find("Spine");
        const math::Mat4 local = jointModel(spine).inverted() * jointModel(arm);
        const bool spineMoved = !nearMat(jointModel(spine), bindModel_[spine], 1e-4f);
        if (nearMat(local, skel_.bones[arm].restLocal, 1e-4f)) {
            if (spineMoved) ++untrackedOk_;
        } else {
            ++untrackedBad_;
        }
    }
    prevWrapped_ = wrapped;
}

void AnimLab::autoDrive(float dt) {
    stepT_ += dt;
    auto next = [this]() { ++step_; stepT_ = 0.0f; };
    switch (step_) {
        case 0:
            checkStatic();
            playClip("Idle");
            if (!rigLoaded_) { step_ = 99; break; }
            next();
            break;
        case 1:
            if (stepT_ >= 0.3f && playStarts_.empty()) { cube_->setTime(0.0f); playStarts_.push_back(t_); }
            if (stepT_ >= 1.3f && playStarts_.size() == 1) { cube_->setTime(0.0f); playStarts_.push_back(t_); }
            if (stepT_ < 4.2f) break;
            checks_->check(kIdleWraps, idleWraps_ >= 2);
            checks_->check(kUntracked, untrackedOk_ >= 30 && untrackedBad_ == 0);
            checks_->check(kRigidEvent, rigidHits_ == 2);
            playStarts_.clear();
            playClip("Attack");
            playStart_ = t_;
            playStarts_.push_back(t_);
            next();
            break;
        case 2:
        case 3: {
            if (stepT_ >= 0.7f && stepT_ < 0.8f) {
                if (heldPose_.empty()) heldPose_ = rigNode_->boneMatrices();
                else if (heldPose_.size() == rigNode_->boneMatrices().size()) {
                    bool same = true;
                    for (size_t i = 0; i < heldPose_.size(); ++i)
                        same = same && nearMat(heldPose_[i], rigNode_->boneMatrices()[i], 1e-6f);
                    if (same && std::abs(anim::wrapTime(*rigNode_->skeletalClip(), rigNode_->time()) - 0.6f) < 1e-5f)
                        ++heldFrames_;
                }
            }
            if (stepT_ < 0.8f) break;
            heldPose_.clear();
            if (step_ == 2) {
                playClip("Attack");
                playStart_ = t_;
                playStarts_.push_back(t_);
                next();
                break;
            }
            bool hitsOk = hitTimes_.size() == 2;
            for (float ht : hitTimes_) hitsOk = hitsOk && std::abs(ht - 0.35f) <= kFrame;
            if (!hitsOk)
                aout << "[animlab] hits=" << hitTimes_.size() << " first="
                     << (hitTimes_.empty() ? 0.0f : hitTimes_[0]) << std::endl;
            checks_->check(kSkelHit, hitsOk);
            playClip("Idle");
            checks_->check(kOneShot, heldFrames_ >= 2 && clipName_ == "Idle");
            next();
            break;
        }
        case 4:
            if (!walkStarted_) {
                walkStarted_ = true;
                playClip("Walk");
                footL_ = 0;
            }
            if (stepT_ < 3.05f) break;
            checks_->check(kRefire, footL_ == 3);
            playClip("Walk", 0.5f);
            footLTimes_.clear();
            next();
            break;
        case 5:
            if (footLTimes_.size() < 2) break;
            {
                const float period = footLTimes_[1] - footLTimes_[0];
                const bool slowOk = std::abs(period - 2.0f) <= std::max(0.04f, 1.0f / 60.0f);
                if (!slowOk) aout << "[animlab] rate 0.5 period=" << period << std::endl;
                playClip("Walk", 2.0f);
                footLTimes_.clear();
                slowRateOk_ = slowOk;
            }
            next();
            break;
        case 6:
            if (footLTimes_.size() < 5) break;
            {
                const float period = (footLTimes_[4] - footLTimes_[0]) / 4.0f;
                const bool fastOk = std::abs(period - 0.5f) <= 0.01f;
                if (!fastOk) aout << "[animlab] rate 2 period=" << period << std::endl;
                checks_->check(kRate, slowRateOk_ && fastOk);
            }
            playClip("Idle");
            {
                auto mv = std::make_unique<action::MoveAction>(
                    unit_.get(), grid_.get(),
                    std::vector<math::Vec2>{kStartCell, {6, 6}, {7, 6}, kEndCell}, 0.6f);
                mv->setWalkClip(&clips_["Walk"], rigNode_);
                actions_->enqueue(std::move(mv));
            }
            sawWalkInMove_ = false;
            next();
            break;
        case 7:
            if (!actions_->isIdle() && rigNode_->skeletalClip() == &clips_["Walk"]) sawWalkInMove_ = true;
            if (stepT_ < 0.2f || !actions_->isIdle()) break;
            checks_->check(kWalkMove, sawWalkInMove_ && rigNode_->skeletalClip() == &clips_["Idle"] &&
                                      unit_->cell().x == kEndCell.x && unit_->cell().y == kEndCell.y);
            resetRigPose();
            ik_.setEnabled(true);
            next();
            break;
        case 8: {
            if (stepT_ < 0.5f) break;
            bool feetOk = true, kneesOk = true;
            for (int l = 0; l < 2; ++l) {
                const auto& leg = ik_.leg(l);
                const math::Vec3 f = jointWorld(leg.foot);
                const float err = std::abs(f.y - (groundY(f.x, f.z) + kFootHeight));
                if (err > 0.01f) {
                    aout << "[animlab] foot " << l << " err=" << err << std::endl;
                    feetOk = false;
                }
                const math::Vec3 knee = translationOf(jointModel(leg.lower));
                const math::Vec3 hip = translationOf(jointModel(leg.upper));
                const math::Vec3 foot = translationOf(jointModel(leg.foot));
                const float mid = (hip.z + foot.z) * 0.5f;
                if (!(knee.z > mid + 1e-3f)) kneesOk = false;
            }
            const float dy = std::abs(jointWorld(ik_.leg(0).foot).y - jointWorld(ik_.leg(1).foot).y);
            checks_->check(kIKSlope, feetOk && dy > 0.03f);
            checks_->check(kKneePlane, kneesOk);
            const math::Vec3 hip0 = translationOf(jointModel(ik_.leg(0).upper));
            ik_.setOverride(0, hip0 + math::Vec3(0.0f, -3.0f, 0.4f));
            next();
            break;
        }
        case 9: {
            if (stepT_ < 0.2f) break;
            const auto& leg = ik_.leg(0);
            const float len = (translationOf(jointModel(leg.foot)) - translationOf(jointModel(leg.upper))).length();
            checks_->check(kIKClamp, leg.clamped && std::abs(len - (leg.upperLen + leg.lowerLen)) < 3e-3f);
            ik_.clearOverride(0);
            aout << "[animlab] auto-drive done: " << checks_->passedCount() << "/"
                 << checks_->size() << " checks green" << std::endl;
            next();
            break;
        }
        default:
            break;
    }
}

void AnimLab::update(float dt, Renderer& r) {
    t_ += dt;
    if (root_) root_->update(dt);
    if (actions_) actions_->update(dt);
    observe(dt);
    if (autoOn_) autoDrive(dt);
    if (panel_) panel_->refresh();
}

void AnimLab::render(Renderer& r) {
    r.renderSceneGraph(root_.get());
    r.renderUI(ui_.get());
}
