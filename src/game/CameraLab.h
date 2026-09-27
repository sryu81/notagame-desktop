#ifndef NOT_A_GAME_CAMERA_LAB_H
#define NOT_A_GAME_CAMERA_LAB_H

#include <memory>
#include <vector>

#include "DebugPanel.h"
#include "EventBus.h"
#include "LabAssets.h"
#include "LabChecklist.h"
#include "Math.h"
#include "Scene.h"
#include "SceneNode.h"
#include "UIRoot.h"

namespace game { class GameState; }

class CameraLab : public Scene {
public:
    explicit CameraLab(game::GameState& gs) : gs_(gs) {}
    void enter(Renderer& r) override;
    void exit(Renderer& r) override;
    void update(float dt, Renderer& r) override;
    void render(Renderer& r) override;
    void onResize(int w, int h, Renderer& r) override;

private:
    void buildUI(Renderer& r);
    void resetCamera(Renderer& r);
    void applyProjection(Renderer& r);
    void applyPreset(Renderer& r, int preset);
    void restartAutoDrive(Renderer& r);
    void autoDrive(float dt, Renderer& r);
    void checkInstant(Renderer& r);
    void checkPicks(Renderer& r);
    void onTap(float x, float y, Renderer& r);
    math::Vec2 project(const math::Vec3& p, Renderer& r) const;

    game::GameState& gs_;
    std::unique_ptr<game::LabAssets> assets_;
    std::unique_ptr<SceneNode> root_;
    std::unique_ptr<SceneNode> targets_;
    std::vector<SceneNode*> targetNodes_;
    std::vector<math::Vec3> targetColors_;
    SceneNode* mover_ = nullptr;
    std::unique_ptr<ui::UIRoot> ui_;
    ui::UIText* status_ = nullptr;
    std::unique_ptr<game::DebugPanel> panel_;
    std::unique_ptr<game::LabChecklist> checks_;
    std::vector<EventBus::SubscriptionId> subs_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);

    float time_   = 0.0f;
    bool  autoOn_ = true;
    float autoT_  = 0.0f;
    int   step_   = 0;
    int   preset_ = 0;
    bool  ortho_  = false;
    bool  follow_ = false;
    bool  forwardX_ = false;
    bool  tightLimits_ = false;
    int   followFrames_ = 0;
    math::Vec3 followStart_{0.0f};
    int   moveEvents_ = 0;
    int   upEvents_   = 0;
    int   keyEvents_  = 0;
    math::Vec3 dragStartTarget_{0.0f};
    bool  inDrag_ = false;
};

#endif
