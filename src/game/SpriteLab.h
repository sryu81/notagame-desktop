#ifndef NOT_A_GAME_SPRITE_LAB_H
#define NOT_A_GAME_SPRITE_LAB_H

#include <memory>
#include <string>
#include <vector>

#include "BattleGrid.h"
#include "DebugPanel.h"
#include "EventBus.h"
#include "LabAssets.h"
#include "LabChecklist.h"
#include "Scene.h"
#include "SceneNode.h"
#include "Sprite.h"
#include "UIRoot.h"

class TextureAsset;
namespace game { class GameState; }

class SpriteLab : public Scene {
public:
    explicit SpriteLab(game::GameState& gs);
    ~SpriteLab() override;
    void enter(Renderer& r) override;
    void exit(Renderer& r) override;
    void update(float dt, Renderer& r) override;
    void render(Renderer& r) override;
    void onResize(int w, int h, Renderer& r) override;
    SceneNode* shadowRoot() const override { return root_.get(); }

private:
    struct SheetAsset {
        std::string name;
        sprite::Sheet sheet;
        std::shared_ptr<TextureAsset> texture;
        const Material* material = nullptr;
    };
    struct Actor {
        SceneNode* node = nullptr;
        const SheetAsset* sheet = nullptr;
        sprite::Player player;
        math::Vec3 feet{0.0f};
        float yawDeg = 0.0f;
        int facing = 0;
        int hits = 0;
    };

    void loadSheets(Renderer& r);
    void spawn();
    void buildUI(Renderer& r);
    void resetCamera(Renderer& r);
    void restart();
    void playClip(int i);
    void placeActors(Renderer& r);
    int pickAt(Renderer& r, float x, float y) const;
    sprite::ScreenRect rectOf(Renderer& r, const Actor& a) const;
    void autoDrive(float dt, Renderer& r);
    bool facingOk(Renderer& r) const;

    game::GameState& gs_;
    std::unique_ptr<game::LabAssets> assets_;
    std::vector<SheetAsset> sheets_;
    std::unique_ptr<SceneNode> root_;
    std::unique_ptr<grid::BattleGrid> grid_;
    std::vector<Actor> actors_;

    std::unique_ptr<ui::UIRoot> ui_;
    std::unique_ptr<game::DebugPanel> panel_;
    std::unique_ptr<game::LabChecklist> checks_;
    std::vector<EventBus::SubscriptionId> subs_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);

    int   clip_ = 0;
    int   picked_ = -1;
    bool  tilt_ = true;
    float yawTarget_ = 0.0f;
    float yaw_ = 0.0f;
    bool  autoOn_ = true;
    int   step_ = 0;
    float stepT_ = 0.0f;
    std::vector<int> framesSeen_;
    bool  facingAll_ = true;
};

#endif
