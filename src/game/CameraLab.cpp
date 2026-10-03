#include "CameraLab.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include <GLES3/gl3.h>

#include "Engine.h"
#include "GameState.h"
#include "HubScene.h"

namespace {

constexpr float kFovY = 30.0f * math::PI / 180.0f;
constexpr float kNear = 0.1f;
constexpr float kFar  = 100.0f;
constexpr float kOrthoHalfHeight = 6.0f;
const math::Vec3 kEye(0.0f, 7.39f, 9.83f);
const math::Vec3 kTarget(0.0f, 0.5f, 0.0f);
constexpr int kKeyFirstPreset = 49;

const char* kOrbitYaw    = "ORBIT YAW";
const char* kPitchLimits = "PITCH LIMITS";
const char* kYawLimits   = "YAW LIMITS";
const char* kOrthoToggle = "ORTHO TOGGLE";
const char* kSetForward  = "SET FORWARD";
const char* kFollow      = "FOLLOW TARGET";
const char* kPickHit     = "PICK HIT";
const char* kPickMiss    = "PICK MISS";
const char* kPointerMove = "POINTER MOVE";
const char* kPointerUp   = "POINTER UP";
const char* kKeyDown     = "KEY DOWN";
const char* kFramesAtk   = "FRAMES ATTACK";
const char* kPanClamp    = "PAN CLAMP";
const char* kPortraitFit = "PORTRAIT FIT";

struct Preset { const char* name; float pitch; float yaw; };
const Preset kPresets[] = {
    {"ISO", 35.0f, 0.0f}, {"SIDE", 15.0f, 0.0f}, {"TOP", 70.0f, 0.0f},
    {"LEFT", 35.0f, -45.0f}, {"RIGHT", 35.0f, 45.0f},
};
constexpr int kPresetCount = 5;

const math::Vec3 kTargetPos[] = {
    {-4.0f, 0.5f, -4.0f}, {4.0f, 0.5f, -4.0f}, {0.0f, 0.5f, 0.0f},
    {-4.0f, 0.5f, 4.0f}, {4.0f, 0.5f, 4.0f},
};
const math::Vec3 kTargetColor[] = {
    {0.9f, 0.3f, 0.2f}, {0.2f, 0.7f, 0.3f}, {0.3f, 0.5f, 0.9f},
    {0.9f, 0.8f, 0.2f}, {0.7f, 0.3f, 0.8f},
};

float eyeYawDeg(const Camera& c) {
    const math::Vec3 o = c.eye() - c.target();
    return math::degrees(std::atan2(o.x, o.z));
}

float eyePitchDeg(const Camera& c) {
    const math::Vec3 o = c.eye() - c.target();
    return math::degrees(std::asin(math::clamp(o.y / o.length(), -1.0f, 1.0f)));
}

math::Vec3 moverPos(float t) {
    return math::Vec3(3.0f * std::cos(t * 0.8f), 0.5f, 3.0f * std::sin(t * 0.8f));
}

}

void CameraLab::enter(Renderer& r) {
    assets_ = std::make_unique<game::LabAssets>(r.platform());
    assets_->upload();
    const Material* lit = assets_->material("lit");
    const Mesh* plane = assets_->mesh("plane");
    const Model* planeModel = assets_->model("plane");
    const Mesh* cube = assets_->mesh("cube");
    const Model* cubeModel = assets_->model("cube");

    root_ = std::make_unique<SceneNode>();
    SceneNode* ground = root_->addChild();
    ground->setLocal(math::Mat4::scale(math::Vec3(20.0f, 1.0f, 20.0f)));
    ground->addDrawable({plane, planeModel, math::Vec3(0.45f, 0.50f, 0.40f), lit});
    mover_ = root_->addChild();
    mover_->setLocal(math::Mat4::translation(moverPos(0.0f)) * math::Mat4::scale(math::Vec3(0.6f)));
    mover_->addDrawable({cube, cubeModel, math::Vec3(1.0f, 1.0f, 1.0f), lit});

    targets_ = std::make_unique<SceneNode>();
    for (int i = 0; i < 5; ++i) {
        SceneNode* n = targets_->addChild();
        n->setLocal(math::Mat4::translation(kTargetPos[i]));
        n->addDrawable({cube, cubeModel, kTargetColor[i], lit});
        targetNodes_.push_back(n);
        targetColors_.push_back(kTargetColor[i]);
    }
    r.setPickRoot(targets_.get());
    r.setClearColor(math::Vec3(0.45f, 0.70f, 0.95f));
    r.setFrameLight(math::Vec3(0.4f, 0.8f, 0.5f).normalized(), 0.3f);

    ui_ = std::make_unique<ui::UIRoot>();
    ui_->init();
    checks_ = std::make_unique<game::LabChecklist>(math::Vec2(16, 16));
    for (const char* row : {kOrbitYaw, kPitchLimits, kYawLimits, kOrthoToggle, kSetForward,
                            kFollow, kPickHit, kPickMiss, kPointerMove, kPointerUp, kKeyDown,
                            kFramesAtk, kPanClamp, kPortraitFit})
        checks_->add(row);
    resetCamera(r);
    buildUI(r);
    restartAutoDrive(r);

    Renderer* rptr = &r;
    auto alive = alive_;
    CameraLab* self = this;
    subs_.push_back(EventBus::on<PointerDownEvent>([alive, self, rptr](const PointerDownEvent& e) {
        if (*alive) self->onTap(e.x, e.y, *rptr);
    }));
    subs_.push_back(EventBus::on<PointerMoveEvent>([alive, self](const PointerMoveEvent&) {
        if (*alive) ++self->moveEvents_;
    }));
    subs_.push_back(EventBus::on<PointerUpEvent>([alive, self](const PointerUpEvent&) {
        if (*alive) ++self->upEvents_;
    }));
    subs_.push_back(EventBus::on<KeyDownEvent>([alive, self, rptr](const KeyDownEvent& e) {
        if (!*alive) return;
        const int preset = e.keyCode - kKeyFirstPreset;
        if (preset < 0 || preset >= kPresetCount) return;
        ++self->keyEvents_;
        self->applyPreset(*rptr, preset);
    }));
}

void CameraLab::exit(Renderer& r) {
    *alive_ = false;
    for (auto id : subs_) EventBus::unsubscribe(id);
    subs_.clear();
    r.setPickRoot(nullptr);
    ortho_ = false;
    forwardX_ = false;
    tightLimits_ = false;
    resetCamera(r);
    ui_.reset();
    panel_.reset();
    checks_.reset();
    targetNodes_.clear();
    targets_.reset();
    root_.reset();
    assets_.reset();
}

void CameraLab::applyProjection(Renderer& r) {
    Camera& cam = r.camera();
    if (ortho_) cam.setOrthographic(kOrthoHalfHeight, cam.aspect(), kNear, kFar);
    else        cam.setPerspective(kFovY, cam.aspect(), kNear, kFar);
}

void CameraLab::resetCamera(Renderer& r) {
    Camera& cam = r.camera();
    applyProjection(r);
    cam.setTerrainHeightFn({});
    cam.setForward(forwardX_ ? math::Vec3(1, 0, 0) : math::Vec3(0, 0, -1));
    if (tightLimits_) {
        cam.setPitchLimits(20.0f, 60.0f);
        cam.setYawLimits(-30.0f, 30.0f);
    } else {
        cam.setPitchLimits(Camera::kMinPitchDeg, Camera::kMaxPitchDeg);
        cam.setYawLimits(Camera::kMinYawDeg, Camera::kMaxYawDeg);
    }
    cam.setDistanceLimits(5.0f, 20.0f, 12.0f);
    cam.setViewLookAt(kEye, kTarget, math::Vec3(0, 1, 0));
    applyPreset(r, 0);
}

void CameraLab::applyPreset(Renderer& r, int preset) {
    preset_ = preset;
    r.camera().setOrbitSnap(kPresets[preset].pitch, kPresets[preset].yaw);
    if (panel_) panel_->refresh();
}

void CameraLab::restartAutoDrive(Renderer& r) {
    autoT_ = 0.0f;
    step_ = 0;
    followFrames_ = 0;
    moveEvents_ = 0;
    upEvents_ = 0;
    keyEvents_ = 0;
    inDrag_ = false;
    if (checks_) checks_->reset();
}

void CameraLab::buildUI(Renderer& r) {
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
    panel_->addAction("RESTART CHECKS", [this, rptr]() {
        autoOn_ = true;
        restartAutoDrive(*rptr);
    });
    panel_->addRow("PRESET", [this]() { return std::string(kPresets[preset_].name); },
                   [this, rptr]() { applyPreset(*rptr, (preset_ + 1) % kPresetCount); });
    panel_->addRow("PROJECTION", [this]() { return std::string(ortho_ ? "ORTHO" : "PERSP"); },
                   [this, rptr]() { ortho_ = !ortho_; applyProjection(*rptr); });
    panel_->addRow("FORWARD", [this]() { return std::string(forwardX_ ? "+X" : "-Z"); },
                   [this, rptr]() { forwardX_ = !forwardX_; resetCamera(*rptr); });
    panel_->addRow("LIMITS", [this]() { return std::string(tightLimits_ ? "TIGHT" : "DEFAULT"); },
                   [this, rptr]() { tightLimits_ = !tightLimits_; resetCamera(*rptr); });
    panel_->addRow("FOLLOW", [this]() { return std::string(follow_ ? "ON" : "OFF"); },
                   [this]() { follow_ = !follow_; });
    panel_->addAction("ORBIT +15", [rptr]() { rptr->camera().orbit(15.0f, 0.0f); });
    panel_->addAction("ZOOM IN", [rptr]() { rptr->zoomBy(1.2f); });
    panel_->addAction("ZOOM OUT", [rptr]() { rptr->zoomBy(1.0f / 1.2f); });
    panel_->addAction("BACK", [rptr, gs]() {
        rptr->sceneManager().replace(std::make_unique<HubScene>(*gs));
    });
    root->addChild(panel_->build());
    root->addChild(checks_->build());

    auto status = std::make_unique<ui::UIText>(
        math::Vec2(16.0f, static_cast<float>(h) - 40.0f), "TAP A CUBE  KEYS 1-5 PRESETS",
        math::Vec4(1.0f, 1.0f, 1.0f, 1.0f), 2);
    status_ = status.get();
    root->addChild(std::move(status));
    ui_->setRoot(std::move(root));
}

void CameraLab::onResize(int w, int h, Renderer& r) {
    if (w < 1 || h < 1 || !ui_) return;
    applyProjection(r);
    buildUI(r);
}

math::Vec2 CameraLab::project(const math::Vec3& p, Renderer& r) const {
    const math::Mat4 vp = r.camera().viewProjection();
    const math::Vec4 c = vp * math::Vec4(p.x, p.y, p.z, 1.0f);
    const float nx = c.x / c.w, ny = c.y / c.w;
    return math::Vec2((nx * 0.5f + 0.5f) * static_cast<float>(r.width()),
                      (0.5f - ny * 0.5f) * static_cast<float>(r.height()));
}

void CameraLab::onTap(float x, float y, Renderer& r) {
    if (!ui_) return;
    if (ui::UIButton* btn = ui_->hitTestButton(math::Vec2(x, y))) {
        btn->handleClick();
        r.cancelPan();
        return;
    }
    const picking::Hit hit = r.pickAt(x, y);
    for (size_t i = 0; i < targetNodes_.size(); ++i)
        targetNodes_[i]->drawables()[0].color = targetColors_[i];
    char buf[48];
    if (hit.valid && hit.shapeIndex >= 0 && hit.shapeIndex < static_cast<int>(targetNodes_.size())) {
        targetNodes_[hit.shapeIndex]->drawables()[0].color = math::Vec3(1.0f, 1.0f, 1.0f);
        snprintf(buf, sizeof(buf), "PICK CUBE %d  D=%.2f", hit.shapeIndex, hit.distance);
    } else {
        snprintf(buf, sizeof(buf), "PICK MISS");
    }
    if (status_) status_->setText(buf);
}

void CameraLab::checkInstant(Renderer& r) {
    Camera& cam = r.camera();

    cam.setOrbitSnap(35.0f, 0.0f);
    const float y0 = eyeYawDeg(cam);
    cam.orbit(45.0f, 0.0f);
    checks_->check(kOrbitYaw, std::abs(eyeYawDeg(cam) - y0 - 45.0f) < 0.5f);

    cam.setOrbitSnap(35.0f, 0.0f);
    cam.setPitchLimits(20.0f, 60.0f);
    cam.orbit(0.0f, 100.0f);
    checks_->check(kPitchLimits, std::abs(cam.pitch() - 60.0f) < 1e-3f &&
                                 std::abs(eyePitchDeg(cam) - 60.0f) < 0.5f);
    cam.setPitchLimits(Camera::kMinPitchDeg, Camera::kMaxPitchDeg);

    cam.setOrbitSnap(35.0f, 0.0f);
    cam.setYawLimits(-30.0f, 30.0f);
    cam.orbit(100.0f, 0.0f);
    checks_->check(kYawLimits, std::abs(cam.yaw() - 30.0f) < 1e-3f &&
                               std::abs(eyeYawDeg(cam) - 30.0f) < 0.5f);
    cam.setYawLimits(Camera::kMinYawDeg, Camera::kMaxYawDeg);
    applyPreset(r, 0);
}

void CameraLab::checkPicks(Renderer& r) {
    bool all = true;
    for (int i = 0; i < static_cast<int>(targetNodes_.size()); ++i) {
        const math::Vec2 s = project(kTargetPos[i], r);
        const picking::Hit hit = r.pickAt(s.x, s.y);
        if (!hit.valid || hit.shapeIndex != i) {
            aout << "[cameralab] pick cube " << i << " at (" << s.x << "," << s.y
                 << ") got valid=" << hit.valid << " index=" << hit.shapeIndex << std::endl;
            all = false;
        }
    }
    checks_->check(kPickHit, all);
    const picking::Hit miss = r.pickAt(static_cast<float>(r.width()) * 0.5f, 8.0f);
    checks_->check(kPickMiss, !miss.valid);
}

void CameraLab::autoDrive(float dt, Renderer& r) {
    autoT_ += dt;
    Camera& cam = r.camera();
    const float cx = static_cast<float>(r.width()) * 0.4f;
    const float cy = static_cast<float>(r.height()) * 0.6f;
    switch (step_) {
        case 0:
            ortho_ = false; forwardX_ = false; tightLimits_ = false; follow_ = false;
            resetCamera(r);
            checkInstant(r);
            step_ = 1;
            break;
        case 1:
            if (autoT_ < 0.5f) break;
            ortho_ = true;
            applyProjection(r);
            step_ = 2;
            break;
        case 2: {
            if (autoT_ < 1.5f) break;
            const math::Mat4& po = cam.projection();
            const bool orthoOk = po.m[15] == 1.0f && po.m[11] == 0.0f;
            ortho_ = false;
            applyProjection(r);
            const math::Mat4& pp = cam.projection();
            checks_->check(kOrthoToggle, orthoOk && pp.m[11] == -1.0f && pp.m[15] == 0.0f);
            step_ = 3;
            break;
        }
        case 3: {
            if (autoT_ < 2.0f) break;
            forwardX_ = true;
            resetCamera(r);
            const math::Vec3 flat = math::Vec3(cam.viewDir().x, 0.0f, cam.viewDir().z).normalized();
            checks_->check(kSetForward, flat.x > 0.999f);
            step_ = 4;
            break;
        }
        case 4:
            if (autoT_ < 3.0f) break;
            forwardX_ = false;
            resetCamera(r);
            follow_ = true;
            followFrames_ = 0;
            followStart_ = moverPos(time_);
            step_ = 5;
            break;
        case 5:
            if (autoT_ < 6.0f) break;
            checks_->check(kFollow, followFrames_ >= 60);
            follow_ = false;
            resetCamera(r);
            step_ = 6;
            break;
        case 6:
            if (autoT_ < 6.3f) break;
            checkPicks(r);
            step_ = 7;
            break;
        case 7:
            if (autoT_ < 6.6f) break;
            dragStartTarget_ = cam.target();
            moveEvents_ = 0;
            upEvents_ = 0;
            inDrag_ = true;
            r.pointerDown(0, cx, cy);
            step_ = 8;
            break;
        case 8: {
            const float k = std::min((autoT_ - 6.6f) / 0.4f, 1.0f);
            r.pointerMove(0, cx + 150.0f * k, cy);
            if (autoT_ < 7.0f) break;
            r.pointerUp(0, cx + 150.0f, cy);
            inDrag_ = false;
            step_ = 9;
            break;
        }
        case 9:
            if (autoT_ < 7.2f) break;
            checks_->check(kPointerMove, moveEvents_ > 0 &&
                                         (cam.target() - dragStartTarget_).length() > 0.1f);
            checks_->check(kPointerUp, upEvents_ > 0);
            keyEvents_ = 0;
            EventBus::emit<KeyDownEvent>(kKeyFirstPreset + 2);
            step_ = 10;
            break;
        case 10:
            if (autoT_ < 7.4f) break;
            checks_->check(kKeyDown, keyEvents_ > 0 && preset_ == 2 &&
                                     std::abs(eyePitchDeg(cam) - kPresets[2].pitch) < 0.5f);
            resetCamera(r);
            {
                const math::Vec3 a = kTargetPos[0], b = kTargetPos[4];
                cam.frame({a, b}, 0.15f);
                const math::Vec3 mid = (a + b) * 0.5f;
                const bool inside = cam.inView(a, 0.15f) && cam.inView(b, 0.15f);
                const float d = cam.distance();
                cam.setDistance(d * 0.85f);
                const bool tight = !(cam.inView(a, 0.15f) && cam.inView(b, 0.15f));
                cam.setDistance(d);
                checks_->check(kFramesAtk, inside && tight && (cam.target() - mid).length() < 1e-3f);
            }
            step_ = 11;
            break;
        case 11: {
            if (autoT_ < 8.4f) break;
            resetCamera(r);
            cam.setTargetBounds(math::Vec2(-8.0f, -8.0f), math::Vec2(8.0f, 8.0f));
            bool clamped = true;
            const float pushes[4][2] = {{-1e5f, 0.0f}, {1e5f, 0.0f}, {0.0f, 1e5f}, {0.0f, -1e5f}};
            for (const auto& p : pushes) {
                cam.pan(p[0], p[1]);
                const math::Vec3 t = cam.target();
                clamped = clamped && std::abs(t.x) <= 8.0f + 1e-4f && std::abs(t.z) <= 8.0f + 1e-4f &&
                          (std::abs(std::abs(t.x) - 8.0f) < 1e-3f || std::abs(std::abs(t.z) - 8.0f) < 1e-3f);
            }
            cam.clearTargetBounds();
            checks_->check(kPanClamp, clamped);
            resetCamera(r);

            const std::vector<math::Vec3> corners{{-10, 0, -10}, {10, 0, -10}, {-10, 0, 10}, {10, 0, 10}};
            Camera wide = cam;
            wide.setDistanceLimits(1.0f, 200.0f, 12.0f);
            wide.setViewportSize(1280, 720);
            wide.setPerspective(cam.fovYRadians(), 1280.0f / 720.0f, 0.1f, 300.0f);
            wide.frame(corners, 0.05f);
            Camera tall = wide;
            tall.setViewportSize(1080, 1920);
            tall.setPerspective(cam.fovYRadians(), 1080.0f / 1920.0f, 0.1f, 300.0f);
            tall.frame(corners, 0.05f);
            bool fit = tall.distance() > wide.distance();
            for (const auto& c : corners) fit = fit && tall.inView(c, 0.05f) && wide.inView(c, 0.05f);
            checks_->check(kPortraitFit, fit);
            aout << "[cameralab] auto-drive done: " << checks_->passedCount() << "/"
                 << checks_->size() << " checks green" << std::endl;
            step_ = 12;
            break;
        }
        default:
            break;
    }
    if (panel_) panel_->refresh();
}

void CameraLab::update(float dt, Renderer& r) {
    time_ += dt;
    const math::Vec3 mp = moverPos(time_);
    mover_->setLocal(math::Mat4::translation(mp) * math::Mat4::scale(math::Vec3(0.6f)));
    if (autoOn_) autoDrive(dt, r);
    if (follow_) {
        r.camera().setTarget(mp);
        if ((r.camera().target() - mp).length() < 1e-4f && (mp - followStart_).length() > 1.0f)
            ++followFrames_;
    }
    if (root_) root_->update(dt);
    if (targets_) targets_->update(dt);
}

void CameraLab::render(Renderer& r) {
    r.renderSceneGraph(root_.get());
    r.renderSceneGraph(targets_.get());
    r.renderUI(ui_.get());
}
