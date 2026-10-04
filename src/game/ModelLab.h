#ifndef NOT_A_GAME_MODEL_LAB_H
#define NOT_A_GAME_MODEL_LAB_H

#include <memory>
#include <string>
#include <vector>

#include "BattleGrid.h"
#include "DebugPanel.h"
#include "EntityBuilder.h"
#include "EventBus.h"
#include "LabAssets.h"
#include "LabChecklist.h"
#include "Scene.h"
#include "SceneNode.h"
#include "UIRoot.h"
#include "UnitCatalog.h"
#include "UnitModels.h"

namespace game { class GameState; class Unit; class Prop; }

class ModelLab : public Scene {
public:
    explicit ModelLab(game::GameState& gs);
    ~ModelLab() override;
    void enter(Renderer& r) override;
    void exit(Renderer& r) override;
    void update(float dt, Renderer& r) override;
    void render(Renderer& r) override;
    void onResize(int w, int h, Renderer& r) override;
    SceneNode* shadowRoot() const override { return root_.get(); }

private:
    void spawn();
    void buildUI(Renderer& r);
    void resetCamera(Renderer& r);
    void restart();
    void playClip(int i);
    void applyTurn();
    void staticChecks();
    void autoDrive(float dt);

    game::GameState& gs_;
    std::unique_ptr<game::LabAssets> assets_;
    game::UnitModels models_;
    game::UnitCatalog catalog_;
    std::unique_ptr<SceneNode> root_;
    std::unique_ptr<grid::BattleGrid> grid_;
    std::unique_ptr<game::EntityBuilder> builder_;
    std::vector<game::Unit*> units_;
    std::vector<std::string> ids_;
    std::vector<game::Prop*> props_;

    std::unique_ptr<ui::UIRoot> ui_;
    std::unique_ptr<game::DebugPanel> panel_;
    std::unique_ptr<game::LabChecklist> checks_;
    std::vector<EventBus::SubscriptionId> subs_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);

    int   clip_ = 0;
    bool  turnOn_ = false;
    float turnDeg_ = 0.0f;
    bool  autoOn_ = true;
    int   step_ = 0;
    float stepT_ = 0.0f;
    bool  playOk_ = true;
    bool  checked_ = false;
};

#endif
