#ifndef NOT_A_GAME_RESULT_SCENE_H
#define NOT_A_GAME_RESULT_SCENE_H

#include <memory>
#include <string>
#include <vector>

#include "BattleFlow.h"
#include "DebugPanel.h"
#include "EventBus.h"
#include "Scene.h"
#include "UIRoot.h"

namespace game { class GameState; }

class ResultScene : public Scene {
public:
    ResultScene(game::GameState& gs, flow::Outcome outcome, std::string summary);
    void enter(Renderer& r) override;
    void exit(Renderer& r) override;
    void update(float dt, Renderer& r) override;
    void render(Renderer& r) override;
    void onResize(int w, int h, Renderer& r) override;

    flow::Outcome outcome() const { return outcome_; }
    std::unique_ptr<Scene> continueScene() const;

private:
    void buildUI(int w, int h, Renderer& r);

    game::GameState& gs_;
    flow::Outcome outcome_;
    std::string summary_;
    std::unique_ptr<ui::UIRoot> ui_;
    std::unique_ptr<game::DebugPanel> menu_;
    std::vector<EventBus::SubscriptionId> subs_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

#endif
