#include "BattleData.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include "BattleScene.h"
#include "GameState.h"
#include "HubScene.h"

namespace battle {

namespace {

constexpr float kMargin = 24.0f;

const float kSpeeds[]        = {0.0f, 0.5f, 1.0f, 2.0f, 4.0f};
const char* kSpeedLabels[]   = {"PAUSE", "0.5X", "1X", "2X", "4X"};
const engine::wx::Weather kWxList[] = {
    engine::wx::Weather::Clear, engine::wx::Weather::Rain, engine::wx::Weather::Storm};
const char* kWxLabels[]      = {"CLEAR", "RAIN", "STORM"};
const char* kOrbitLabels[]   = {"ISO", "SIDE", "TOP"};
const float kOrbitPitch[]    = {35.0f, 15.0f, 70.0f};
const float kOrbitYaw[]      = {0.0f, 0.0f, 0.0f};
const float kFovDeg[]        = {30.0f, 45.0f, 60.0f};
const char* kPanLabels[]     = {"LO", "MED", "HI"};
const float kPanVal[]        = {0.5f, 1.0f, 2.0f};
const float kVolVal[]        = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
const float kGravVal[]       = {0.0f, -1.0f, -2.0f, -4.0f, -8.0f};
const float kUpVelVal[]      = {0.5f, 1.0f, 1.5f, 2.0f, 3.0f};
const float kShrinkVal[]     = {0.2f, 0.5f, 0.8f, 1.5f};
const float kSkyNightVal[]   = {2.0f, 3.0f, 4.0f, 6.0f, 8.0f};
const float kSkyDawnVal[]    = {0.15f, 0.25f, 0.30f, 0.40f, 0.50f};
const float kSkyNoonVal[]    = {1.0f, 1.5f, 2.0f, 3.0f, 4.0f};
const float kSkyAmbientVal[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
const float kWxFlashVal[]    = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f};
const float kWxDecayVal[]    = {2.0f, 4.0f, 6.0f, 10.0f, 16.0f};
const float kWxIntervalVal[] = {1.0f, 2.0f, 3.0f, 5.0f, 8.0f};
const float kWxJitterVal[]   = {0.0f, 2.0f, 7.0f, 12.0f, 20.0f};
const char* kLandLabels[]    = {"LEVEL", "FLAT", "HILL", "VALLEY", "RIDGE", "CLIFF"};
const char* kPanelNames[]    = {"TIME", "CAMERA", "AUDIO", "PARTICLES", "SKY", "WEATHER", "SCENE"};

template <size_t N>
int cycle(int& idx, const float (&)[N]) { idx = (idx + 1) % static_cast<int>(N); return idx; }

std::string fmt(float v) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%g", v);
    return buf;
}

std::string pct(float v) {
    return std::to_string(static_cast<int>(v * 100.0f + 0.5f)) + "%";
}

void applyPanelVisibility() {
    for (int i = 0; i < kPanelCount; ++i) {
        if (d().hudPanels[i]) d().hudPanels[i]->setVisible(d().hudOpen == i);
    }
}

void resetLightningTimer() {
    d().climate.lightning.nextDelay = -1.0f;
    d().climate.lightning.timer     = 0.0f;
}

void addTimeRows(game::DebugPanel& p) {
    p.addRow("SPEED", []() { return std::string(kSpeedLabels[d().speedPreset]); }, []() {
        const float s = kSpeeds[cycle(d().speedPreset, kSpeeds)];
        if (s <= 0.0f) d().clock.pause();
        else           d().clock.resume();
        d().clock.setTimeScale(s > 0.0f ? s : 1.0f);
        refreshTimeText();
    });
    p.addAction("+1 HOUR", []() { d().clock.addSeconds(3600.0); refreshTimeText(); });
    p.addAction("DAWN", []() { d().clock.setTimeOfDay(6, 0, 0); refreshTimeText(); });
    p.addRow("WEATHER", []() { return std::string(kWxLabels[d().wxPreset]); }, []() {
        d().wxPreset = (d().wxPreset + 1) % 3;
        d().climate.setWeather(kWxList[d().wxPreset], static_cast<int>(d().clock.totalSeconds()));
    });
}

void addCameraRows(game::DebugPanel& p, Renderer* r) {
    p.addRow("ORBIT", []() { return std::string(kOrbitLabels[d().camOrbitPreset]); }, [r]() {
        const int i = cycle(d().camOrbitPreset, kOrbitPitch);
        r->setOrbitSnap(kOrbitPitch[i], kOrbitYaw[i]);
    });
    p.addAction("ZOOM IN", [r]() { r->zoomBy(1.2f); });
    p.addAction("ZOOM OUT", [r]() { r->zoomBy(1.0f / 1.2f); });
    p.addAction("RESET VIEW", [r]() { r->resetView(); });
    p.addRow("FOV", []() { return fmt(kFovDeg[d().camFovPreset]); }, [r]() {
        r->setFovYRadians(math::radians(kFovDeg[cycle(d().camFovPreset, kFovDeg)]));
    });
    p.addRow("PAN", []() { return std::string(kPanLabels[d().camSensPreset]); }, [r]() {
        r->setPanSensitivity(kPanVal[cycle(d().camSensPreset, kPanVal)]);
    });
}

void addAudioRows(game::DebugPanel& p, Renderer* r) {
    p.addRow("MASTER", []() { return pct(kVolVal[d().masterPreset]); }, [r]() {
        r->setMasterVolume(kVolVal[cycle(d().masterPreset, kVolVal)]);
    });
    p.addRow("MUSIC", []() { return pct(kVolVal[d().musicPreset]); }, [r]() {
        r->setMusicVolume(kVolVal[cycle(d().musicPreset, kVolVal)]);
    });
    p.addRow("SFX", []() { return pct(kVolVal[d().sfxPreset]); }, [r]() {
        r->setSfxVolume(kVolVal[cycle(d().sfxPreset, kVolVal)]);
    });
    p.addAction("PLAY SFX", [r]() { r->playClickSfx(); });
}

void addParticleRows(game::DebugPanel& p) {
    p.addRow("GRAVITY", []() { return fmt(kGravVal[d().partGravityPreset]); }, []() {
        d().particleEmitter.setGravity(
            math::Vec3(0.0f, kGravVal[cycle(d().partGravityPreset, kGravVal)], 0.0f));
    });
    p.addRow("UP VEL", []() { return fmt(kUpVelVal[d().partUpVelPreset]); }, []() {
        d().particleEmitter.setUpVelocityMult(kUpVelVal[cycle(d().partUpVelPreset, kUpVelVal)]);
    });
    p.addRow("SHRINK", []() { return fmt(kShrinkVal[d().partShrinkPreset]); }, []() {
        d().particleEmitter.setSizeShrinkFactor(kShrinkVal[cycle(d().partShrinkPreset, kShrinkVal)]);
    });
    p.addAction("BURST", []() {
        d().particleEmitter.burst(math::Vec3(0, 0.5f, 0), 16, 2.5f,
                                  math::Vec4(0.4f, 0.9f, 1.0f, 1.0f), 0.4f, 1.5f);
        d().burstTimer = 0.0f;
    });
}

void addSkyRows(game::DebugPanel& p, Renderer* r) {
    p.addRow("NIGHT BLEND", []() { return fmt(kSkyNightVal[d().skyNightPreset]); }, [r]() {
        r->setSkyNightBlendScale(kSkyNightVal[cycle(d().skyNightPreset, kSkyNightVal)]);
    });
    p.addRow("DAWN WIDTH", []() { return fmt(kSkyDawnVal[d().skyDawnPreset]); }, [r]() {
        r->setSkyDawnWidth(kSkyDawnVal[cycle(d().skyDawnPreset, kSkyDawnVal)]);
    });
    p.addRow("NOON", []() { return fmt(kSkyNoonVal[d().skyNoonPreset]); }, [r]() {
        r->setSkyNoonScale(kSkyNoonVal[cycle(d().skyNoonPreset, kSkyNoonVal)]);
    });
    p.addRow("AMBIENT", []() { return fmt(kSkyAmbientVal[d().skyAmbientPreset]); }, [r]() {
        r->setSkyAmbientBaseline(kSkyAmbientVal[cycle(d().skyAmbientPreset, kSkyAmbientVal)]);
    });
}

void addWeatherRows(game::DebugPanel& p) {
    p.addRow("FLASH", []() { return fmt(kWxFlashVal[d().wxFlashPreset]); }, []() {
        d().climate.lightning.peakIntensity = kWxFlashVal[cycle(d().wxFlashPreset, kWxFlashVal)];
    });
    p.addRow("DECAY", []() { return fmt(kWxDecayVal[d().wxDecayPreset]); }, []() {
        d().climate.lightning.flashDecay = kWxDecayVal[cycle(d().wxDecayPreset, kWxDecayVal)];
    });
    p.addRow("INTERVAL", []() { return fmt(kWxIntervalVal[d().wxIntervalPreset]); }, []() {
        d().climate.lightning.baseDelay = kWxIntervalVal[cycle(d().wxIntervalPreset, kWxIntervalVal)];
        resetLightningTimer();
    });
    p.addRow("JITTER", []() { return fmt(kWxJitterVal[d().wxJitterPreset]); }, []() {
        d().climate.lightning.jitterRange = kWxJitterVal[cycle(d().wxJitterPreset, kWxJitterVal)];
        resetLightningTimer();
    });
}

void addSceneRows(game::DebugPanel& p, Renderer* r, game::GameState* gs) {
    p.addRow("LAND", []() { return std::string(kLandLabels[d().landscapeIdx]); }, []() {
        setLandscape((d().landscapeIdx + 1) % kLandscapeCount);
    });
    p.addAction("RESET SCENE", [r, gs]() {
        pendingLandscapeIdx() = d().landscapeIdx;
        r->sceneManager().replace(std::make_unique<BattleScene>(*gs));
    });
    p.addAction("SAVE", []() { saveGame(); });
}

} // namespace

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

void buildHUD(Renderer& r, game::GameState& gs) {
    if (!d().ui) {
        d().ui = std::make_unique<ui::UIRoot>();
        d().ui->init();
    }
    const int w = r.width()  > 0 ? r.width()  : 1080;
    const int h = r.height() > 0 ? r.height() : 1920;
    d().ui->setScreenSize(w, h);
    const float fw = static_cast<float>(w);
    const float fh = static_cast<float>(h);

    const float rowH  = std::clamp(fh / 18.0f, 36.0f, 72.0f);
    const int   scale = rowH >= 56.0f ? 3 : 2;
    const float gap   = rowH * 0.15f;
    const float menuW = rowH * 4.2f;
    const float panelW = rowH * 5.5f;

    auto root = std::make_unique<ui::UIPanel>(math::Vec2(0, 0), math::Vec2(fw, fh),
                                              math::Vec4(0, 0, 0, 0));

    auto banner = std::make_unique<ui::UIText>(math::Vec2(0, 0), "WAITING",
                                               math::Vec4(1, 1, 1, 1), scale + 2);
    banner->setPosition(math::Vec2((fw - banner->size().x) * 0.5f, kMargin));
    const float bannerBottom = kMargin + banner->size().y;
    d().turnBanner = banner.get();
    root->addChild(std::move(banner));

    auto timeLbl = std::make_unique<ui::UIText>(math::Vec2(0, 0), "Y10000M12D30 23:59 PAUSED",
                                                math::Vec4(0.95f, 0.95f, 0.70f, 1.0f), scale + 1);
    timeLbl->setPosition(math::Vec2((fw - timeLbl->size().x) * 0.5f, bannerBottom + 6.0f));
    const float timeBottom = bannerBottom + 6.0f + timeLbl->size().y;
    d().timeText = timeLbl.get();
    root->addChild(std::move(timeLbl));
    refreshTimeText();

    auto fps = std::make_unique<ui::UIText>(math::Vec2(0, 0), "FPS:000",
                                            math::Vec4(0.5f, 1.0f, 0.5f, 1.0f), scale);
    fps->setPosition(math::Vec2(fw - kMargin - fps->size().x, kMargin));
    d().fpsText = fps.get();
    const float fpsBottom = kMargin + fps->size().y;
    root->addChild(std::move(fps));

    auto dbg = std::make_unique<ui::UIText>(math::Vec2(0, 0), "anim:b=00 ev=-0.000",
                                            math::Vec4(0.5f, 0.9f, 1.0f, 1.0f), 2);
    dbg->setPosition(math::Vec2(fw - kMargin - dbg->size().x, fpsBottom + 4.0f));
    d().debugText = dbg.get();
    root->addChild(std::move(dbg));

    Renderer* rptr = &r;
    game::GameState* gsp = &gs;

    const float panelTop = timeBottom + rowH * 0.5f;
    for (int i = 0; i < kPanelCount; ++i) {
        d().hudPanels[i] = std::make_unique<game::DebugPanel>(
            math::Vec2(kMargin, panelTop), math::Vec2(panelW, rowH), gap, scale);
    }
    addTimeRows(*d().hudPanels[kPanelTime]);
    addCameraRows(*d().hudPanels[kPanelCamera], rptr);
    addAudioRows(*d().hudPanels[kPanelAudio], rptr);
    addParticleRows(*d().hudPanels[kPanelParticles]);
    addSkyRows(*d().hudPanels[kPanelSky], rptr);
    addWeatherRows(*d().hudPanels[kPanelWeather]);
    addSceneRows(*d().hudPanels[kPanelScene], rptr, gsp);

    d().hudMenu = std::make_unique<game::DebugPanel>(math::Vec2(0, 0), math::Vec2(menuW, rowH), gap, scale);
    for (int i = 0; i < kPanelCount; ++i) {
        d().hudMenu->addRow(kPanelNames[i],
            [i]() { return std::string(d().hudOpen == i ? "<" : ""); },
            [i]() {
                d().hudOpen = d().hudOpen == i ? -1 : i;
                applyPanelVisibility();
            });
    }
    d().hudMenu->addAction("BACK", [rptr, gsp]() {
        rptr->sceneManager().replace(std::make_unique<HubScene>(*gsp));
    });
    d().hudMenu->addAction("EXIT", [rptr]() { rptr->finish(); });

    const math::Vec2 menuSize = d().hudMenu->panelSize();
    d().hudMenu->setOrigin(math::Vec2(fw - kMargin - menuSize.x, fh - kMargin - menuSize.y));
    root->addChild(d().hudMenu->build());

    for (int i = 0; i < kPanelCount; ++i) root->addChild(d().hudPanels[i]->build());
    applyPanelVisibility();

    d().ui->setRoot(std::move(root));
}

void updateHUD(float dt) {
    if (d().turnManager && d().turnBanner) {
        char label[40];
        const float remain = d().turnManager->currentDuration() - d().turnManager->elapsed();
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
    if (d().fpsAccum < 0.5f || !d().fpsText) return;
    const int fps = static_cast<int>(d().fpsFrames / d().fpsAccum + 0.5f);
    char buf[32];
    snprintf(buf, sizeof(buf), "FPS:%d", fps);
    d().fpsText->setText(buf);
    refreshTimeText();
    if (d().debugText) {
        char dbuf[48];
        snprintf(dbuf, sizeof(dbuf), "anim:b=%d ev=%.3f", d().boneCount, action::lastEventTime());
        d().debugText->setText(dbuf);
    }
    d().fpsAccum = 0.0f;
    d().fpsFrames = 0;
}

} // namespace battle
