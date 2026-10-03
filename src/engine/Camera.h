#ifndef NOT_A_GAME_CAMERA_H
#define NOT_A_GAME_CAMERA_H

#include <functional>
#include <vector>

#include "Math.h"

class Camera {
public:
    Camera();

    void setOrthographic(float halfHeight, float aspect, float near, float far);
    void setPerspective(float fovYRadians, float aspect, float near, float far);
    void setFovYRadians(float fovYRadians);
    void setViewLookAt(const math::Vec3& eye, const math::Vec3& target, const math::Vec3& up);
    void setViewportSize(int width, int height);

    int width() const;
    int height() const;
    float aspect() const;
    const math::Mat4& projection() const;
    const math::Mat4& view() const;
    math::Mat4 viewProjection() const;
    const math::Vec3 &eye() const { return eye_; }
    const math::Vec3 &target() const { return target_; }
    const math::Vec3 &viewDir() const { return viewDir_; }
    float distance() const { return distance_; }

    void setDistanceLimits(float min, float max, float defaultDistance);
    void setDistance(float d);
    void zoomBy(float factor);
    void resetDistance();
    using TerrainHeightFn = std::function<float(float x, float z)>;
    void setTerrainHeightFn(TerrainHeightFn fn, float offset = 0.5f);
    void setTarget(const math::Vec3& t);
    void pan(float dx, float dy);
    void setTargetBounds(const math::Vec2& minXZ, const math::Vec2& maxXZ);
    void clearTargetBounds() { hasBounds_ = false; }
    bool inView(const math::Vec3& p, float margin = 0.0f) const;
    void frame(const std::vector<math::Vec3>& points, float margin = 0.1f);

    // 2.5D orbit controls
    void setForward(const math::Vec3& forward);
    void orbit(float dyawDeg, float dpitchDeg);
    void setOrbitSnap(float pitchDeg, float yawDeg);
    void setPitchLimits(float minDeg, float maxDeg);
    void setYawLimits(float minDeg, float maxDeg);
    float pitch() const { return pitch_; }
    float yaw() const { return yaw_; }
    float roll() const { return roll_; }
    float fovYRadians() const { return fovY_; }
    float minPitchDeg() const { return minPitchDeg_; }
    float maxPitchDeg() const { return maxPitchDeg_; }
    float minYawDeg()   const { return minYawDeg_; }
    float maxYawDeg()   const { return maxYawDeg_; }

    static constexpr float kMinPitchDeg = 15.0f;
    static constexpr float kMaxPitchDeg = 70.0f;
    static constexpr float kMinYawDeg = -45.0f;
    static constexpr float kMaxYawDeg = 45.0f;
    static constexpr float kIsoPitchDeg = 35.0f;
    static constexpr float kIsoYawDeg = 0.0f;

    static void selfTest();

private:
    void recomputeEyeFromOrbit();
    void clampTarget();

    math::Mat4 view_;
    math::Mat4 projection_;
    math::Vec3 eye_{0.0f, 0.0f, 0.0f};
    math::Vec3 target_{0.0f, 0.0f, 0.0f};
    math::Vec3 up_{0.0f, 1.0f, 0.0f};
    math::Vec3 viewDir_{0.0f, 0.0f, -1.0f};
    math::Vec3 forward_{0.0f, 0.0f, -1.0f};
    float distance_ = 1.0f;
    float minDistance_ = 0.1f;
    float maxDistance_ = 1000.0f;
    float defaultDistance_ = 1.0f;
    bool hasDistanceLimits_ = false;
    int width_;
    int height_;

    float fovY_ = 0.0f;
    float nearPlane_ = 0.1f;
    float farPlane_  = 100.0f;

    float pitch_ = kIsoPitchDeg;
    float yaw_ = kIsoYawDeg;
    float roll_ = 0.0f;
    float minPitchDeg_ = kMinPitchDeg;
    float maxPitchDeg_ = kMaxPitchDeg;
    float minYawDeg_   = kMinYawDeg;
    float maxYawDeg_   = kMaxYawDeg;

    bool hasBounds_ = false;
    math::Vec2 boundsMin_{0.0f, 0.0f};
    math::Vec2 boundsMax_{0.0f, 0.0f};

    TerrainHeightFn terrainHeightFn_;
    float terrainOffset_ = 0.5f;
};

#endif
