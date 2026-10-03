#ifndef NOT_A_GAME_LEVEL_SELECT_SCENE_H
#define NOT_A_GAME_LEVEL_SELECT_SCENE_H

#include <memory>
#include <vector>

#include "DebugPanel.h"
#include "EventBus.h"
#include "Scene.h"
#include "UIRoot.h"

namespace game { class GameState; }

class LevelSelectScene : public Scene {
public:
    explicit LevelSelectScene(game::GameState& gs) : gs_(gs) {}
    void enter(Renderer& r) override;
    void exit(Renderer& r) override;
    void update(float, Renderer&) override {}
    void render(Renderer& r) override;
    void onResize(int w, int h, Renderer& r) override;

private:
    void buildUI(int w, int h, Renderer& r);

    game::GameState& gs_;
    int level_ = 1;
    bool autoBattle_ = false;
    std::unique_ptr<ui::UIRoot> ui_;
    std::unique_ptr<game::DebugPanel> menu_;
    std::vector<EventBus::SubscriptionId> subs_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

#endif
