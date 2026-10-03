#ifndef NOT_A_GAME_PICK_LAB_H
#define NOT_A_GAME_PICK_LAB_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "BattleGrid.h"
#include "DebugPanel.h"
#include "EntityBuilder.h"
#include "EventBus.h"
#include "LabAssets.h"
#include "LabChecklist.h"
#include "Picker.h"
#include "Scene.h"
#include "SceneNode.h"
#include "UIRoot.h"
#include "UnitCatalog.h"

namespace game { class GameState; class Unit; }

class PickLab : public Scene {
public:
    explicit PickLab(game::GameState& gs);
    ~PickLab() override;
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
    void onTap(float x, float y, Renderer& r);
    void onHover(float x, float y, Renderer& r);
    void placeRing(SceneNode* ring, const game::Unit* u);
    math::Vec2 bodyPoint(Renderer& r, const game::Unit* u, float t) const;
    void runChecks(Renderer& r);

    game::GameState& gs_;
    std::unique_ptr<game::LabAssets> assets_;
    std::unique_ptr<SceneNode> root_;
    std::unique_ptr<SceneNode> rings_;
    SceneNode* selRing_ = nullptr;
    SceneNode* hoverRing_ = nullptr;
    anim::Skeleton skel_;
    std::map<std::string, anim::SkeletalClip> clips_;
    asset::Handle hRig_ = asset::kNull;
    game::UnitCatalog catalog_;
    std::unique_ptr<grid::BattleGrid> grid_;
    std::unique_ptr<game::EntityBuilder> builder_;
    std::vector<game::Unit*> units_;
    game::Unit* front_ = nullptr;
    game::Unit* back_ = nullptr;
    game::Unit* lone_ = nullptr;
    game::Unit* corpse_ = nullptr;
    pick::Picker picker_;
    game::Unit* selected_ = nullptr;
    game::Unit* hovered_ = nullptr;
    int taps_ = 0;
    std::string info_;

    std::unique_ptr<ui::UIRoot> ui_;
    std::unique_ptr<game::DebugPanel> panel_;
    std::unique_ptr<game::LabChecklist> checks_;
    ui::UIText* infoText_ = nullptr;
    std::vector<EventBus::SubscriptionId> subs_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);

    bool  autoOn_ = true;
    int   step_ = 0;
    float stepT_ = 0.0f;
};

#endif
