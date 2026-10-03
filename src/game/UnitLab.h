#ifndef NOT_A_GAME_UNIT_LAB_H
#define NOT_A_GAME_UNIT_LAB_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Action.h"
#include "Animation.h"
#include "BattleGrid.h"
#include "Bone.h"
#include "DebugPanel.h"
#include "EntityBuilder.h"
#include "EventBus.h"
#include "LabAssets.h"
#include "LabChecklist.h"
#include "Scene.h"
#include "SceneNode.h"
#include "UIRoot.h"
#include "UnitCatalog.h"

namespace game { class GameState; class Unit; }

class UnitLab : public Scene {
public:
    explicit UnitLab(game::GameState& gs);
    ~UnitLab() override;
    void enter(Renderer& r) override;
    void exit(Renderer& r) override;
    void update(float dt, Renderer& r) override;
    void render(Renderer& r) override;
    void onResize(int w, int h, Renderer& r) override;
    SceneNode* shadowRoot() const override { return root_.get(); }

private:
    bool loadRig(Renderer& r);
    void buildCatalog();
    void buildScene();
    void spawnUnits();
    void buildUI(Renderer& r);
    void resetCamera(Renderer& r);
    void restartAutoDrive();
    void autoDrive(float dt);
    void observe();
    void updateRing();
    const anim::SkeletalClip* clip(const char* name) const;

    game::GameState& gs_;
    std::unique_ptr<game::LabAssets> assets_;
    std::unique_ptr<SceneNode> root_;
    std::unique_ptr<SceneNode> ring_;
    SceneNode* unitsRoot_ = nullptr;
    anim::Skeleton skel_;
    std::map<std::string, anim::SkeletalClip> clips_;
    asset::Handle hRig_ = asset::kNull;
    bool rigLoaded_ = false;
    game::UnitCatalog catalog_;
    std::unique_ptr<grid::BattleGrid> grid_;
    std::unique_ptr<game::EntityBuilder> builder_;
    std::unique_ptr<action::Manager> actions_;
    game::Unit* knight_ = nullptr;
    game::Unit* goblin_ = nullptr;
    game::Unit* golem_ = nullptr;

    std::unique_ptr<ui::UIRoot> ui_;
    std::unique_ptr<game::DebugPanel> panel_;
    std::unique_ptr<game::LabChecklist> checks_;
    std::vector<EventBus::SubscriptionId> subs_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);

    bool  autoOn_ = true;
    int   step_ = 0;
    float stepT_ = 0.0f;
    int   frame_ = 0;

    bool  scaleOk_ = true;
    bool  sawWalk_ = false;
    bool  sawAttack_ = false;
    int   lastHp_ = 0;
    int   hpChanges_ = 0;
    int   hitFrame_ = -1;
    int   hurtFrame_ = -1;
    bool  ringShown_ = false;
    int   heldFrames_ = 0;
    std::vector<math::Mat4> deathPose_;
};

#endif
