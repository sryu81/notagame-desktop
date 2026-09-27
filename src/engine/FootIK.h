#ifndef NOT_A_GAME_FOOT_IK_H
#define NOT_A_GAME_FOOT_IK_H

#include "Math.h"

namespace engine {
namespace foot_ik {

struct TwoBoneResult {
    bool         reachable = true;
    bool         clamped   = false;
    math::Quat   hipRotation  = math::Quat::identity();
    math::Quat   kneeRotation = math::Quat::identity();
    math::Vec3   solvedFootPos{0.0f, 0.0f, 0.0f};
};

// Analytical 2-bone IK. The chain has 3 nodes in MODEL space:
//   hipPos (root, fixed) → kneePos → footPos (end effector).
// In REST POSE, knee and foot lie on a straight line below the hip:
//   upperLen = |kneePos - hipPos|
//   lowerLen = |footPos - kneePos|
//   knee - hip points "down" in rest (we treat +Y as up).
// The solver returns HIP and KNEE rotations to apply so the foot reaches
// `footTarget` (in MODEL space). `hintDir` is a model-space hint for the
// knee bend plane (e.g. unit's forward direction).
//
//   hipRotation  — applied to the hip node (rotates the entire chain)
//   kneeRotation — applied to the knee node (rotates the lower segment)
//
// Use the returned quaternions to drive SceneNode local rotations:
//   hipNode->setLocal(Mat4::translation(hipPos) * Mat4::rotation(hipRotation));
//   kneeNode->setLocal(Mat4::translation(0,-upperLen,0) * Mat4::rotation(kneeRotation));
TwoBoneResult solveTwoBone(
    const math::Vec3& hipPos,
    float upperLen,
    float lowerLen,
    const math::Vec3& footTarget,
    const math::Vec3& hintDir);

void selfTest();

}
}

#endif
