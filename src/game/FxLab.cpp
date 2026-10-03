#include "FxLab.h"

#include <algorithm>
#include <cmath>
#include <string>

#include <GLES3/gl3.h>

#include "Engine.h"
#include "GameState.h"
#include "HubScene.h"
#include "Unit.h"

namespace {

constexpr float kFovY   = 30.0f * math::PI / 180.0f;
constexpr float kNear   = 0.1f;
constexpr float kFar    = 100.0f;
const math::Vec3 kEye(0.0f, 2.0f, 9.0f);
const math::Vec3 kLookAt(0.0f, 3.0f, 0.0f);
const math::Vec3 kTarget(0.0f, 1.0f, 0.0f);
const math::Vec4 kBurstColor(1.0f, 0.6f, 0.1f, 1.0f);
constexpr float kRainAlpha = 0.5f;
constexpr float kRainSize  = 0.06f;

const char* kBurstFades   = "BURST ALPHA FADES";
const char* kRainAlphaRow = "RAIN KEEPS ALPHA";
const char* kIboGrows     = "PARTICLE IBO GROWS";
const char* kGravityKnob  = "GRAVITY KNOB";
const char* kShrinkFps    = "SHRINK FPS-INDEP.";
const char* kRainColor    = "RAIN COLOR";
const char* kSkyKnobs     = "SKY KNOBS";
const char* kStormMix     = "STORM MIX";
const char* kStarsNight   = "STARS AT NIGHT";
const char* kStarKnobs    = "STAR KNOBS";
const char* kLightning    = "LIGHTNING";
const char* kBlendState   = "BLEND STATE";
const char* kDamageNumber = "DAMAGE NUMBER";
const char* kStatusIcon   = "STATUS ICON";
const char* kTrail        = "TRAIL";
constexpr int kBoltDamage = 5;

const engine::wx::Weather kWx[] = {
    engine::wx::Weather::Clear, engine::wx::Weather::Rain, engine::wx::Weather::Storm};
const char* kWxLabels[] = {"CLEAR", "RAIN", "STORM"};
const float kGrav[]     = {0.0f, -1.0f, -2.0f, -4.0f, -8.0f};
const float kRainRates[] = {60.0f, 300.0f, 900.0f};

float meanVy(const particle::Emitter& e) {
    const int n = e.activeCount();
    if (n == 0) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) sum += e.particles()[i].velocity.y;
    return sum / static_cast<float>(n);
}

int starsInView(const engine::stars::StarField& field, const math::Mat4& vp,
                float dayPhase, const math::Vec3& skyAxis) {
    const float angle = math::TWO_PI * dayPhase;
    const float c = std::cos(angle), s = std::sin(angle);
    const math::Vec3 axis = skyAxis.normalized();
    int n = 0;
    for (const auto& star : field.stars) {
        const math::Vec3& v = star.dir;
        const math::Vec3 r = v * c + math::cross(axis, v) * s + axis * (math::dot(axis, v) * (1.0f - c));
        const float x = vp.m[0] * r.x + vp.m[4] * r.y + vp.m[8]  * r.z;
        const float y = vp.m[1] * r.x + vp.m[5] * r.y + vp.m[9]  * r.z;
        const float w = vp.m[3] * r.x + vp.m[7] * r.y + vp.m[11] * r.z;
        if (w > 0.0f && std::abs(x) < w && std::abs(y) < w) ++n;
    }
    return n;
}

bool sunDiffers(const engine::sky::SunState& a, const engine::sky::SunState& b) {
    const math::Vec3 ds = a.skyColor - b.skyColor;
    const math::Vec3 da = a.ambientColor - b.ambientColor;
    return std::abs(ds.x) + std::abs(ds.y) + std::abs(ds.z) > 1e-4f ||
           std::abs(da.x) + std::abs(da.y) + std::abs(da.z) > 1e-4f ||
           std::abs(a.ambient - b.ambient) > 1e-4f ||
           std::abs(a.starIntensity - b.starIntensity) > 1e-4f;
}

bool configChangesSun(const game::ClimatePalettes& p, const engine::sky::SkyConfig& base,
                      const engine::sky::SkyConfig& changed, engine::wx::Weather wx) {
    for (int i = 0; i <= 48; ++i) {
        const float t = static_cast<float>(i) / 48.0f;
        if (sunDiffers(engine::sky::computeSunState(t, p.sky, wx, p.weather, base),
                       engine::sky::computeSunState(t, p.sky, wx, p.weather, changed)))
            return true;
    }
    return false;
}

}

void FxLab::enter(Renderer& r) {
    savedSky_   = r.skyConfig();
    savedStars_ = r.starRenderConfig();
    r.setPickRoot(nullptr);

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
    SceneNode::Drawable floor{plane, planeModel, math::Vec3(0.45f, 0.50f, 0.40f), lit};
    floor.castShadow = false;
    ground->addDrawable(floor);
    const math::Vec3 cubePos[] = {{-3, 0.5f, 0}, {0, 0.5f, -3}, {3, 0.5f, 0}};
    for (const auto& p : cubePos) {
        SceneNode* c = root_->addChild();
        c->setLocal(math::Mat4::translation(p));
        c->addDrawable({cube, cubeModel, math::Vec3(0.7f, 0.7f, 0.75f), lit});
    }

    {
        particle::Emitter::Config rc;
        rc.gravity             = math::Vec3(0.0f, -8.0f, 0.0f);
        rc.upVelocityMult      = 0.0f;
        rc.sizeJitter          = 0.0f;
        rc.lifetimeJitterMin   = 0.6f;
        rc.lifetimeJitterRange = 0.4f;
        rc.sizeShrinkFactor    = 0.0f;
        rain_.setConfig(rc);
    }

    std::vector<uint8_t> bytes;
    if (r.platform().readAsset("stars.bin", bytes))
        engine::stars::loadStars(bytes.data(), bytes.size(), stars_);

    palettes_ = game::defaultPalettes();
    clock_.setDateTime(1, 1, 1, 12, 0, 0);
    climate_ = game::ClimateState{};
    climate_.clock = &clock_;

    r.camera().setPerspective(kFovY,
        r.width() > 0 ? static_cast<float>(r.width()) / static_cast<float>(r.height()) : 1.0f,
        kNear, kFar);
    r.camera().setViewLookAt(kEye, kLookAt, math::Vec3(0, 1, 0));

    ui_ = std::make_unique<ui::UIRoot>();
    ui_->init();
    checks_ = std::make_unique<game::LabChecklist>(math::Vec2(16, 16));
    for (const char* row : {kBurstFades, kRainAlphaRow, kIboGrows, kGravityKnob, kShrinkFps,
                            kRainColor, kSkyKnobs, kStormMix, kStarsNight, kStarKnobs,
                            kLightning, kBlendState, kDamageNumber, kStatusIcon, kTrail})
        checks_->add(row);
    buildUI(r);
    restartAutoDrive();

    Renderer* rptr = &r;
    auto alive = alive_;
    FxLab* self = this;
    subs_.push_back(EventBus::on<PointerDownEvent>([alive, self, rptr](const PointerDownEvent& e) {
        if (!*alive || !self->ui_) return;
        if (ui::UIButton* btn = self->ui_->hitTestButton(math::Vec2(e.x, e.y))) {
            btn->handleClick();
            rptr->cancelPan();
        }
    }));
}

void FxLab::exit(Renderer& r) {
    *alive_ = false;
    for (auto id : subs_) EventBus::unsubscribe(id);
    subs_.clear();
    r.setSkyConfig(savedSky_);
    r.setStarRenderConfig(savedStars_);
    fxActions_.reset();
    archer_.reset();
    dummy_.reset();
    fxGrid_.reset();
    fxRoot_.reset();
    ui_.reset();
    panel_.reset();
    checks_.reset();
    root_.reset();
    assets_.reset();
}

void FxLab::setWeather(engine::wx::Weather w) {
    for (int i = 0; i < 3; ++i) if (kWx[i] == w) wxIdx_ = i;
    climate_.setWeather(w, static_cast<int>(clock_.totalSeconds()));
    if (w == engine::wx::Weather::Storm) {
        stormStart_ = autoT_;
        sawFlash_ = false;
    }
}

void FxLab::restartAutoDrive() {
    autoT_ = 0.0f;
    step_  = 0;
    sawFlash_ = false;
    sawDecay_ = false;
    stormStart_ = -1.0f;
    starsAtNight_ = false;
    starsOffNoon_ = false;
    if (checks_) checks_->reset();
}

void FxLab::buildUI(Renderer& r) {
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
    panel_->addRow("WEATHER", [this]() { return std::string(kWxLabels[wxIdx_]); },
                   [this]() { setWeather(kWx[(wxIdx_ + 1) % 3]); });
    panel_->addAction("+3 HOURS", [this]() { clock_.addSeconds(3.0 * 3600.0); });
    panel_->addRow("TIMELAPSE", [this]() { return std::string(timeLapse_ ? "ON" : "OFF"); },
                   [this]() {
                       timeLapse_ = !timeLapse_;
                       clock_.setTimeScale(timeLapse_ ? 7200.0f : 1.0f);
                   });
    panel_->addRow("GRAVITY", [this]() {
                       char buf[16]; snprintf(buf, sizeof(buf), "%g", kGrav[gravIdx_]); return std::string(buf);
                   },
                   [this]() {
                       gravIdx_ = (gravIdx_ + 1) % 5;
                       burst_.setGravity(math::Vec3(0.0f, kGrav[gravIdx_], 0.0f));
                   });
    panel_->addRow("RAIN RATE", [this]() { return std::to_string(static_cast<int>(rainRate_)); },
                   [this]() {
                       int i = 0;
                       while (i < 3 && kRainRates[i] != rainRate_) ++i;
                       rainRate_ = kRainRates[(i + 1) % 3];
                   });
    panel_->addAction("BURST", [this]() { burst_.burst(kTarget, 16, 2.5f, kBurstColor, 0.3f, 1.5f); });
    panel_->addAction("BACK", [rptr, gs]() {
        rptr->sceneManager().replace(std::make_unique<HubScene>(*gs));
    });
    auto layer = std::make_unique<ui::UIPanel>(math::Vec2(0, 0),
        math::Vec2(static_cast<float>(w), static_cast<float>(h)), math::Vec4(0, 0, 0, 0));
    numbers_.attach(layer.get());
    icons_.attach(layer.get());
    root->addChild(std::move(layer));
    root->addChild(panel_->build());
    root->addChild(checks_->build());
    ui_->setRoot(std::move(root));
}

void FxLab::startGameplayFx(Renderer& r) {
    const Mesh* cube = assets_->mesh("cube");
    const Model* cubeModel = assets_->model("cube");
    const Material* lit = assets_->material("lit");
    fxActions_ = std::make_unique<action::Manager>();
    archer_.reset();
    dummy_.reset();
    fxGrid_ = std::make_unique<grid::BattleGrid>(8, 5, 1.0f);
    fxRoot_ = std::make_unique<SceneNode>();
    projRoot_ = fxRoot_->addChild();
    auto unitAt = [&](int x, unit::Faction f, math::Vec3 color) {
        SceneNode* n = fxRoot_->addChild();
        const math::Vec3 p = fxGrid_->cellToWorld(x, 4);
        n->setLocal(math::Mat4::translation(p + math::Vec3(0.0f, 0.4f, 0.0f)) *
                    math::Mat4::scale(math::Vec3(0.5f, 0.8f, 0.5f)));
        n->addDrawable({cube, cubeModel, color, lit});
        auto u = std::make_unique<game::Unit>(n, f, unit::Stats{12, 6, 1, 3, 6});
        u->setCell(math::Vec2(static_cast<float>(x), 4.0f));
        u->setGrid(fxGrid_.get());
        return u;
    };
    archer_ = unitAt(1, unit::Faction::Player, math::Vec3(0.3f, 0.8f, 0.4f));
    dummy_ = unitAt(5, unit::Faction::Enemy, math::Vec3(0.85f, 0.25f, 0.2f));
    r.camera().setViewLookAt(math::Vec3(0.0f, 4.5f, 10.0f), math::Vec3(0.0f, 0.6f, 2.0f), math::Vec3(0, 1, 0));

    frame_ = 0;
    hitFrame_ = numberFrame_ = -1;
    numberRose_ = numberFaded_ = sawTrail_ = trailOff_ = iconAbove_ = false;
    numberText_.clear();
    iconSeen_.clear();
    projPath_.clear();
    trail_.clear();
    numbers_.clear();

    game::Unit* target = dummy_.get();
    auto bolt = std::make_unique<action::RangedAttackAction>(
        archer_.get(), target, kBoltDamage, fxGrid_.get(), projRoot_,
        SceneNode::Drawable{cube, cubeModel, math::Vec3(1.0f, 0.9f, 0.3f), lit}, true, 6.0f);
    bolt->setStatus(unit::StatusEffect{unit::StatusType::Stunned, 1, 0});
    bolt->setHitCallback([this, target]() {
        hitFrame_ = frame_;
        target->applyStatus({unit::StatusType::Poisoned, 2, 1});
        const math::Mat4 m = target->node()->world();
        const math::Vec3 head(m.m[12], m.m[13] + 1.4f, m.m[14]);
        numbers_.spawn(head, std::to_string(kBoltDamage), math::Vec4(1.0f, 0.35f, 0.25f, 1.0f), 1.0f, 0.9f);
        burst_.burst(math::Vec3(m.m[12], m.m[13], m.m[14]), 12, 2.0f, kBurstColor, 0.25f, 0.6f);
    });
    fxActions_->enqueue(std::move(bolt));
}

void FxLab::updateGameplayFx(float dt, Renderer& r) {
    if (!fxActions_) return;
    ++frame_;
    fxActions_->update(dt);
    if (!projRoot_->children().empty()) {
        const math::Mat4 m = projRoot_->children().front()->world();
        const math::Vec3 p(m.m[12], m.m[13], m.m[14]);
        projPath_.push_back(p);
        fx::emitTrail(trail_, p, dt);
        sawTrail_ = true;
    }
    trail_.update(dt);
    for (int i = 0; i < trail_.activeCount(); ++i) {
        const auto& tp = trail_.particles()[i];
        if (tp.age > 0.05f) continue;
        float best = 1e9f;
        for (const auto& q : projPath_) best = std::min(best, (tp.position - q).length());
        if (best > 0.4f) trailOff_ = true;
    }
    numbers_.update(dt);
    const int w = r.width() > 0 ? r.width() : 1280;
    const int h = r.height() > 0 ? r.height() : 720;
    numbers_.layout(r.camera(), w, h);
    if (!numbers_.entries().empty()) {
        const auto& e = numbers_.entries().front();
        if (numberFrame_ < 0) {
            numberFrame_ = frame_;
            numberY0_ = e.screen.y;
            numberA0_ = e.alpha();
            numberText_ = e.text;
        } else {
            if (e.screen.y < numberY0_ - 5.0f) numberRose_ = true;
            if (e.alpha() < numberA0_ - 0.3f) numberFaded_ = true;
        }
    }
    icons_.update({archer_.get(), dummy_.get()}, r.camera(), w, h);
    if (!icons_.textFor(1).empty()) {
        iconSeen_ = icons_.textFor(1);
        math::Vec2 base;
        const math::Mat4 m = dummy_->node()->world();
        if (fx::worldToScreen(r.camera(), math::Vec3(m.m[12], m.m[13], m.m[14]), w, h, base))
            iconAbove_ = icons_.screenFor(1).y < base.y;
    }
}

void FxLab::onResize(int w, int h, Renderer& r) {
    if (w < 1 || h < 1 || !ui_) return;
    r.camera().setPerspective(kFovY, static_cast<float>(w) / static_cast<float>(h), kNear, kFar);
    buildUI(r);
}

void FxLab::checkInstant(Renderer& r) {
    {
        particle::Emitter a(1), b(1);
        a.emit({0, 0, 0}, {0, 0, 0}, {1, 1, 1, 1}, 1.0f, 1.0f);
        b.emit({0, 0, 0}, {0, 0, 0}, {1, 1, 1, 1}, 1.0f, 1.0f);
        for (int i = 0; i < 15; ++i) a.update(1.0f / 30.0f);
        for (int i = 0; i < 30; ++i) b.update(1.0f / 60.0f);
        checks_->check(kShrinkFps,
            std::abs(a.particles()[0].size - b.particles()[0].size) < 1e-4f &&
            std::abs(a.particles()[0].color.w - b.particles()[0].color.w) < 1e-4f);
    }

    {
        using Setter = void (Renderer::*)(float);
        struct Knob { const char* name; float engine::sky::SkyConfig::*field; Setter set; };
        const Knob knobs[] = {
            {"nightBlendShift", &engine::sky::SkyConfig::nightBlendShift, &Renderer::setSkyNightBlendShift},
            {"nightBlendScale", &engine::sky::SkyConfig::nightBlendScale, &Renderer::setSkyNightBlendScale},
            {"dawnStart",       &engine::sky::SkyConfig::dawnStart,       &Renderer::setSkyDawnStart},
            {"dawnWidth",       &engine::sky::SkyConfig::dawnWidth,       &Renderer::setSkyDawnWidth},
            {"noonScale",       &engine::sky::SkyConfig::noonScale,       &Renderer::setSkyNoonScale},
            {"ambientDawnMix",  &engine::sky::SkyConfig::ambientDawnMix,  &Renderer::setSkyAmbientDawnMix},
            {"ambientSunScale", &engine::sky::SkyConfig::ambientSunScale, &Renderer::setSkyAmbientSunScale},
            {"ambientBaseline", &engine::sky::SkyConfig::ambientBaseline, &Renderer::setSkyAmbientBaseline},
        };
        const engine::sky::SkyConfig base = r.skyConfig();
        bool all = true;
        for (const auto& k : knobs) {
            const float v = base.*k.field + 0.2f;
            (r.*k.set)(v);
            const bool stored = std::abs(r.skyConfig().*k.field - v) < 1e-6f;
            const bool visible = configChangesSun(palettes_, base, r.skyConfig(),
                                                  engine::wx::Weather::Clear);
            r.setSkyConfig(base);
            if (!stored || !visible)
                aout << "[fxlab] sky knob " << k.name << " stored=" << stored
                     << " visible=" << visible << std::endl;
            all = all && stored && visible;
        }
        checks_->check(kSkyKnobs, all);
    }

    {
        engine::sky::SkyConfig lo = r.skyConfig(), hi = r.skyConfig();
        lo.stormSkyMix = 0.0f;
        hi.stormSkyMix = 1.0f;
        const auto a = engine::sky::computeSunState(0.5f, palettes_.sky, engine::wx::Weather::Storm,
                                                    palettes_.weather, lo);
        const auto b = engine::sky::computeSunState(0.5f, palettes_.sky, engine::wx::Weather::Storm,
                                                    palettes_.weather, hi);
        checks_->check(kStormMix, sunDiffers(a, b));
    }

    {
        const Renderer::StarRenderConfig saved = climate_.stars;
        climate_.stars.pointSize = saved.pointSize + 3.0f;
        climate_.stars.brightnessScale = saved.brightnessScale * 0.5f;
        climate_.applyToRenderer(r);
        const auto& got = r.starRenderConfig();
        checks_->check(kStarKnobs,
            std::abs(got.pointSize - climate_.stars.pointSize) < 1e-6f &&
            std::abs(got.brightnessScale - climate_.stars.brightnessScale) < 1e-6f);
        climate_.stars = saved;
        climate_.applyToRenderer(r);
    }
}

void FxLab::autoDrive(float dt, Renderer& r) {
    autoT_ += dt;
    switch (step_) {
        case 0:
            setWeather(engine::wx::Weather::Clear);
            clock_.setTimeScale(1.0f);
            clock_.setTimeOfDay(12, 0, 0);
            rainRate_ = 60.0f;
            checkInstant(r);
            burst_.clear();
            burst_.setGravity(math::Vec3(0.0f, 0.0f, 0.0f));
            burst_.burst(kTarget, 16, 2.5f, kBurstColor, 0.3f, 1.5f);
            step_ = 1;
            break;
        case 1:
            if (autoT_ < 0.5f) break;
            gravityVy0_ = meanVy(burst_);
            burst_.clear();
            burst_.setGravity(math::Vec3(0.0f, -8.0f, 0.0f));
            burst_.burst(kTarget, 16, 2.5f, kBurstColor, 0.3f, 1.5f);
            step_ = 2;
            break;
        case 2:
            if (autoT_ < 1.0f) break;
            checks_->check(kGravityKnob, gravityVy0_ > 0.0f && meanVy(burst_) < 0.0f);
            burst_.setGravity(math::Vec3(0.0f, kGrav[gravIdx_], 0.0f));
            step_ = 3;
            break;
        case 3:
            if (autoT_ < 2.0f) break;
            setWeather(engine::wx::Weather::Rain);
            step_ = 4;
            break;
        case 4:
            if (autoT_ < 6.0f) break;
            climate_.lightning.baseDelay = 1.0f;
            climate_.lightning.jitterRange = 1.0f;
            setWeather(engine::wx::Weather::Storm);
            rainRate_ = 900.0f;
            step_ = 5;
            break;
        case 5:
            if (autoT_ < 16.0f) break;
            climate_.lightning = engine::wx::LightningState{};
            setWeather(engine::wx::Weather::Clear);
            rainRate_ = 60.0f;
            clock_.setTimeOfDay(12, 0, 0);
            clock_.setTimeScale(7200.0f);
            timeLapse_ = true;
            step_ = 6;
            break;
        case 6:
            if (autoT_ < 28.0f) break;
            clock_.setTimeScale(1.0f);
            clock_.setTimeOfDay(12, 0, 0);
            timeLapse_ = false;
            startGameplayFx(r);
            step_ = 7;
            break;
        case 7:
            if (autoT_ < 30.2f || !fxActions_->isIdle()) break;
            checks_->check(kTrail, sawTrail_ && !trailOff_ && projPath_.size() >= 5);
            checks_->check(kDamageNumber, hitFrame_ >= 0 && numberFrame_ == hitFrame_ &&
                                          numberText_ == std::to_string(kBoltDamage) && numberRose_ &&
                                          numberFaded_ && numbers_.entries().empty());
            if (!checks_->passed(kDamageNumber))
                aout << "[fxlab] number hit=" << hitFrame_ << " seen=" << numberFrame_ << " rose=" << numberRose_
                     << " faded=" << numberFaded_ << std::endl;
            dummy_->clearStatuses();
            step_ = 8;
            break;
        case 8:
            if (autoT_ < 30.4f) break;
            checks_->check(kStatusIcon, iconSeen_ == "STN PSN" && iconAbove_ && icons_.textFor(1).empty());
            r.camera().setViewLookAt(kEye, kLookAt, math::Vec3(0, 1, 0));
            aout << "[fxlab] auto-drive done: " << checks_->passedCount() << "/"
                 << checks_->size() << " checks green" << std::endl;
            step_ = 9;
            break;
        default:
            break;
    }
    if (panel_) panel_->refresh();
}

void FxLab::emitRain(float dt, Renderer& r) {
    if (!climate_.isRaining()) {
        rain_.clear();
        rainAccum_ = 0.0f;
        return;
    }
    rainAccum_ += rainRate_ * dt;
    const int count = static_cast<int>(rainAccum_);
    rainAccum_ -= static_cast<float>(count);
    const math::Vec3 eye = r.camera().eye();
    const math::Vec3& rc = palettes_.weather.rainColor;
    for (int i = 0; i < count; ++i) {
        rain_.emit(engine::wx::rainSpawnPos(eye), math::Vec3(0.0f, -5.0f, 0.0f),
                   math::Vec4(rc.x, rc.y, rc.z, kRainAlpha), kRainSize, 1.0f);
    }
}

void FxLab::checkParticles(Renderer& r) {
    int samples = 0, bad = 0;
    for (int i = 0; i < burst_.activeCount(); ++i) {
        const auto& p = burst_.particles()[i];
        const float t = p.age / p.lifetime;
        if (t < 0.45f || t > 0.55f) continue;
        ++samples;
        const float expected = p.baseAlpha * (1.0f - t);
        if (std::abs(p.color.w - expected) > 0.01f || std::abs(p.color.w - 0.5f * p.baseAlpha) > 0.06f) ++bad;
    }
    if (samples > 0) checks_->check(kBurstFades, bad == 0);

    const math::Vec3& rc = palettes_.weather.rainColor;
    for (int i = 0; i < rain_.activeCount(); ++i) {
        const auto& p = rain_.particles()[i];
        if (p.age > 0.05f) continue;
        checks_->check(kRainAlphaRow, p.color.w <= kRainAlpha + 1e-4f && p.color.w > kRainAlpha * 0.9f);
        checks_->check(kRainColor, std::abs(p.color.x - rc.x) < 1e-5f &&
                                   std::abs(p.color.y - rc.y) < 1e-5f &&
                                   std::abs(p.color.z - rc.z) < 1e-5f);
        break;
    }

    if (climate_.current == engine::wx::Weather::Storm) {
        const auto& l = climate_.lightning;
        if (!sawFlash_ && l.flashIntensity > 0.0f) {
            sawFlash_ = true;
            const float waited = autoT_ - stormStart_;
            if (stormStart_ >= 0.0f && waited > l.baseDelay + l.jitterRange + 0.2f) {
                aout << "[fxlab] first flash late: " << waited << "s" << std::endl;
                sawFlash_ = false;
                stormStart_ = autoT_;
            }
        }
        if (sawFlash_ && l.flashIntensity == 0.0f) sawDecay_ = true;
        checks_->check(kLightning, sawFlash_ && sawDecay_);
    }

    const int hour = clock_.now().hour;
    if (hour == 0 && sun_.starIntensity > 0.0f &&
        starsInView(stars_, r.camera().viewProjection(), climate_.dayPhase(), climate_.stars.skyAxis) >= 10)
        starsAtNight_ = true;
    if (hour == 12 && sun_.starIntensity == 0.0f) starsOffNoon_ = true;
    checks_->check(kStarsNight, starsAtNight_ && starsOffNoon_);
}

void FxLab::update(float dt, Renderer& r) {
    if (autoOn_) autoDrive(dt, r);

    clock_.tick(dt);
    climate_.update(dt);
    climate_.applyToRenderer(r);
    sun_ = engine::sky::computeSunState(climate_.dayPhase(), palettes_.sky, climate_.current,
                                        palettes_.weather, r.skyConfig(), climate_.stars.skyAxis);
    r.setClearColor(sun_.skyColor);
    r.setFrameLight(sun_.lightDir, sun_.ambient);

    burstTimer_ += dt;
    if (burstTimer_ >= 1.0f) {
        burstTimer_ = 0.0f;
        burst_.burst(kTarget, 8, 2.0f, kBurstColor, 0.3f, 1.5f);
    }

    emitRain(dt, r);
    updateGameplayFx(dt, r);
    burst_.update(dt);
    rain_.update(dt);
    if (root_) root_->update(dt);
    checkParticles(r);
}

void FxLab::render(Renderer& r) {
    r.renderSceneGraph(root_.get());
    if (fxRoot_) r.renderSceneGraph(fxRoot_.get());
    r.renderParticles(trail_);
    r.renderParticles(burst_);
    while (glGetError() != GL_NO_ERROR) {}
    r.renderParticles(rain_);
    if (rain_.activeCount() > 512) checks_->check(kIboGrows, glGetError() == GL_NO_ERROR);
    r.renderStars(stars_, sun_.starIntensity, climate_.dayPhase());
    if (climate_.current == engine::wx::Weather::Storm)
        r.renderLightningFlash(palettes_.weather.lightningColor, climate_.lightning.flashIntensity);
    r.renderUI(ui_.get());
    checks_->check(kBlendState, glIsEnabled(GL_BLEND) == GL_FALSE);
}
