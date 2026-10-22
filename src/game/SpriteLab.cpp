#include "SpriteLab.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "Engine.h"
#include "GameState.h"
#include "HubScene.h"

namespace {

constexpr float kFovY = 30.0f * math::PI / 180.0f;
constexpr float kNear = 0.1f;
constexpr float kFar  = 100.0f;
constexpr float kPitch = Camera::kIsoPitchDeg;
constexpr float kDistance = 9.0f;
constexpr float kTurnDegPerSec = 300.0f;
constexpr int   kGridW = 7;
constexpr int   kGridH = 5;
const char* kClipNames[] = {"Idle", "Walk", "Attack", "Hurt", "Death"};

const char* kSheetRow   = "SHEET LOADS";
const char* kNearestRow = "NEAREST FILTER";
const char* kFeetRow    = "FEET ON CELL";
const char* kFaceRow    = "FACES CAMERA";
const char* kFacingRow  = "FACING ROWS";
const char* kHitRow     = "HIT EVENT ONCE";
const char* kFramesRow  = "CLIP FRAMES";
const char* kShadowRow  = "CUTOUT SHADOW";
const char* kPickRow    = "PICK RECT";
const char* kSquareRow  = "SQUARE PIXELS";

const char* kSheets[] = {"sprite_test", "knight_sprite", "archer_sprite", "goblin_sprite", "mage_sprite",
                         "slinger_sprite", "knight_mounted_sprite"};

struct Placement { int x, y; float yaw; const char* sheet; };
const Placement kPlacements[] = {
    {1, 1, 0.0f, "sprite_test"}, {2, 1, 90.0f, "sprite_test"}, {3, 1, 180.0f, "sprite_test"},
    {4, 1, 270.0f, "sprite_test"}, {2, 2, 30.0f, "sprite_test"}, {4, 0, 0.0f, "sprite_test"},
    {0, 3, 0.0f, "knight_sprite"}, {1, 3, 0.0f, "archer_sprite"}, {2, 3, 0.0f, "goblin_sprite"},
    {3, 3, 0.0f, "mage_sprite"}, {4, 3, 0.0f, "slinger_sprite"}, {6, 3, 90.0f, "knight_mounted_sprite"},
    {1, 4, 90.0f, "knight_sprite"}, {3, 4, 180.0f, "goblin_sprite"}, {5, 4, 270.0f, "archer_sprite"},
};

}

SpriteLab::SpriteLab(game::GameState& gs) : gs_(gs) {}
SpriteLab::~SpriteLab() = default;

void SpriteLab::enter(Renderer& r) {
    r.setPickRoot(nullptr);
    assets_ = std::make_unique<game::LabAssets>(r.platform());
    loadSheets(r);
    root_ = std::make_unique<SceneNode>();

    ui_ = std::make_unique<ui::UIRoot>();
    ui_->init();
    checks_ = std::make_unique<game::LabChecklist>(math::Vec2(16, 16));
    for (const char* row : {kSheetRow, kNearestRow, kFeetRow, kFaceRow, kFacingRow, kHitRow, kFramesRow,
                            kShadowRow, kPickRow, kSquareRow})
        checks_->add(row);
    resetCamera(r);
    restart();
    buildUI(r);

    Renderer* rptr = &r;
    auto alive = alive_;
    SpriteLab* self = this;
    subs_.push_back(EventBus::on<PointerDownEvent>([alive, self, rptr](const PointerDownEvent& e) {
        if (!*alive || !self->ui_) return;
        if (ui::UIButton* btn = self->ui_->hitTestButton(math::Vec2(e.x, e.y))) {
            btn->handleClick();
            rptr->cancelPan();
        }
    }));
    subs_.push_back(EventBus::on<TapEvent>([alive, self, rptr](const TapEvent& e) {
        if (!*alive || !self->ui_ || self->ui_->hitTestButton(math::Vec2(e.x, e.y))) return;
        self->picked_ = self->pickAt(*rptr, e.x, e.y);
    }));
}

void SpriteLab::exit(Renderer& r) {
    *alive_ = false;
    for (auto id : subs_) EventBus::unsubscribe(id);
    subs_.clear();
    actors_.clear();
    grid_.reset();
    ui_.reset();
    panel_.reset();
    checks_.reset();
    root_.reset();
    assets_.reset();
    sheets_.clear();
}

void SpriteLab::loadSheets(Renderer& r) {
    sheets_.clear();
    sheets_.reserve(std::size(kSheets));
    for (const char* name : kSheets) {
        SheetAsset a;
        a.name = name;
        std::vector<uint8_t> bytes;
        std::string err;
        if (!r.platform().readAsset(std::string(name) + ".sheet", bytes) ||
            !sprite::parse(std::string(bytes.begin(), bytes.end()), a.sheet, &err)) {
            aout << "[spritelab] " << name << ".sheet: " << (err.empty() ? "missing" : err) << std::endl;
            continue;
        }
        a.texture = TextureAsset::loadAsset(r.platform(), std::string(name) + ".png", true);
        if (!a.texture) continue;
        a.sheet.atlasW = a.texture->width();
        a.sheet.atlasH = a.texture->height();
        assets_->addMesh("quad_" + a.name, sprite::makeQuad(a.sheet.anchorV()));
        sheets_.push_back(std::move(a));
    }
    assets_->upload();
    asset::Registry& reg = assets_->registry();
    const Shader* sh = reg.shader(reg.findShader("sprite"));
    for (SheetAsset& a : sheets_) reg.addMaterial("sprite_" + a.name, {sh, a.texture.get()});
    for (SheetAsset& a : sheets_) a.material = reg.material(reg.findMaterial("sprite_" + a.name));
}

void SpriteLab::spawn() {
    actors_.clear();
    grid_.reset();
    root_->clearChildren();
    grid_ = std::make_unique<grid::BattleGrid>(kGridW, kGridH, 1.0f);
    const Material* lit = assets_->material("lit");
    grid_->attachTo(root_.get(), assets_->mesh("plane"), assets_->model("plane"),
                    math::Vec3(0.62f, 0.66f, 0.52f), math::Vec3(0.25f, 0.25f, 0.28f), lit);

    SceneNode* pillar = root_->addChild();
    const math::Vec3 pc = grid_->cellToWorld(5, 1);
    pillar->setLocal(math::Mat4::translation(pc + math::Vec3(0.0f, 0.6f, 0.0f)) *
                     math::Mat4::scale(math::Vec3(0.6f, 1.2f, 0.6f)));
    pillar->addDrawable({assets_->mesh("cube"), assets_->model("cube"), math::Vec3(0.55f, 0.5f, 0.45f), lit});

    if (sheets_.empty()) return;
    for (const Placement& p : kPlacements) {
        const auto it = std::find_if(sheets_.begin(), sheets_.end(),
                                     [&p](const SheetAsset& a) { return a.name == p.sheet; });
        if (it == sheets_.end()) continue;
        Actor a;
        a.sheet = &*it;
        a.feet = grid_->cellToWorld(p.x, p.y);
        a.yawDeg = p.yaw;
        a.node = root_->addChild();
        SceneNode::Drawable d{assets_->mesh("quad_" + it->name), assets_->model("quad_" + it->name),
                              math::Vec3(1.0f), it->material};
        d.alphaCutout = true;
        a.node->addDrawable(d);
        actors_.push_back(std::move(a));
    }
    playClip(clip_);
}

void SpriteLab::restart() {
    step_ = 0;
    stepT_ = 0.0f;
    clip_ = 0;
    picked_ = -1;
    facingAll_ = true;
    framesSeen_.clear();
    if (checks_) checks_->reset();
    spawn();
}

void SpriteLab::playClip(int i) {
    clip_ = i;
    for (Actor& a : actors_) {
        a.player.play(a.sheet->sheet.find(kClipNames[i]));
        a.hits = 0;
    }
}

void SpriteLab::placeActors(Renderer& r) {
    const math::Mat4& view = r.camera().view();
    const float camYaw = sprite::cameraYawDeg(view);
    const float viewPitch = sprite::cameraPitchDeg(view);
    const float tilt = tilt_ ? viewPitch : 0.0f;
    for (size_t i = 0; i < actors_.size(); ++i) {
        Actor& a = actors_[i];
        const sprite::Sheet& s = a.sheet->sheet;
        a.facing = sprite::facingIndex(a.yawDeg, camYaw);
        auto& d = a.node->drawables().front();
        if (const sprite::Clip* c = a.player.clip())
            d.uvRect = sprite::frameRect(s, *c, a.player.frame(), sprite::viewFor(s, a.facing));
        d.color = static_cast<int>(i) == picked_ ? math::Vec3(1.0f, 1.0f, 0.55f) : math::Vec3(1.0f);
        a.node->setLocal(sprite::billboard(s, sprite::snapToTexels(a.feet, s.pixelsPerUnit), camYaw, tilt, viewPitch));
    }
}

sprite::ScreenRect SpriteLab::rectOf(Renderer& r, const Actor& a) const {
    return sprite::screenRect(a.node->world(), r.camera().viewProjection(), r.width(), r.height(),
                              a.sheet->sheet.anchorV());
}

int SpriteLab::pickAt(Renderer& r, float x, float y) const {
    int best = -1;
    float bestDist = 1e30f;
    for (size_t i = 0; i < actors_.size(); ++i) {
        if (!rectOf(r, actors_[i]).contains(x, y)) continue;
        const float d = (actors_[i].feet - r.camera().eye()).length();
        if (d < bestDist) { bestDist = d; best = static_cast<int>(i); }
    }
    return best;
}

bool SpriteLab::facingOk(Renderer& r) const {
    const math::Mat4& view = r.camera().view();
    const float camYaw = sprite::cameraYawDeg(view);
    const math::Vec3 camZ(view.m[2], view.m[6], view.m[10]);
    for (const Actor& a : actors_) {
        const sprite::Sheet& s = a.sheet->sheet;
        const int want = sprite::facingIndex(a.yawDeg, camYaw);
        const math::Vec4 rect = sprite::frameRect(s, *a.player.clip(), a.player.frame(), sprite::viewFor(s, want));
        if (a.facing != want || !math::approxEqual(a.node->drawables().front().uvRect, rect, 1e-6f)) return false;
        const math::Vec3 n = a.node->world().transformDirection(math::Vec3(0, 0, 1)).normalized();
        if (math::dot(n, camZ) < 0.999f) return false;
    }
    return true;
}

void SpriteLab::autoDrive(float dt, Renderer& r) {
    stepT_ += dt;
    const auto next = [this]() { ++step_; stepT_ = 0.0f; };
    if (step_ == 0) {
        if (stepT_ < 0.4f) return;
        bool sheetOk = sheets_.size() == std::size(kSheets) && !actors_.empty();
        bool nearest = sheetOk;
        for (const SheetAsset& a : sheets_) {
            sheetOk = sheetOk && a.material && sprite::fitsAtlas(a.sheet, a.texture->width(), a.texture->height()) &&
                      a.sheet.clips.size() == 5;
            nearest = nearest && a.texture->nearest();
        }
        bool feet = sheetOk;
        for (const Actor& a : actors_) {
            const sprite::Sheet& s = a.sheet->sheet;
            const math::Vec3 anchor(s.anchorX / s.frameW, 1.0f - s.anchorY / s.frameH, 0.0f);
            const math::Vec3 p = a.node->world().transformPoint(anchor);
            feet = feet && (p - a.feet).length() < 1.0f / s.pixelsPerUnit;
        }
        checks_->check(kSheetRow, sheetOk);
        checks_->check(kNearestRow, nearest);
        checks_->check(kFeetRow, feet);
        checks_->check(kShadowRow, sheetOk && r.shadowStats().cutoutDraws == static_cast<int>(actors_.size()));
        checks_->check(kFaceRow, sheetOk && facingOk(r));
        if (!sheetOk || !nearest || !feet)
            aout << "[spritelab] sheet=" << sheetOk << " nearest=" << nearest << " feet=" << feet << std::endl;
        yawTarget_ += 90.0f;
        next();
        return;
    }
    if (step_ <= 4) {
        if (stepT_ < 0.6f) return;
        const bool ok = facingOk(r);
        facingAll_ = facingAll_ && ok;
        if (!ok) aout << "[spritelab] facing wrong at camera yaw " << yaw_ << std::endl;
        if (step_ == 4) {
            checks_->check(kFacingRow, facingAll_);
            checks_->check(kFaceRow, facingAll_ && checks_->passed(kFaceRow));
            playClip(2);
        } else {
            yawTarget_ += 90.0f;
        }
        next();
        return;
    }
    if (step_ == 5) {
        if (stepT_ < 0.9f) return;
        bool once = !actors_.empty();
        for (const Actor& a : actors_) once = once && a.hits == 1 && a.player.finished();
        checks_->check(kHitRow, once);
        playClip(1);
        next();
        return;
    }
    if (step_ == 6) {
        const int f = actors_.empty() ? -1 : actors_[0].player.frame();
        if (std::find(framesSeen_.begin(), framesSeen_.end(), f) == framesSeen_.end()) framesSeen_.push_back(f);
        if (stepT_ < 0.7f) return;
        const bool walkCycles = framesSeen_.size() == 4;
        playClip(4);
        framesSeen_.clear();
        framesSeen_.push_back(walkCycles ? 1 : 0);
        next();
        return;
    }
    if (step_ == 7) {
        if (stepT_ < 1.1f) return;
        bool death = framesSeen_[0] == 1;
        for (const Actor& a : actors_)
            death = death && a.player.finished() && a.player.frame() == a.player.clip()->frames - 1;
        checks_->check(kFramesRow, death);
        playClip(0);
        next();
        return;
    }
    if (step_ == 8) {
        if (stepT_ < 0.2f) return;
        bool pick = actors_.size() > 4;
        if (pick) {
            const sprite::ScreenRect sr = rectOf(r, actors_[0]);
            const sprite::ScreenRect back = rectOf(r, actors_[1]);
            const sprite::ScreenRect front = rectOf(r, actors_[4]);
            const float ox = (std::max(back.x0, front.x0) + std::min(back.x1, front.x1)) * 0.5f;
            const float oy = (std::max(back.y0, front.y0) + std::min(back.y1, front.y1)) * 0.5f;
            pick = pickAt(r, (sr.x0 + sr.x1) * 0.5f, sr.y0 + (sr.y1 - sr.y0) * 0.6f) == 0 &&
                   back.contains(ox, oy) && pickAt(r, ox, oy) == 4 &&
                   pickAt(r, 4.0f, static_cast<float>(r.height()) - 4.0f) == -1;
            const sprite::Sheet& s = actors_[0].sheet->sheet;
            const float ratio = ((sr.y1 - sr.y0) / (sr.x1 - sr.x0)) / (static_cast<float>(s.frameH) / s.frameW);
            checks_->check(kSquareRow, std::fabs(ratio - 1.0f) < 0.08f);
            if (std::fabs(ratio - 1.0f) >= 0.08f) aout << "[spritelab] texel aspect " << ratio << std::endl;
        }
        checks_->check(kPickRow, pick);
        aout << "[spritelab] auto-drive done: " << checks_->passedCount() << "/" << checks_->size()
             << " checks green" << std::endl;
        next();
    }
}

void SpriteLab::resetCamera(Renderer& r) {
    Camera& cam = r.camera();
    cam.setPerspective(kFovY, cam.aspect(), kNear, kFar);
    cam.setTerrainHeightFn({});
    cam.clearTargetBounds();
    cam.setForward(math::Vec3(0, 0, -1));
    cam.setPitchLimits(Camera::kMinPitchDeg, Camera::kMaxPitchDeg);
    cam.setYawLimits(-3600.0f, 3600.0f);
    cam.setDistanceLimits(3.0f, 25.0f, kDistance);
    cam.setTarget(math::Vec3(0.0f, 0.4f, 0.0f));
    cam.setDistance(kDistance);
    yaw_ = yawTarget_ = 0.0f;
    cam.setOrbitSnap(kPitch, yaw_);
    r.setClearColor(math::Vec3(0.36f, 0.52f, 0.70f));
    r.setFrameLight(math::Vec3(-0.4f, 0.8f, 0.45f).normalized(), 0.45f);
}

void SpriteLab::buildUI(Renderer& r) {
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
    panel_->addRow("CLIP", [this]() { return std::string(kClipNames[clip_]); },
                   [this]() { autoOn_ = false; playClip((clip_ + 1) % 5); });
    panel_->addRow("TURN", [this]() {
                       const int deg = static_cast<int>(std::fmod(std::fmod(yawTarget_, 360.0f) + 360.0f, 360.0f));
                       return std::to_string(deg);
                   },
                   [this]() { autoOn_ = false; yawTarget_ += 90.0f; });
    panel_->addRow("TILT", [this]() { return std::string(tilt_ ? "ON" : "OFF"); },
                   [this]() { tilt_ = !tilt_; });
    panel_->addRow("AUTO", [this]() { return std::string(autoOn_ ? "ON" : "OFF"); },
                   [this]() { autoOn_ = !autoOn_; });
    panel_->addAction("RESTART CHECKS", [this, rptr]() {
        autoOn_ = true;
        tilt_ = true;
        resetCamera(*rptr);
        restart();
    });
    panel_->addAction("BACK", [rptr, gs]() {
        rptr->sceneManager().replace(std::make_unique<HubScene>(*gs));
    });
    root->addChild(panel_->build());
    root->addChild(checks_->build());
    ui_->setRoot(std::move(root));
}

void SpriteLab::onResize(int w, int h, Renderer& r) {
    if (w < 1 || h < 1 || !ui_) return;
    r.camera().setPerspective(kFovY, static_cast<float>(w) / static_cast<float>(h), kNear, kFar);
    buildUI(r);
}

void SpriteLab::update(float dt, Renderer& r) {
    if (yaw_ != yawTarget_) {
        const float stepDeg = kTurnDegPerSec * dt;
        yaw_ = std::fabs(yawTarget_ - yaw_) <= stepDeg ? yawTarget_ : yaw_ + std::copysign(stepDeg, yawTarget_ - yaw_);
        r.camera().setOrbitSnap(kPitch, yaw_);
    }
    for (Actor& a : actors_) {
        std::vector<int> events;
        a.player.update(dt, &events);
        a.hits += static_cast<int>(std::count(events.begin(), events.end(), 1));
    }
    placeActors(r);
    if (root_) root_->update(dt);
    if (autoOn_) autoDrive(dt, r);
    if (panel_) panel_->refresh();
}

void SpriteLab::render(Renderer& r) {
    r.renderSceneGraph(root_.get());
    r.renderUI(ui_.get());
}
