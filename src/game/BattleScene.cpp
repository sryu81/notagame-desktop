#include "BattleScene.h"

#include <algorithm>

#include "BattleData.h"
#include "GameState.h"
#include "Unit.h"

namespace battle {

Data& d() {
    static Data data;
    return data;
}

} // namespace battle

using battle::d;

void BattleScene::enter(Renderer& r) {
    d() = battle::Data{};
    d().levelId = levelId_;
    d().autoBattle = autoBattle_;
    if (autoBattle_) {
        d().speedIdx = 3;
        d().speed = 4.0f;
    }
    battle::createContent(r, gs_);
    r.setPickRoot(nullptr);
    {
        particle::Emitter::Config rc;
        rc.gravity             = math::Vec3(0.0f, -8.0f, 0.0f);
        rc.upVelocityMult      = 0.0f;
        rc.sizeJitter          = 0.0f;
        rc.lifetimeJitterMin   = 0.6f;
        rc.lifetimeJitterRange = 0.4f;
        rc.sizeShrinkFactor    = 0.0f;
        d().rain.setConfig(rc);
    }
    d().palettes = game::defaultPalettes();
    d().clock.setDateTime(1, 1, 1, 9, 0, 0);
    d().clock.setTimeScale(1.0f);
    d().climate = game::ClimateState{};
    d().climate.clock = &d().clock;
    {
        std::vector<uint8_t> bytes;
        if (r.platform().readAsset("stars.bin", bytes)) engine::stars::loadStars(bytes.data(), bytes.size(), d().stars);
    }
    battle::buildHUD(r, gs_);

    Renderer* rptr = &r;
    d().subs.push_back(EventBus::on<TapEvent>([rptr](const TapEvent& e) { battle::handleTap(e.x, e.y, *rptr); }));
    d().subs.push_back(EventBus::on<HoverEvent>([rptr](const HoverEvent& e) {
        battle::handleHover(e.x, e.y, *rptr);
    }));
}

void BattleScene::exit(Renderer& r) {
    r.setPickRoot(nullptr);
    r.camera().setTerrainHeightFn({});
    r.camera().clearTargetBounds();
    for (auto id : d().subs) EventBus::unsubscribe(id);
    if (d().registry && !d().registry->empty()) gs_.captureFrom(*d().registry);
    if (d().actions) d().actions->clear();
    if (d().registry) d().registry->clear();
    d() = battle::Data{};
}

void BattleScene::update(float dt, Renderer& r) {
    if (!d().root) return;
    const float gameDt = dt * d().speed;

    d().clock.tick(dt);
    d().climate.update(dt);
    d().climate.applyToRenderer(r);
    d().currentSun = engine::sky::computeSunState(d().climate.dayPhase(), d().palettes.sky, d().climate.current,
                                                  d().palettes.weather, r.skyConfig(), d().climate.stars.skyAxis);
    r.setClearColor(d().currentSun.skyColor);
    r.setFrameLight(d().currentSun.lightDir, d().currentSun.ambient);

    d().root->update(gameDt);
    if (d().actions) d().actions->update(gameDt);
    if (d().registry) d().registry->updateAll(gameDt);

    if (d().projRoot && !d().projRoot->children().empty()) {
        const math::Mat4 m = d().projRoot->children().front()->world();
        fx::emitTrail(d().trail, math::Vec3(m.m[12], m.m[13], m.m[14]), gameDt);
    }
    d().trail.update(gameDt);
    d().particles.update(gameDt);

    const engine::wx::Weather wx = d().climate.current;
    if (wx == engine::wx::Weather::Rain || wx == engine::wx::Weather::Storm) {
        d().rainAccum += (wx == engine::wx::Weather::Storm ? 120.0f : 60.0f) * dt;
        const int count = static_cast<int>(d().rainAccum);
        d().rainAccum -= static_cast<float>(count);
        const math::Vec3& rc = d().palettes.weather.rainColor;
        for (int i = 0; i < count; ++i)
            d().rain.emit(engine::wx::rainSpawnPos(r.camera().eye()), math::Vec3(0.0f, -5.0f, 0.0f),
                          math::Vec4(rc.x, rc.y, rc.z, 0.5f), 0.04f, 1.0f);
    } else {
        d().rain.clear();
        d().rainAccum = 0.0f;
    }
    d().rain.update(dt);

    battle::updateTurns(dt, r, gs_);

    if (d().turnUnit && d().actions && !d().actions->isIdle() && !d().framing) {
        const math::Mat4 m = d().turnUnit->node()->world();
        const math::Vec3 cur = r.camera().target();
        const math::Vec3 to(m.m[12], m.m[13], m.m[14]);
        r.camera().setTarget(cur + (to - cur) * std::min(1.0f, gameDt * 6.0f));
    }

    if (d().turnUnit && !d().turnUnit->isDead()) {
        const math::Vec3 p = d().grid->cellToWorld(static_cast<int>(d().turnUnit->cell().x),
                                                   static_cast<int>(d().turnUnit->cell().y));
        d().ring->setLocal(math::Mat4::translation(math::Vec3(p.x, p.y + 0.09f, p.z)) *
                           math::Mat4::scale(math::Vec3(0.55f, 1.0f, 0.55f)));
    } else {
        d().ring->setLocal(math::Mat4::scale(math::Vec3(0.0f, 1.0f, 0.0f)));
    }
    if (d().hovered && !d().hovered->isDead()) {
        const math::Vec3 p = d().grid->cellToWorld(static_cast<int>(d().hovered->cell().x),
                                                   static_cast<int>(d().hovered->cell().y));
        d().hoverRing->setLocal(math::Mat4::translation(math::Vec3(p.x, p.y + 0.085f, p.z)) *
                                math::Mat4::scale(math::Vec3(0.8f, 1.0f, 0.8f)));
    } else {
        d().hoverRing->setLocal(math::Mat4::scale(math::Vec3(0.0f, 1.0f, 0.0f)));
    }
    d().sprites.update(r.camera());
    if (d().sprites.enabled()) d().spritesOk = d().spritesOk && d().sprites.inSync();
    battle::updateHUD(dt, r);
}

void BattleScene::render(Renderer& r) {
    if (!d().root) {
        r.renderUI(d().ui.get());
        return;
    }
    r.renderSceneGraph(d().root.get());
    r.renderSceneGraph(d().overlay.get());
    r.renderSceneGraph(d().hoverRing.get());
    r.renderSceneGraph(d().ring.get());
    r.renderParticles(d().trail);
    r.renderParticles(d().particles);
    r.renderParticles(d().rain);
    r.renderStars(d().stars, d().currentSun.starIntensity, d().climate.dayPhase());
    if (d().climate.current == engine::wx::Weather::Storm)
        r.renderLightningFlash(d().palettes.weather.lightningColor, d().climate.lightning.flashIntensity);
    r.renderUI(d().ui.get());
}

void BattleScene::onResize(int w, int h, Renderer& r) {
    if (w < 1 || h < 1) return;
    r.camera().setPerspective(battle::kCameraFovYRadians, static_cast<float>(w) / static_cast<float>(h),
                              battle::kCameraNearPlane, battle::kCameraFarPlane);
    if (d().ui) battle::buildHUD(r, gs_);
}

SceneNode* BattleScene::shadowRoot() const {
    return d().root.get();
}
