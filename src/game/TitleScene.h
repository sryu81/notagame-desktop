#ifndef NOT_A_GAME_TITLESCENE_H
#define NOT_A_GAME_TITLESCENE_H

#include <memory>
#include <vector>

#include "EventBus.h"
#include "Scene.h"
#include "UIRoot.h"

class Renderer;
namespace game { class GameState; }

class TitleScene : public Scene {
public:
    explicit TitleScene(game::GameState& gs) : gs_(gs) {}
    void enter(Renderer& r) override;
    void exit(Renderer& r) override;
    void update(float dt, Renderer& r) override {}
    void render(Renderer& r) override;
    void onResize(int w, int h, Renderer& r) override;

private:
    void buildUI(int w, int h, Renderer* r = nullptr);

    game::GameState& gs_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    std::unique_ptr<ui::UIRoot> ui_;
    std::vector<EventBus::SubscriptionId> subs_;
};

#endif //NOT_A_GAME_TITLESCENE_H
