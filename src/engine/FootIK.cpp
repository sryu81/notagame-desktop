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

math::Quat fromTo(const math::Vec3& a, const math::Vec3& b) {
    const float c = math::dot(a, b);
    if (c < -0.99999f) {
        math::Vec3 axis = math::cross(a, math::Vec3(1, 0, 0));
        if (axis.length() < 1e-4f) axis = math::cross(a, math::Vec3(0, 0, 1));
        return math::Quat::fromAxisAngle(axis.normalized(), math::PI);
    }
    const math::Vec3 x = math::cross(a, b);
    math::Quat q{x.x, x.y, x.z, 1.0f + c};
    const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    return math::Quat{q.x / len, q.y / len, q.z / len, q.w / len};
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

    const math::Vec3 restDir(0, -1, 0);
    math::Vec3 hintN = safeNormalize(hintDir, math::Vec3(0, 0, 1));
    math::Vec3 planeN = math::cross(toTargetN, hintN);
    if (planeN.length() < 1e-4f) {
        planeN = math::cross(toTargetN, math::Vec3(0, 0, 1));
        if (planeN.length() < 1e-4f) planeN = math::cross(toTargetN, math::Vec3(1, 0, 0));
    }
    planeN = planeN.normalized();

    const float cosAlpha = std::max(-1.0f, std::min(1.0f,
        (upperLen * upperLen + d * d - lowerLen * lowerLen) / (2.0f * upperLen * d)));
    const float alpha = std::acos(cosAlpha);
    const math::Vec3 upperDir = math::Mat4::rotation(math::Quat::fromAxisAngle(planeN, alpha))
                                    .transformDirection(toTargetN).normalized();
    const math::Vec3 knee = hipPos + upperDir * upperLen;
    const math::Vec3 clampedTarget = hipPos + toTargetN * d;
    const math::Vec3 lowerDir = safeNormalize(clampedTarget - knee, restDir);

    r.hipRotation = fromTo(restDir, upperDir);
    const math::Vec3 lowerInHip = math::Mat4::rotation(r.hipRotation.inverse())
                                      .transformDirection(lowerDir).normalized();
    r.kneeRotation = fromTo(restDir, lowerInHip);
    r.solvedFootPos = knee + lowerDir * lowerLen;
    return r;
}

namespace {

math::Vec3 forwardFoot(const math::Vec3& hip, float l1, float l2, const TwoBoneResult& r, math::Vec3* kneeOut) {
    const math::Mat4 hipR = math::Mat4::rotation(r.hipRotation);
    const math::Mat4 kneeR = hipR * math::Mat4::rotation(r.kneeRotation);
    const math::Vec3 knee = hip + hipR.transformDirection(math::Vec3(0, -l1, 0));
    if (kneeOut) *kneeOut = knee;
    return knee + kneeR.transformDirection(math::Vec3(0, -l2, 0));
}

}

void selfTest() {
    aout << "[foot_ik] self-test starting" << std::endl;

    const math::Vec3 hip(0, 1, 0);
    const math::Vec3 targets[] = {
        {0, 0.5f, 0}, {0.2f, 0.5f, 0}, {0, 0.5f, 0.3f}, {0, 0.5f, -0.3f}, {0.1f, 0.3f, 0.2f},
    };
    for (const auto& t : targets) {
        const TwoBoneResult r = solveTwoBone(hip, 0.5f, 0.5f, t, math::Vec3(0, 0, 1));
        assert(r.reachable && !r.clamped);
        math::Vec3 knee;
        const math::Vec3 foot = forwardFoot(hip, 0.5f, 0.5f, r, &knee);
        assert((foot - t).length() < 1e-3f);
        assert((r.solvedFootPos - t).length() < 1e-3f);
        assert(std::abs((knee - hip).length() - 0.5f) < 1e-4f);
    }

    {
        const TwoBoneResult r = solveTwoBone(hip, 0.5f, 0.5f, math::Vec3(0, 0.4f, 0), math::Vec3(0, 0, 1));
        math::Vec3 knee;
        forwardFoot(hip, 0.5f, 0.5f, r, &knee);
        assert(knee.z > 0.1f);
        const TwoBoneResult b = solveTwoBone(hip, 0.5f, 0.5f, math::Vec3(0, 0.4f, 0), math::Vec3(0, 0, -1));
        forwardFoot(hip, 0.5f, 0.5f, b, &knee);
        assert(knee.z < -0.1f);
    }

    {
        const TwoBoneResult r = solveTwoBone(hip, 0.5f, 0.5f, math::Vec3(10.0f, 0, 0), math::Vec3(0, 0, 1));
        assert(r.clamped && !r.reachable);
        const math::Vec3 foot = forwardFoot(hip, 0.5f, 0.5f, r, nullptr);
        assert(std::abs((foot - hip).length() - (1.0f - 1e-3f)) < 1e-3f);
        assert(math::dot((foot - hip).normalized(), (math::Vec3(10.0f, 0, 0) - hip).normalized()) > 0.9999f);
        assert((r.solvedFootPos - foot).length() < 1e-3f);
    }

    {
        const TwoBoneResult r = solveTwoBone(hip, 0.5f, 0.5f, hip, math::Vec3(0, 0, 1));
        assert(!r.reachable);
    }

    aout << "[foot_ik] self-test PASS" << std::endl;
}

}
}
