#ifndef NOT_A_GAME_FX_LAB_H
#define NOT_A_GAME_FX_LAB_H

#include <memory>
#include <vector>

#include "Climate.h"
#include "DebugPanel.h"
#include "EventBus.h"
#include "LabAssets.h"
#include "LabChecklist.h"
#include "ParticleSystem.h"
#include "Renderer.h"
#include "Scene.h"
#include "SceneNode.h"
#include "Stars.h"
#include "UIRoot.h"

namespace game { class GameState; }

class FxLab : public Scene {
public:
    explicit FxLab(game::GameState& gs) : gs_(gs) {}
    void enter(Renderer& r) override;
    void exit(Renderer& r) override;
    void update(float dt, Renderer& r) override;
    void render(Renderer& r) override;
    void onResize(int w, int h, Renderer& r) override;

private:
    void buildUI(Renderer& r);
    void restartAutoDrive();
    void autoDrive(float dt, Renderer& r);
    void emitRain(float dt, Renderer& r);
    void checkParticles(Renderer& r);
    void checkInstant(Renderer& r);
    void setWeather(engine::wx::Weather w);

    game::GameState& gs_;
    std::unique_ptr<game::LabAssets> assets_;
    std::unique_ptr<SceneNode> root_;
    std::unique_ptr<ui::UIRoot> ui_;
    std::unique_ptr<game::DebugPanel> panel_;
    std::unique_ptr<game::LabChecklist> checks_;
    std::vector<EventBus::SubscriptionId> subs_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);

    particle::Emitter burst_{512};
    particle::Emitter rain_{1024};
    float rainAccum_ = 0.0f;
    float rainRate_  = 60.0f;

    engine::stars::StarField stars_;
    engine::clock::Clock clock_;
    game::ClimateState climate_;
    game::ClimatePalettes palettes_;
    engine::sky::SunState sun_{};

    engine::sky::SkyConfig savedSky_;
    Renderer::StarRenderConfig savedStars_;

    bool  autoOn_ = true;
    float autoT_  = 0.0f;
    int   step_   = 0;
    float gravityVy0_ = 0.0f;
    float stormStart_ = -1.0f;
    bool  sawFlash_   = false;
    bool  sawDecay_   = false;
    int   wxIdx_      = 0;
    int   gravIdx_    = 2;
    bool  timeLapse_  = false;
    float burstTimer_ = 0.0f;
    bool  starsAtNight_ = false;
    bool  starsOffNoon_ = false;
};

#endif
