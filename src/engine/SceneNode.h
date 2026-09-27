#ifndef NOT_A_GAME_SCENE_NODE_H
#define NOT_A_GAME_SCENE_NODE_H

#include <deque>
#include <functional>
#include <memory>
#include <vector>

#include "Animation.h"
#include "Bone.h"
#include "Math.h"
#include "Mesh.h"

class Model;
struct Material;

class SceneNode {
public:
    struct Drawable {
        const Mesh *mesh = nullptr;
        const Model *model = nullptr;
        math::Vec3 color{1.0f, 1.0f, 1.0f};
        const Material *material = nullptr;
        bool depthWrite = true;
    };

    SceneNode() = default;
    ~SceneNode() = default;

    SceneNode(const SceneNode &) = delete;
    SceneNode &operator=(const SceneNode &) = delete;

    void setLocal(const math::Mat4 &m);
    const math::Mat4 &local() const { return local_; }

    void setClip(const anim::Clip *clip) { clip_ = clip; }
    const anim::Clip *clip() const { return clip_; }
    void setTime(float t) { time_ = t; }
    float time() const { return time_; }
    void setPlaybackRate(float r) { playbackRate_ = r; }

    using AnimEventCallback = std::function<void(int eventId)>;
    void setAnimEventCallback(AnimEventCallback cb) { animEventCb_ = std::move(cb); }
    const AnimEventCallback& animEventCallback() const { return animEventCb_; }

    void setSkeleton(const anim::Skeleton *s) { skeleton_ = s; }
    const anim::Skeleton *skeleton() const { return skeleton_; }

    void setSkeletalClip(const anim::SkeletalClip *clip) { skeletalClip_ = clip; }
    const anim::SkeletalClip *skeletalClip() const { return skeletalClip_; }

    const std::vector<math::Mat4> &boneMatrices() const { return boneMatrices_; }
    bool hasBoneMatrices() const { return !boneMatrices_.empty(); }
    void setBoneMatrices(const std::vector<math::Mat4> &m) { boneMatrices_ = m; }

    void update(float dt);

    SceneNode *parent() const { return parent_; }
    const std::deque<std::unique_ptr<SceneNode>> &children() const { return children_; }

    SceneNode *addChild();
    void addChild(std::unique_ptr<SceneNode> child);

    void clearChildren() { children_.clear(); }

    void setAnimOffset(const math::Vec3 &o) { animOffset_ = o; }
    const math::Vec3 &animOffset() const { return animOffset_; }

    void setAnimRotOffset(const math::Quat &q) { animRotOffset_ = q; }
    const math::Quat &animRotOffset() const { return animRotOffset_; }

    void setBaseOffset(const math::Vec3 &o) { baseOffset_ = o; }
    const math::Vec3 &baseOffset() const { return baseOffset_; }

    void addDrawable(Drawable d) { drawables_.push_back(d); }
    const std::deque<Drawable> &drawables() const { return drawables_; }
    std::deque<Drawable> &drawables() { return drawables_; }

    math::Mat4 world() const;

    static void selfTest();

private:
    void markDirty();

    math::Mat4 local_ = math::Mat4::identity();
    SceneNode *parent_ = nullptr;
    std::deque<std::unique_ptr<SceneNode>> children_;
    std::deque<Drawable> drawables_;

    const anim::Clip *clip_ = nullptr;
    const anim::SkeletalClip *skeletalClip_ = nullptr;
    const anim::Skeleton *skeleton_ = nullptr;
    float time_ = 0.0f;
    float playbackRate_ = 1.0f;
    std::vector<math::Mat4> boneMatrices_;

    AnimEventCallback animEventCb_;
    float prevEventTime_ = -1.0f;
    math::Vec3 animOffset_{0.0f};
    math::Quat animRotOffset_;
    math::Vec3 baseOffset_{0.0f};

    mutable math::Mat4 cachedWorld_ = math::Mat4::identity();
    mutable bool dirty_ = true;
};

#endif
