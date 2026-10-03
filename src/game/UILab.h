#ifndef NOT_A_GAME_UI_LAB_H
#define NOT_A_GAME_UI_LAB_H

#include <memory>
#include <string>
#include <vector>

#include "Action.h"
#include "BattleGrid.h"
#include "DebugPanel.h"
#include "EntityBuilder.h"
#include "EventBus.h"
#include "GameUI.h"
#include "LabAssets.h"
#include "LabChecklist.h"
#include "Scene.h"
#include "SceneNode.h"
#include "Turn.h"
#include "UIRoot.h"

namespace game { class GameState; class Unit; }

class UILab : public Scene {
public:
    explicit UILab(game::GameState& gs);
    ~UILab() override;
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
    void refreshWidgets();
    void onAction(gui::Action a);
    void autoDrive(float dt);

    game::GameState& gs_;
    std::unique_ptr<game::LabAssets> assets_;
    std::unique_ptr<SceneNode> root_;
    SceneNode* unitsRoot_ = nullptr;
    std::unique_ptr<grid::BattleGrid> grid_;
    std::unique_ptr<game::EntityBuilder> builder_;
    std::unique_ptr<action::Manager> actions_;
    std::unique_ptr<turn::Manager> turns_;
    std::vector<game::Unit*> units_;
    std::vector<std::string> names_;
    gui::TurnState state_;
    gui::ActionMenu menu_;
    gui::UnitCard card_;
    gui::TurnOrderBar bar_;
    const game::Unit* selected_ = nullptr;

    std::unique_ptr<ui::UIRoot> ui_;
    std::unique_ptr<game::DebugPanel> panel_;
    std::unique_ptr<game::LabChecklist> checks_;
    std::vector<EventBus::SubscriptionId> subs_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);

    bool  autoOn_ = true;
    int   step_ = 0;
    float stepT_ = 0.0f;
    int   orderSteps_ = 0;
    bool  orderOk_ = true;
    bool  legalBefore_ = false;
    int   hp0_ = 0;
};

#endif
