#include "PickLab.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "BattleFx.h"
#include "ClipEvents.h"
#include "Engine.h"
#include "GameState.h"
#include "HubScene.h"
#include "Unit.h"
#include "UnitDef.h"

namespace {

constexpr float kFovY = 30.0f * math::PI / 180.0f;
constexpr float kNear = 0.1f;
constexpr float kFar  = 100.0f;
const math::Vec3 kEye(0.0f, 7.0f, 9.0f);
const math::Vec3 kLook(0.0f, 0.3f, 0.5f);

const char* kBodyOverCell = "BODY OVER CELL";
const char* kCellPick     = "CELL PICK";
const char* kTolerance    = "TOLERANCE";
const char* kCorpse       = "CORPSE";
const char* kIntent       = "INTENT";
const char* kCycle        = "CYCLE";
const char* kDragNoTap    = "DRAG NO TAP";
const char* kHover        = "HOVER";

}

PickLab::PickLab(game::GameState& gs) : gs_(gs) {}
PickLab::~PickLab() = default;

void PickLab::enter(Renderer& r) {
    r.setPickRoot(nullptr);
    assets_ = std::make_unique<game::LabAssets>(r.platform());
    std::vector<uint8_t> bytes;
    gltf::LoadResult glb;
    const bool rig = r.platform().readAsset("test_rig.glb", bytes) && gltf::load(bytes.data(), bytes.size(), glb);
    if (rig) {
        skel_ = std::move(glb.skeleton);
        for (auto& [name, c] : glb.clips) {
            game::applyClipEvents(name, c);
            clips_[name] = std::move(c);
        }
        hRig_ = assets_->addMesh("test_rig", std::move(glb.mesh));
    }
    assets_->upload();

    auto clip = [this](const char* n) -> const anim::SkeletalClip* {
        auto it = clips_.find(n);
        return it == clips_.end() ? nullptr : &it->second;
    };
    const unit::ClipSet set{clip("Idle"), clip("Walk"), clip("Attack"), clip("Hurt"), clip("Death")};
    for (auto [id, f, color] : {std::tuple{"knight", unit::Faction::Player, math::Vec3(0.90f, 0.55f, 0.25f)},
                                std::tuple{"goblin", unit::Faction::Enemy, math::Vec3(0.55f, 0.75f, 0.20f)}}) {
        unit::UnitDef d;
        d.name = id;
        d.faction = f;
        d.baseStats = {10, 5, 2, 3, 1};
        d.baseColor = color;
        d.baseScale = 0.5f;
        d.modelYawDeg = 180.0f;
        if (rig) {
            d.skeleton = &skel_;
            d.mesh = assets_->mesh(hRig_);
            d.model = assets_->model(hRig_);
            d.material = assets_->material("skinned");
            d.clips = set;
        }
        catalog_.registerDef(id, d);
    }

    root_ = std::make_unique<SceneNode>();
    rings_ = std::make_unique<SceneNode>();
    auto ring = [this](math::Vec3 color) {
        SceneNode* n = rings_->addChild();
        n->addDrawable({assets_->mesh("plane"), assets_->model("plane"), color, assets_->material("unlit")});
        n->setLocal(math::Mat4::scale(math::Vec3(0.0f, 1.0f, 0.0f)));
        return n;
    };
    selRing_ = ring(math::Vec3(1.0f, 0.95f, 0.15f));
    hoverRing_ = ring(math::Vec3(0.4f, 0.85f, 1.0f));

    ui_ = std::make_unique<ui::UIRoot>();
    ui_->init();
    checks_ = std::make_unique<game::LabChecklist>(math::Vec2(16, 16));
    for (const char* row : {kBodyOverCell, kCellPick, kTolerance, kCorpse, kIntent, kCycle, kDragNoTap, kHover})
        checks_->add(row);
    resetCamera(r);
    buildUI(r);
    restart();

    Renderer* rptr = &r;
    auto alive = alive_;
    PickLab* self = this;
    subs_.push_back(EventBus::on<TapEvent>([alive, self, rptr](const TapEvent& e) {
        if (*alive) self->onTap(e.x, e.y, *rptr);
    }));
    subs_.push_back(EventBus::on<HoverEvent>([alive, self, rptr](const HoverEvent& e) {
        if (*alive) self->onHover(e.x, e.y, *rptr);
    }));
}

void PickLab::exit(Renderer& r) {
    *alive_ = false;
    for (auto id : subs_) EventBus::unsubscribe(id);
    subs_.clear();
    builder_.reset();
    units_.clear();
    grid_.reset();
    ui_.reset();
    panel_.reset();
    checks_.reset();
    rings_.reset();
    root_.reset();
    catalog_.clear();
    assets_.reset();
}

void PickLab::spawn() {
    builder_.reset();
    units_.clear();
    grid_.reset();
    root_->clearChildren();
    grid_ = std::make_unique<grid::BattleGrid>(8, 6, 1.0f);
    grid_->attachTo(root_.get(), assets_->mesh("plane"), assets_->model("plane"),
                    math::Vec3(0.62f, 0.66f, 0.52f), math::Vec3(0.25f, 0.25f, 0.28f), assets_->material("lit"));
    builder_ = std::make_unique<game::EntityBuilder>(root_.get(), grid_.get());
    auto at = [this](const char* id, int x, int y) {
        game::SpawnDef s;
        s.cellX = x;
        s.cellY = y;
        s.mesh = assets_->mesh("cube");
        s.model = assets_->model("cube");
        s.material = assets_->material("lit");
        game::Unit* u = builder_->spawnFromCatalog(catalog_, id, s);
        units_.push_back(u);
        return u;
    };
    back_   = at("goblin", 3, 2);
    front_  = at("knight", 3, 3);
    lone_   = at("goblin", 6, 4);
    corpse_ = at("goblin", 1, 4);
    corpse_->takeDamage(999);
    selected_ = hovered_ = nullptr;
    picker_.resetCycle();
}

void PickLab::restart() {
    step_ = 0;
    stepT_ = 0.0f;
    taps_ = 0;
    if (checks_) checks_->reset();
    spawn();
}

void PickLab::resetCamera(Renderer& r) {
    Camera& cam = r.camera();
    cam.setPerspective(kFovY, cam.aspect(), kNear, kFar);
    cam.setTerrainHeightFn({});
    cam.clearTargetBounds();
    cam.setForward(math::Vec3(0, 0, -1));
    cam.setPitchLimits(Camera::kMinPitchDeg, Camera::kMaxPitchDeg);
    cam.setYawLimits(Camera::kMinYawDeg, Camera::kMaxYawDeg);
    cam.setDistanceLimits(4.0f, 25.0f, 11.3f);
    cam.setViewLookAt(kEye, kLook, math::Vec3(0, 1, 0));
    r.setClearColor(math::Vec3(0.38f, 0.55f, 0.75f));
    r.setFrameLight(math::Vec3(0.3f, 0.85f, 0.45f).normalized(), 0.4f);
}

void PickLab::buildUI(Renderer& r) {
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
    panel_ = std::make_unique<game::DebugPanel>(math::Vec2(static_cast<float>(w) - 24.0f - rowW, 24.0f),
                                                math::Vec2(rowW, rowH), rowH * 0.15f, scale);
    panel_->addRow("AUTO", [this]() { return std::string(autoOn_ ? "ON" : "OFF"); }, [this]() { autoOn_ = !autoOn_; });
    panel_->addAction("RESTART CHECKS", [this, rptr]() { autoOn_ = true; resetCamera(*rptr); restart(); });
    panel_->addAction("BACK", [rptr, gs]() { rptr->sceneManager().replace(std::make_unique<HubScene>(*gs)); });
    root->addChild(panel_->build());
    root->addChild(checks_->build());
    auto info = std::make_unique<ui::UIText>(math::Vec2(16.0f, static_cast<float>(h) - 40.0f), info_,
                                             math::Vec4(1, 1, 1, 1), 2);
    infoText_ = info.get();
    root->addChild(std::move(info));
    ui_->setRoot(std::move(root));
}

void PickLab::onResize(int w, int h, Renderer& r) {
    if (w < 1 || h < 1 || !ui_) return;
    r.camera().setPerspective(kFovY, static_cast<float>(w) / static_cast<float>(h), kNear, kFar);
    buildUI(r);
}

void PickLab::placeRing(SceneNode* ring, const game::Unit* u) {
    if (!u) {
        ring->setLocal(math::Mat4::scale(math::Vec3(0.0f, 1.0f, 0.0f)));
        return;
    }
    const math::Vec3 p = grid_->cellToWorld(static_cast<int>(u->cell().x), static_cast<int>(u->cell().y));
    const float s = ring == hoverRing_ ? 0.95f : 0.7f;
    ring->setLocal(math::Mat4::translation(math::Vec3(p.x, p.y + (ring == hoverRing_ ? 0.004f : 0.008f), p.z)) *
                   math::Mat4::scale(math::Vec3(s, 1.0f, s)));
}

void PickLab::onTap(float x, float y, Renderer& r) {
    if (ui_) {
        if (ui::UIButton* btn = ui_->hitTestButton(math::Vec2(x, y))) {
            btn->handleClick();
            return;
        }
    }
    ++taps_;
    const pick::Result res = picker_.pick(r.camera(), r.width(), r.height(), x, y, *grid_, units_);
    selected_ = res.unit;
    char buf[96];
    snprintf(buf, sizeof(buf), "TAP %.0f,%.0f -> %s (%s, %d CANDIDATES)", x, y,
             res.unit ? res.unit->faction() == unit::Faction::Player ? "KNIGHT" : "GOBLIN" : "NOTHING",
             pick::toString(res.how), res.candidates);
    info_ = buf;
}

void PickLab::onHover(float x, float y, Renderer& r) {
    pick::Picker probe(picker_.options());
    hovered_ = probe.pick(r.camera(), r.width(), r.height(), x, y, *grid_, units_).unit;
}

math::Vec2 PickLab::bodyPoint(Renderer& r, const game::Unit* u, float t) const {
    const pick::Body b = pick::project(r.camera(), r.width(), r.height(), *u, picker_.options());
    return b.feet + (b.head - b.feet) * t;
}

void PickLab::runChecks(Renderer& r) {
    const Camera& cam = r.camera();
    const int w = r.width(), h = r.height();
    auto pickAt = [&](math::Vec2 p, const pick::Picker::Prefer& prefer = {}) {
        return picker_.pick(cam, w, h, p.x, p.y, *grid_, units_, prefer);
    };
    auto cellUnder = [&](math::Vec2 p) {
        math::Vec2 c(-1, -1);
        pick::cellAt(*grid_, pick::rayAt(cam, p.x, p.y, w, h), c);
        return c;
    };

    bool overOk = false;
    for (float t = 1.0f; t >= 0.5f && !overOk; t -= 0.02f) {
        const math::Vec2 p = bodyPoint(r, front_, t);
        const math::Vec2 c = cellUnder(p);
        if (c.x == back_->cell().x && c.y == back_->cell().y) {
            picker_.resetCycle();
            const pick::Result res = pickAt(p);
            overOk = res.unit == front_ && res.how == pick::How::Body;
            if (!overOk) aout << "[picklab] over-cell got " << pick::toString(res.how) << std::endl;
            break;
        }
    }
    checks_->check(kBodyOverCell, overOk);

    picker_.resetCycle();
    const math::Vec3 corner = grid_->cellToWorld(6, 4) + math::Vec3(0.38f, 0.0f, 0.38f);
    math::Vec2 cornerPx;
    fx::worldToScreen(cam, corner, w, h, cornerPx);
    const pick::Result cellRes = pickAt(cornerPx);
    checks_->check(kCellPick, cellRes.unit == lone_ && cellRes.how == pick::How::Cell);

    const pick::Body lb = pick::project(cam, w, h, *lone_, picker_.options());
    const math::Vec2 mid = (lb.feet + lb.head) * 0.5f;
    const math::Vec2 nearPx = mid + math::Vec2(lb.radiusPx + 18.0f, -lb.radiusPx);
    const math::Vec2 farPx = mid + math::Vec2(lb.radiusPx + 90.0f, -40.0f);
    const pick::Result nearRes = pickAt(nearPx);
    const pick::Result farRes = pickAt(farPx);
    checks_->check(kTolerance, nearRes.unit == lone_ && farRes.unit == nullptr);
    if (nearRes.unit != lone_ || farRes.unit)
        aout << "[picklab] tolerance near=" << pick::toString(nearRes.how) << " far=" << pick::toString(farRes.how)
             << std::endl;

    const pick::Result corpseBody = pickAt(bodyPoint(r, corpse_, 0.3f));
    const math::Vec2 corpseCellPx = [&] {
        math::Vec2 p;
        fx::worldToScreen(cam, grid_->cellToWorld(1, 4) + math::Vec3(0.35f, 0.0f, 0.35f), w, h, p);
        return p;
    }();
    checks_->check(kCorpse, corpseBody.unit == nullptr && pickAt(corpseCellPx).unit == nullptr);

    bool found = false;
    math::Vec2 overlap;
    const pick::Body fb = pick::project(cam, w, h, *front_, picker_.options());
    const pick::Body bb = pick::project(cam, w, h, *back_, picker_.options());
    for (float t = 0.0f; t <= 1.0f && !found; t += 0.02f) {
        const math::Vec2 p = bodyPoint(r, back_, t);
        found = pick::distanceToBody(fb, p.x, p.y) <= fb.radiusPx && pick::distanceToBody(bb, p.x, p.y) <= bb.radiusPx;
        if (found) overlap = p;
    }
    if (!found) aout << "[picklab] front and back bodies do not overlap on screen" << std::endl;

    bool intentOk = false, cycleOk = false;
    if (found) {
        picker_.resetCycle();
        const pick::Result plain = pickAt(overlap);
        picker_.resetCycle();
        const pick::Result toBack = pickAt(overlap, [&](const game::Unit* u) { return u == back_; });
        picker_.resetCycle();
        const pick::Result toFront = pickAt(overlap, [&](const game::Unit* u) { return u == front_; });
        intentOk = plain.unit == front_ && plain.candidates >= 2 && toBack.unit == back_ && toFront.unit == front_;

        picker_.resetCycle();
        const game::Unit* first = pickAt(overlap).unit;
        const game::Unit* second = pickAt(overlap).unit;
        const game::Unit* third = pickAt(overlap).unit;
        pickAt(overlap + math::Vec2(30.0f, 30.0f));
        const game::Unit* restarted = pickAt(overlap).unit;
        cycleOk = first == front_ && second == back_ && third == front_ && restarted == front_;
    }
    checks_->check(kIntent, intentOk);
    checks_->check(kCycle, cycleOk);
    picker_.resetCycle();
}

void PickLab::update(float dt, Renderer& r) {
    if (root_) root_->update(dt);
    if (builder_) builder_->updateAll(dt);
    stepT_ += dt;
    if (autoOn_) {
        switch (step_) {
            case 0:
                if (stepT_ < 0.3f) break;
                runChecks(r);
                {
                    const pick::Body lb = pick::project(r.camera(), r.width(), r.height(), *lone_, picker_.options());
                    const math::Vec2 mid = (lb.feet + lb.head) * 0.5f;
                    selected_ = nullptr;
                    const int tapsBefore = taps_;
                    r.pointerDown(0, mid.x, mid.y);
                    r.pointerMove(0, mid.x + 20.0f, mid.y);
                    r.pointerMove(0, mid.x + 40.0f, mid.y);
                    r.pointerUp(0, mid.x + 40.0f, mid.y);
                    const bool dragQuiet = taps_ == tapsBefore && selected_ == nullptr;
                    resetCamera(r);
                    r.pointerDown(0, mid.x, mid.y);
                    r.pointerUp(0, mid.x, mid.y);
                    checks_->check(kDragNoTap, dragQuiet && taps_ == tapsBefore + 1 && selected_ == lone_);
                    r.hover(mid.x, mid.y);
                    const bool overLone = hovered_ == lone_;
                    r.hover(mid.x + 300.0f, mid.y - 200.0f);
                    checks_->check(kHover, overLone && hovered_ == nullptr);
                    r.hover(mid.x, mid.y);
                }
                aout << "[picklab] auto-drive done: " << checks_->passedCount() << "/" << checks_->size()
                     << " checks green" << std::endl;
                step_ = 1;
                break;
            default:
                break;
        }
    }
    placeRing(selRing_, selected_);
    placeRing(hoverRing_, hovered_);
    if (infoText_) infoText_->setText(info_);
    if (panel_) panel_->refresh();
}

void PickLab::render(Renderer& r) {
    r.renderSceneGraph(root_.get());
    r.renderSceneGraph(rings_.get());
    r.renderUI(ui_.get());
}
