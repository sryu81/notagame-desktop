#ifndef NOT_A_GAME_ANIM_LAB_H
#define NOT_A_GAME_ANIM_LAB_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Action.h"
#include "Animation.h"
#include "BattleGrid.h"
#include "Bone.h"
#include "DebugPanel.h"
#include "EventBus.h"
#include "FootIKRig.h"
#include "LabAssets.h"
#include "LabChecklist.h"
#include "Mesh.h"
#include "Scene.h"
#include "SceneNode.h"
#include "UIRoot.h"

namespace game { class GameState; class Unit; }

class AnimLab : public Scene {
public:
    explicit AnimLab(game::GameState& gs);
    ~AnimLab() override;
    void enter(Renderer& r) override;
    void exit(Renderer& r) override;
    void update(float dt, Renderer& r) override;
    void render(Renderer& r) override;
    void onResize(int w, int h, Renderer& r) override;
    SceneNode* shadowRoot() const override { return root_.get(); }

private:
    bool loadRig(Renderer& r);
    void buildScene();
    void buildUI(Renderer& r);
    void resetCamera(Renderer& r);
    void resetRigPose();
    void playClip(const char* name, float rate = 1.0f);
    void restartAutoDrive();
    void autoDrive(float dt);
    void checkStatic();
    void observe(float dt);
    math::Vec3 jointWorld(int joint) const;
    math::Mat4 jointModel(int joint) const;

    game::GameState& gs_;
    std::unique_ptr<game::LabAssets> assets_;
    std::unique_ptr<SceneNode> root_;
    std::unique_ptr<game::Unit> unit_;
    SceneNode* rigNode_ = nullptr;
    SceneNode* cube_ = nullptr;
    anim::Skeleton skel_;
    std::map<std::string, anim::SkeletalClip> clips_;
    std::vector<math::Mat4> bindModel_;
    asset::Handle hRig_ = asset::kNull;
    anim::Clip hold_;
    anim::Clip cubeAttack_;
    std::unique_ptr<grid::BattleGrid> grid_;
    std::unique_ptr<action::Manager> actions_;
    game::FootIKRig ik_;
    bool rigLoaded_ = false;

    std::unique_ptr<ui::UIRoot> ui_;
    std::unique_ptr<game::DebugPanel> panel_;
    std::unique_ptr<game::LabChecklist> checks_;
    std::vector<EventBus::SubscriptionId> subs_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);

    std::string clipName_ = "Idle";
    float rate_ = 1.0f;
    bool  autoOn_ = true;
    float t_ = 0.0f;
    float stepT_ = 0.0f;
    int   step_ = 0;

    int   idleWraps_ = 0;
    float prevWrapped_ = -1.0f;
    int   untrackedOk_ = 0;
    int   untrackedBad_ = 0;
    std::vector<float> hitTimes_;
    float playStart_ = 0.0f;
    std::vector<float> playStarts_;
    int   footL_ = 0;
    std::vector<float> footLTimes_;
    int   rigidHits_ = 0;
    int   heldFrames_ = 0;
    bool  sawWalkInMove_ = false;
    bool  slowRateOk_ = false;
    bool  walkStarted_ = false;
    std::vector<math::Mat4> heldPose_;
};

#endif
