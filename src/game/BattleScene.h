#ifndef NOT_A_GAME_BATTLESCENE_H
#define NOT_A_GAME_BATTLESCENE_H

#include "Scene.h"

class Renderer;
namespace game { class GameState; }

class BattleScene : public Scene {
public:
    BattleScene(game::GameState& gs, int levelId = 1, bool autoBattle = false)
        : gs_(gs), levelId_(levelId), autoBattle_(autoBattle) {}
    void enter(Renderer& r) override;
    void exit(Renderer& r) override;
    void update(float dt, Renderer& r) override;
    void render(Renderer& r) override;
    void onResize(int w, int h, Renderer& r) override;
    SceneNode* shadowRoot() const override;

private:
    game::GameState& gs_;
    int levelId_;
    bool autoBattle_;
};

#endif //NOT_A_GAME_BATTLESCENE_H
