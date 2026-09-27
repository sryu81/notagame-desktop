#include "FootIKRig.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "Engine.h"

namespace game {

namespace {

constexpr float kReachMargin = 0.02f;
const char* kLegNames[2][3] = {
    {"UpperLeg.L", "LowerLeg.L", "Foot.L"},
    {"UpperLeg.R", "LowerLeg.R", "Foot.R"},
};

math::Vec3 translationOf(const math::Mat4& m) { return math::Vec3(m.m[12], m.m[13], m.m[14]); }

std::vector<math::Mat4> modelSpace(const anim::Skeleton& skel, const std::vector<math::Mat4>& local) {
    std::vector<math::Mat4> out(local.size());
    for (size_t i = 0; i < local.size(); ++i) {
        const int p = skel.bones[i].parent;
        out[i] = p >= 0 ? out[p] * local[i] : local[i];
    }
    return out;
}

}

bool FootIKRig::bind(const anim::Skeleton& skel) {
    skel_ = &skel;
    bool ok = true;
    for (int l = 0; l < 2; ++l) {
        legs_[l] = Leg{};
        legs_[l].upper = skel.find(kLegNames[l][0]);
        legs_[l].lower = skel.find(kLegNames[l][1]);
        legs_[l].foot  = skel.find(kLegNames[l][2]);
        ok = ok && legs_[l].upper >= 0 && legs_[l].lower >= 0 && legs_[l].foot >= 0;
    }
    if (!ok) return false;
    hips_ = skel.bones[legs_[0].upper].parent;
    std::vector<math::Mat4> rest(skel.bones.size());
    for (size_t i = 0; i < rest.size(); ++i) rest[i] = skel.bones[i].restLocal;
    const auto model = modelSpace(skel, rest);
    for (auto& leg : legs_) {
        leg.upperLen = (translationOf(model[leg.lower]) - translationOf(model[leg.upper])).length();
        leg.lowerLen = (translationOf(model[leg.foot]) - translationOf(model[leg.lower])).length();
    }
    return hips_ >= 0 && hips_ == skel.bones[legs_[1].upper].parent;
}

void FootIKRig::attach(SceneNode* node, GroundFn ground, float footHeight) {
    node_ = node;
    ground_ = std::move(ground);
    footHeight_ = footHeight;
    node_->setPoseModifier([this](std::vector<math::Mat4>& local) { apply(local); });
}

void FootIKRig::detach() {
    if (node_) node_->setPoseModifier(nullptr);
    node_ = nullptr;
}

void FootIKRig::setOverride(int leg, const math::Vec3& modelTarget) {
    legs_[leg].hasOverride = true;
    legs_[leg].overrideModel = modelTarget;
}

void FootIKRig::clearOverride(int leg) {
    legs_[leg].hasOverride = false;
}

void FootIKRig::apply(std::vector<math::Mat4>& local) {
    if (!enabled_ || !skel_ || !node_ || !ground_ || hips_ < 0) return;
    const math::Mat4 world = node_->world();
    const math::Mat4 worldInv = world.inverted();
    const float scale = math::Vec3(world.m[0], world.m[1], world.m[2]).length();

    auto model = modelSpace(*skel_, local);
    float drop = 0.0f;
    for (auto& leg : legs_) {
        const math::Vec3 footW = world.transformPoint(translationOf(model[leg.foot]));
        leg.targetWorld = math::Vec3(footW.x, ground_(footW.x, footW.z) + footHeight_, footW.z);
        if (leg.hasOverride) continue;
        const math::Vec3 hipW = world.transformPoint(translationOf(model[leg.upper]));
        const float reach = (leg.upperLen + leg.lowerLen - kReachMargin) * scale;
        const float dx = hipW.x - leg.targetWorld.x, dz = hipW.z - leg.targetWorld.z;
        const float h2 = dx * dx + dz * dz;
        if (h2 >= reach * reach) continue;
        const float maxV = std::sqrt(reach * reach - h2);
        drop = std::max(drop, (hipW.y - leg.targetWorld.y) - maxV);
    }
    lastDrop_ = drop;
    if (drop > 0.0f) {
        local[hips_] = math::Mat4::translation(math::Vec3(0.0f, -drop / scale, 0.0f)) * local[hips_];
        model = modelSpace(*skel_, local);
    }

    const math::Mat4 parentInv = model[hips_].inverted();
    const math::Vec3 hint = parentInv.transformDirection(math::Vec3(0.0f, 0.0f, 1.0f)).normalized();
    for (auto& leg : legs_) {
        const math::Vec3 targetModel = leg.hasOverride ? leg.overrideModel
                                                       : worldInv.transformPoint(leg.targetWorld);
        const math::Vec3 targetP = parentInv.transformPoint(targetModel);
        const math::Vec3 hipP = translationOf(local[leg.upper]);
        const auto r = engine::foot_ik::solveTwoBone(hipP, leg.upperLen, leg.lowerLen, targetP, hint);
        leg.clamped = r.clamped;
        local[leg.upper] = math::Mat4::translation(hipP) * math::Mat4::rotation(r.hipRotation);
        local[leg.lower] = math::Mat4::translation(translationOf(local[leg.lower])) *
                           math::Mat4::rotation(r.kneeRotation);
    }
}

void FootIKRig::selfTest() {
    aout << "[foot-ik-rig] self-test starting" << std::endl;

    anim::Skeleton skel;
    auto addBone = [&skel](const char* name, int parent, const math::Vec3& off) {
        anim::Bone b;
        b.parent = parent;
        b.name = name;
        b.restLocal = math::Mat4::translation(off);
        math::Vec3 model = off;
        for (int p = parent; p >= 0; p = skel.bones[p].parent) model = model + translationOf(skel.bones[p].restLocal);
        b.invBindPose = math::Mat4::translation(model * -1.0f);
        skel.bones.push_back(b);
    };
    addBone("Hips", -1, {0.0f, 1.0f, 0.0f});
    addBone("UpperLeg.L", 0, {0.12f, 0.0f, 0.0f});
    addBone("LowerLeg.L", 1, {0.0f, -0.45f, 0.0f});
    addBone("Foot.L", 2, {0.0f, -0.45f, 0.0f});
    addBone("UpperLeg.R", 0, {-0.12f, 0.0f, 0.0f});
    addBone("LowerLeg.R", 4, {0.0f, -0.45f, 0.0f});
    addBone("Foot.R", 5, {0.0f, -0.45f, 0.0f});

    anim::SkeletalClip hold;
    hold.tracks.push_back({0, {{0.0f, math::Vec3(0.0f, 1.0f, 0.0f), math::Quat::identity(), math::Vec3(1.0f)},
                               {1.0f, math::Vec3(0.0f, 1.0f, 0.0f), math::Quat::identity(), math::Vec3(1.0f)}}});

    auto footY = [&skel](const SceneNode& n, int foot) {
        const math::Vec3 bind = skel.bones[foot].invBindPose.inverted().transformPoint(math::Vec3(0.0f));
        return n.world().transformPoint(n.boneMatrices()[foot].transformPoint(bind));
    };

    for (float groundY : {0.2f, -0.3f}) {
        SceneNode node;
        node.setSkeleton(&skel);
        node.setSkeletalClip(&hold);
        FootIKRig rig;
        assert(rig.bind(skel));
        assert(math::approxEqual(rig.leg(0).upperLen, 0.45f, 1e-5f));
        rig.attach(&node, [groundY](float, float) { return groundY; }, 0.1f);
        rig.setEnabled(true);
        node.update(0.016f);
        for (int l = 0; l < 2; ++l) {
            const math::Vec3 f = footY(node, rig.leg(l).foot);
            assert(math::approxEqual(f.y, groundY + 0.1f, 2e-3f));
            assert(!rig.leg(l).clamped);
        }
        if (groundY < 0.0f) assert(rig.lastHipDrop() > 0.3f);
        const math::Vec3 knee = footY(node, rig.leg(0).lower);
        const math::Vec3 hip = footY(node, rig.leg(0).upper);
        if (groundY > 0.0f) assert(knee.z > hip.z + 0.01f);

        rig.setOverride(0, math::Vec3(0.12f, -5.0f, 0.0f));
        node.update(0.016f);
        assert(rig.leg(0).clamped && !rig.leg(1).clamped);
        rig.detach();
    }

    aout << "[foot-ik-rig] self-test PASS" << std::endl;
}

} // namespace game
