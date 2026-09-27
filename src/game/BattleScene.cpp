#include "BattleScene.h"

#include <algorithm>

#include "BattleData.h"
#include "EntityBuilder.h"
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
    battle::createContent(r, gs_);
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
    battle::buildHUD(r, gs_);
    d().lastTurnUnit = d().turnManager ? d().turnManager->currentUnit() : nullptr;

    Renderer* rptr = &r;
    d().subs.push_back(EventBus::on<PointerDownEvent>([rptr](const PointerDownEvent& e) {
        battle::handlePointerDown(e.x, e.y, *rptr);
    }));
}

void BattleScene::exit(Renderer& r) {
    r.setPickRoot(nullptr);
    for (auto id : d().subs) EventBus::unsubscribe(id);
    if (d().registry && !d().registry->empty()) gs_.captureFrom(*d().registry);
    if (d().registry) d().registry->clear();
    d() = battle::Data{};
}

void BattleScene::update(float dt, Renderer& r) {
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

    {
        const bool isIdle = !d().actionManager || d().actionManager->isIdle();
        if (d().wasActionIdle && !isIdle) {
            d().preMoveCamTarget = r.camera().target();
            d().camReturnActive  = false;
            d().camReturnTimer   = 0.0f;
        }
        if (!isIdle && d().movingUnit && d().movingUnit->node()) {
            const math::Mat4 w = d().movingUnit->node()->world();
            r.camera().setTarget(math::Vec3(w.m[12], w.m[13], w.m[14]));
        }
        if (!d().wasActionIdle && isIdle) {
            d().preMoveCamTarget.y = 0.5f;
            d().camReturnActive = true;
            d().camReturnTimer  = 0.0f;
            d().movingUnit      = nullptr;
        }
        if (d().camReturnActive) {
            d().camReturnTimer += userDt;
            const float t = std::min(d().camReturnTimer / 0.5f, 1.0f);
            const math::Vec3 cur = r.camera().target();
            r.camera().setTarget(cur + (d().preMoveCamTarget - cur) * t);
            if (t >= 1.0f) d().camReturnActive = false;
        }
        d().wasActionIdle = isIdle;
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

    battle::tickTurnStart();

    d().burstTimer += dt;
    if (d().burstTimer >= 5.0f) {
        d().burstTimer = 0.0f;
        d().particleEmitter.burst(
            math::Vec3(0, 0.5f, 0), 8, 2.0f,
            math::Vec4(1, 0.6f, 0.1f, 1), 0.3f, 1.5f);
    }
    d().particleEmitter.update(userDt);

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

    for (size_t i = 0; i < d().registry->size(); ++i) {
        SceneNode* sn = d().registry->shadowAt(i);
        auto* c = d().registry->unitAt(i);
        if (sn && c && c->node())
            game::EntityBuilder::syncShadowMirror(c->node(), sn);
    }

    battle::refreshSelectionOverlay();
    battle::updateHUD(dt);
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
    r.camera().setPerspective(battle::kCameraFovYRadians,
        static_cast<float>(w) / static_cast<float>(h),
        battle::kCameraNearPlane, battle::kCameraFarPlane);
    if (d().ui) battle::buildHUD(r, gs_);
}

SceneNode* BattleScene::shadowRoot() const {
    return d().shadowRoot.get();
}
