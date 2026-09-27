#ifndef NOT_A_GAME_HUB_SCENE_H
#define NOT_A_GAME_HUB_SCENE_H

#include <memory>
#include <string>
#include <vector>

#include "DebugPanel.h"
#include "EventBus.h"
#include "LabChecklist.h"
#include "Scene.h"
#include "SceneManager.h"
#include "UIRoot.h"

class Renderer;
namespace game { class GameState; }

class HubScene : public Scene {
public:
    struct Entry {
        std::string label;
        SceneFactory factory;
    };

    explicit HubScene(game::GameState& gs) : gs_(gs) {}
    void enter(Renderer& r) override;
    void exit(Renderer& r) override;
    void update(float dt, Renderer& r) override {}
    void render(Renderer& r) override;
    void onResize(int w, int h, Renderer& r) override;

    static std::vector<Entry> entries(game::GameState& gs);

private:
    void buildUI(int w, int h, Renderer& r);

    game::GameState& gs_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    std::unique_ptr<ui::UIRoot> ui_;
    std::unique_ptr<game::DebugPanel> menu_;
    std::unique_ptr<game::LabChecklist> checks_;
    std::vector<EventBus::SubscriptionId> subs_;
};

#endif
