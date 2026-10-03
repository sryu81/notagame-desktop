#ifndef NOT_A_GAME_FX_LAB_H
#define NOT_A_GAME_FX_LAB_H

#include <memory>
#include <vector>

#include "Action.h"
#include "BattleFx.h"
#include "BattleGrid.h"
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

namespace game { class GameState; class Unit; }

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
    void startGameplayFx(Renderer& r);
    void updateGameplayFx(float dt, Renderer& r);

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

    std::unique_ptr<SceneNode> fxRoot_;
    SceneNode* projRoot_ = nullptr;
    std::unique_ptr<grid::BattleGrid> fxGrid_;
    std::unique_ptr<game::Unit> archer_;
    std::unique_ptr<game::Unit> dummy_;
    std::unique_ptr<action::Manager> fxActions_;
    particle::Emitter trail_{256};
    fx::FloatingTexts numbers_;
    fx::StatusIcons icons_;
    std::vector<math::Vec3> projPath_;
    int   frame_ = 0;
    int   hitFrame_ = -1;
    int   numberFrame_ = -1;
    float numberY0_ = 0.0f;
    float numberA0_ = 0.0f;
    bool  numberRose_ = false;
    bool  numberFaded_ = false;
    std::string numberText_;
    bool  sawTrail_ = false;
    bool  trailOff_ = false;
    std::string iconSeen_;
    bool  iconAbove_ = false;
};

#endif
