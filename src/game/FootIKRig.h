#ifndef NOT_A_GAME_FOOT_IK_RIG_H
#define NOT_A_GAME_FOOT_IK_RIG_H

#include <functional>
#include <vector>

#include "Bone.h"
#include "Math.h"

class SceneNode;

namespace game {

class FootIKRig {
public:
    using GroundFn = std::function<float(float x, float z)>;

    struct Leg {
        int upper = -1, lower = -1, foot = -1;
        float upperLen = 0.0f, lowerLen = 0.0f;
        bool clamped = false;
        bool hasOverride = false;
        math::Vec3 overrideModel{0.0f};
        math::Vec3 targetWorld{0.0f};
    };

    bool bind(const anim::Skeleton& skel);
    void attach(SceneNode* node, GroundFn ground, float footHeight);
    void detach();

    void setEnabled(bool e) { enabled_ = e; }
    bool enabled() const { return enabled_; }
    void setOverride(int leg, const math::Vec3& modelTarget);
    void clearOverride(int leg);

    const Leg& leg(int i) const { return legs_[i]; }
    int hips() const { return hips_; }
    float lastHipDrop() const { return lastDrop_; }

    void apply(std::vector<math::Mat4>& perBoneLocal);

    static void selfTest();

private:
    const anim::Skeleton* skel_ = nullptr;
    SceneNode* node_ = nullptr;
    GroundFn ground_;
    float footHeight_ = 0.0f;
    bool enabled_ = false;
    int hips_ = -1;
    Leg legs_[2];
    float lastDrop_ = 0.0f;
};

} // namespace game

#endif
