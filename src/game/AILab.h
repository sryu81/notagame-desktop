#ifndef NOT_A_GAME_AI_LAB_H
#define NOT_A_GAME_AI_LAB_H

#include <memory>
#include <string>
#include <vector>

#include "AI.h"
#include "Action.h"
#include "BattleGrid.h"
#include "DebugPanel.h"
#include "EntityBuilder.h"
#include "EventBus.h"
#include "LabAssets.h"
#include "LabChecklist.h"
#include "Scene.h"
#include "SceneNode.h"
#include "UIRoot.h"

namespace game { class GameState; class Unit; }

class AILab : public Scene {
public:
    explicit AILab(game::GameState& gs);
    ~AILab() override;
    void enter(Renderer& r) override;
    void exit(Renderer& r) override;
    void update(float dt, Renderer& r) override;
    void render(Renderer& r) override;
    void onResize(int w, int h, Renderer& r) override;
    SceneNode* shadowRoot() const override { return root_.get(); }

private:
    void runBatch();
    void startLive(uint32_t seed);
    void stepLive();
    void finishChecks();
    void buildUI(Renderer& r);
    void resetCamera(Renderer& r);
    void restart();
    void updateRing();

    game::GameState& gs_;
    std::unique_ptr<game::LabAssets> assets_;
    std::unique_ptr<SceneNode> root_;
    std::unique_ptr<SceneNode> ring_;
    std::unique_ptr<grid::BattleGrid> grid_;
    std::unique_ptr<game::EntityBuilder> builder_;
    std::unique_ptr<action::Manager> actions_;
    std::vector<game::Unit*> units_;
    std::vector<ai::SimUnit> roster_;
    std::vector<ai::SimResult> batch_;
    ai::SimConfig liveCfg_;
    std::unique_ptr<ai::Rng> liveRng_;
    std::string liveLog_;
    std::string expectedLog_;
    int   liveRound_ = 1;
    int   liveUnit_ = -1;
    int   livePhase_ = 0;
    game::Unit* liveTarget_ = nullptr;
    int   liveDamage_ = 0;
    bool  liveDone_ = false;
    float speed_ = 4.0f;
    bool  checked_ = false;

    std::unique_ptr<ui::UIRoot> ui_;
    std::unique_ptr<game::DebugPanel> panel_;
    std::unique_ptr<game::LabChecklist> checks_;
    std::vector<ui::UIText*> infoText_;
    std::vector<std::string> info_;
    std::vector<EventBus::SubscriptionId> subs_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

#endif
