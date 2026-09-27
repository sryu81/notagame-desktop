#include "FootIK.h"

#include "AndroidOut.h"

#include <cassert>
#include <cmath>

namespace engine {
namespace foot_ik {

namespace {

math::Vec3 safeNormalize(const math::Vec3& v, const math::Vec3& fallback) {
    const float len = v.length();
    if (len < 1e-5f) return fallback;
    return v / len;
}

math::Mat4 rotationAroundPoint(const math::Mat4& rot, const math::Vec3& pivot) {
    math::Mat4 t  = math::Mat4::identity();
    math::Mat4 tInv = math::Mat4::identity();
    t.m[12] = pivot.x;  t.m[13] = pivot.y;  t.m[14] = pivot.z;
    tInv.m[12] = -pivot.x;  tInv.m[13] = -pivot.y;  tInv.m[14] = -pivot.z;
    return t * rot * tInv;
}

}

TwoBoneResult solveTwoBone(
    const math::Vec3& hipPos,
    float upperLen,
    float lowerLen,
    const math::Vec3& footTarget,
    const math::Vec3& hintDir)
{
    TwoBoneResult r;

    if (upperLen < 1e-4f || lowerLen < 1e-4f) {
        r.reachable = false;
        r.solvedFootPos = hipPos + math::Vec3(0, -(upperLen + lowerLen), 0);
        return r;
    }

    const math::Vec3 restFootPos = hipPos + math::Vec3(0, -(upperLen + lowerLen), 0);
    const math::Vec3 toTarget = footTarget - hipPos;
    const float dRaw = toTarget.length();
    if (dRaw < 1e-4f) {
        r.reachable = false;
        r.solvedFootPos = restFootPos;
        return r;
    }

    const float maxReach = upperLen + lowerLen - 1e-3f;
    const float minReach = std::abs(upperLen - lowerLen) + 1e-3f;
    float d = dRaw;
    const math::Vec3 toTargetN = toTarget / dRaw;
    if (d > maxReach) { d = maxReach; r.clamped = true; }
    if (d < minReach) { d = minReach; r.clamped = true; }
    r.reachable = !r.clamped;

    const math::Vec3 hintN = safeNormalize(hintDir, math::Vec3(1, 0, 0));
    math::Vec3 perp = toTargetN - hintN * math::dot(toTargetN, hintN);
    if (perp.length() < 1e-4f) {
        perp = math::cross(toTargetN, math::Vec3(0, 1, 0));
        if (perp.length() < 1e-4f) perp = math::cross(toTargetN, math::Vec3(0, 0, 1));
    }
    const math::Vec3 bendAxis = perp.normalized();

    const math::Vec3 restLowerDir = math::Vec3(0, -1, 0);
    const float cosAlpha = std::max(-1.0f, std::min(1.0f,
        (upperLen * upperLen + d * d - lowerLen * lowerLen) / (2.0f * upperLen * d)));
    const float alpha = std::acos(cosAlpha);

    const math::Mat4 hipRot = math::Mat4::rotation(
        math::Quat::fromAxisAngle(bendAxis, alpha));
    const math::Mat4 hipRotWorld = rotationAroundPoint(hipRot, hipPos);
    const math::Vec4 knee4 = hipRotWorld * math::Vec4(hipPos.x, hipPos.y - upperLen, hipPos.z, 1.0f);
    const math::Vec3 newKnee(knee4.x, knee4.y, knee4.z);

    const math::Vec3 newFootDir = safeNormalize(footTarget - newKnee, math::Vec3(0, -1, 0));
    const math::Quat kneeRot = math::Quat::fromAxisAngle(bendAxis, std::acos(std::max(-1.0f, std::min(1.0f,
        math::dot(newFootDir, restLowerDir)))));

    r.hipRotation    = math::Quat::fromAxisAngle(bendAxis, alpha);
    r.kneeRotation   = kneeRot;
    if (r.clamped) r.solvedFootPos = hipPos + toTargetN * d;
    else           r.solvedFootPos = footTarget;
    return r;
}

void selfTest() {
    aout << "[foot_ik] self-test starting" << std::endl;

    {
        const TwoBoneResult r = solveTwoBone(
            math::Vec3(0, 1, 0), 0.5f, 0.5f,
            math::Vec3(0, 0.5f, 0), math::Vec3(0, 0, 1));
        assert(r.reachable);
        assert((r.solvedFootPos - math::Vec3(0, 0.5f, 0)).length() < 1e-3f);
    }

    {
        const TwoBoneResult r = solveTwoBone(
            math::Vec3(0, 1, 0), 0.5f, 0.5f,
            math::Vec3(0.2f, 0.5f, 0), math::Vec3(0, 0, 1));
        assert(r.reachable);
        assert((r.solvedFootPos - math::Vec3(0.2f, 0.5f, 0)).length() < 1e-3f);
    }

    {
        const TwoBoneResult r = solveTwoBone(
            math::Vec3(0, 1, 0), 0.5f, 0.5f,
            math::Vec3(10.0f, 0, 0), math::Vec3(0, 0, 1));
        assert(r.clamped);
        assert(!r.reachable);
        assert((r.solvedFootPos - math::Vec3(0, 1, 0)).length() <= 1.0f + 1e-3f);
    }

    {
        const TwoBoneResult a = solveTwoBone(
            math::Vec3(0, 1, 0), 0.5f, 0.5f,
            math::Vec3(0, 0.5f, 0.3f), math::Vec3(0, 1, 0));
        const TwoBoneResult b = solveTwoBone(
            math::Vec3(0, 1, 0), 0.5f, 0.5f,
            math::Vec3(0, 0.5f, -0.3f), math::Vec3(0, 1, 0));
        assert(a.reachable);
        assert(b.reachable);
        assert(std::abs(a.hipRotation.y + b.hipRotation.y) < 1e-3f);
        assert(std::abs(a.hipRotation.x) < 1e-3f);
        assert(std::abs(b.hipRotation.x) < 1e-3f);
    }

    {
        const TwoBoneResult r = solveTwoBone(
            math::Vec3(0, 1, 0), 0.5f, 0.5f,
            math::Vec3(0, 1, 0), math::Vec3(0, 0, 1));
        assert(!r.reachable);
    }

    aout << "[foot_ik] self-test PASS" << std::endl;
}

}
}
