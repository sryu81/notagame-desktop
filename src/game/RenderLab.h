#ifndef NOT_A_GAME_RENDER_LAB_H
#define NOT_A_GAME_RENDER_LAB_H

#include <memory>
#include <vector>

#include "Animation.h"
#include "Bone.h"
#include "DebugPanel.h"
#include "EventBus.h"
#include "LabAssets.h"
#include "LabChecklist.h"
#include "Math.h"
#include "Mesh.h"
#include "Scene.h"
#include "SceneNode.h"
#include "UIRoot.h"

class TextureAsset;
namespace game { class GameState; }

class RenderLab : public Scene {
public:
    explicit RenderLab(game::GameState& gs) : gs_(gs) {}
    void enter(Renderer& r) override;
    void exit(Renderer& r) override;
    void update(float dt, Renderer& r) override;
    void render(Renderer& r) override;
    void onResize(int w, int h, Renderer& r) override;
    SceneNode* shadowRoot() const override { return root_.get(); }

private:
    struct DepthProbe { math::Vec3 cubePos; bool depthWrite; };

    void loadContent(Renderer& r);
    void buildScene();
    void buildUI(Renderer& r);
    void resetCamera(Renderer& r);
    void restartAutoDrive();
    void autoDrive(float dt, Renderer& r);
    void checkStatic(Renderer& r);
    void checkSkinning();
    void readDepthProbes(Renderer& r);
    void readTexturePixels(Renderer& r);
    void loadHeavyMesh(Renderer& r);
    math::Vec2 project(const math::Vec3& p, Renderer& r) const;

    game::GameState& gs_;
    std::unique_ptr<game::LabAssets> assets_;
    std::shared_ptr<TextureAsset> texture_;
    const Material* texMat_ = nullptr;
    std::unique_ptr<SceneNode> root_;
    SceneNode* skinnedNode_ = nullptr;
    SceneNode* gizmoNode_ = nullptr;
    Mesh unitMesh_;
    anim::Skeleton unitSkeleton_;
    anim::SkeletalClip unitIdle_;
    asset::Handle hUnit_ = asset::kNull;
    asset::Handle hGiant_ = asset::kNull;
    asset::Handle hSphere_ = asset::kNull;
    std::vector<math::Mat4> firstBones_;
    std::vector<DepthProbe> probes_;

    std::unique_ptr<ui::UIRoot> ui_;
    std::unique_ptr<game::DebugPanel> panel_;
    std::unique_ptr<game::LabChecklist> checks_;
    std::vector<EventBus::SubscriptionId> subs_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);

    float time_ = 0.0f;
    bool  autoOn_ = true;
    float autoT_ = 0.0f;
    int   step_ = 0;
    bool  readPixels_ = false;
    bool  sunMoving_ = true;
    bool  gizmo_ = false;
    bool  uiDirty_ = false;
    float savedShadowExtent_ = 12.0f;
    math::Vec3 followProbe_{20.0f, 0.5f, 0.0f};
};

#endif
