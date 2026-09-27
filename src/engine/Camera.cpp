#include "Camera.h"

#include <cassert>
#include <cmath>

#include "AndroidOut.h"

Camera::Camera() :
        view_(math::Mat4::identity()),
        projection_(math::Mat4::identity()),
        width_(0),
        height_(0) {}

void Camera::setOrthographic(float halfHeight, float aspect, float near, float far) {
    projection_ = math::Mat4::orthographic(halfHeight, aspect, near, far);
}

void Camera::setPerspective(float fovYRadians, float aspect, float near, float far) {
    fovY_ = fovYRadians;
    nearPlane_ = near;
    farPlane_ = far;
    projection_ = math::Mat4::perspective(fovYRadians, aspect, near, far);
}

void Camera::setFovYRadians(float fovYRadians) {
    setPerspective(fovYRadians, aspect(), nearPlane_, farPlane_);
}

void Camera::setViewLookAt(const math::Vec3& eye, const math::Vec3& target, const math::Vec3& up) {
    eye_ = eye;
    target_ = target;
    up_ = up;
    const math::Vec3 delta = target - eye;
    distance_ = delta.length();
    if (distance_ > 1e-6f) {
        viewDir_ = delta * (1.0f / distance_);
    } else {
        viewDir_ = math::Vec3(0.0f, 0.0f, -1.0f);
    }
    if (!hasDistanceLimits_) {
        minDistance_ = distance_ * 0.25f;
        maxDistance_ = distance_ * 2.0f;
        defaultDistance_ = distance_;
    } else {
        if (distance_ < minDistance_) distance_ = minDistance_;
        if (distance_ > maxDistance_) distance_ = maxDistance_;
        eye_ = target - viewDir_ * distance_;
    }

    // Derive pitch and yaw from view direction
    pitch_ = std::abs(math::degrees(std::asin(math::clamp(viewDir_.y, -1.0f, 1.0f))));
    pitch_ = math::clamp(pitch_, minPitchDeg_, maxPitchDeg_);

    math::Vec3 viewDirXZ(viewDir_.x, 0.0f, viewDir_.z);
    float xzLen = viewDirXZ.length();
    if (xzLen > 1e-6f) {
        viewDirXZ = viewDirXZ * (1.0f / xzLen);
        float sinYaw = math::cross(forward_, viewDirXZ).y;
        float cosYaw = math::dot(forward_, viewDirXZ);
        yaw_ = math::degrees(std::atan2(sinYaw, cosYaw));
        yaw_ = math::clamp(yaw_, minYawDeg_, maxYawDeg_);
    } else {
        yaw_ = 0.0f;
    }
    roll_ = 0.0f;

    recomputeEyeFromOrbit();
    view_ = math::Mat4::lookAt(eye_, target_, up_);
}

void Camera::setViewportSize(int width, int height) {
    width_ = width;
    height_ = height;
}

int Camera::width() const { return width_; }
int Camera::height() const { return height_; }

float Camera::aspect() const {
    return height_ > 0 ? static_cast<float>(width_) / static_cast<float>(height_) : 1.0f;
}

const math::Mat4& Camera::projection() const { return projection_; }
const math::Mat4& Camera::view() const { return view_; }
math::Mat4 Camera::viewProjection() const { return projection_ * view_; }

void Camera::setDistanceLimits(float min, float max, float defaultDistance) {
    assert(min > 0.0f);
    assert(max >= min);
    minDistance_ = min;
    maxDistance_ = max;
    defaultDistance_ = defaultDistance;
    hasDistanceLimits_ = true;
    if (distance_ < minDistance_) setDistance(minDistance_);
    if (distance_ > maxDistance_) setDistance(maxDistance_);
}

void Camera::setDistance(float d) {
    if (hasDistanceLimits_) {
        if (d < minDistance_) d = minDistance_;
        if (d > maxDistance_) d = maxDistance_;
    }
    distance_ = d;
    eye_ = target_ - viewDir_ * distance_;
    view_ = math::Mat4::lookAt(eye_, target_, up_);
}

void Camera::zoomBy(float factor) {
    if (factor <= 0.0f) return;
    setDistance(distance_ / factor);
}

void Camera::resetDistance() {
    setDistance(defaultDistance_);
}

void Camera::setTerrainHeightFn(TerrainHeightFn fn, float offset) {
    terrainHeightFn_ = fn;
    terrainOffset_ = offset;
}

void Camera::setTarget(const math::Vec3& t) {
    target_ = t;
    if (terrainHeightFn_) {
        target_.y = terrainHeightFn_(target_.x, target_.z) + terrainOffset_;
    }
    recomputeEyeFromOrbit();
    view_ = math::Mat4::lookAt(eye_, target_, up_);
}

void Camera::pan(float dx, float dy) {
    const float h = static_cast<float>(height_);
    if (h < 1.0f) return;
    const float visH = 2.0f * distance_ * std::tan(fovY_ * 0.5f);
    const float visW = visH * aspect();
    const float worldPerPxX = visW / h;
    const float worldPerPxY = visH / h;
    const math::Vec3 right(view_.m[0], view_.m[4], view_.m[8]);
    const math::Vec3 up(view_.m[1], view_.m[5], view_.m[9]);
    target_ += right * (-dx * worldPerPxX) + up * (dy * worldPerPxY);
    if (terrainHeightFn_) {
        target_.y = terrainHeightFn_(target_.x, target_.z) + terrainOffset_;
    }
    recomputeEyeFromOrbit();
    view_ = math::Mat4::lookAt(eye_, target_, up_);
}

void Camera::setForward(const math::Vec3& forward) {
    forward_ = forward.normalized();
}

void Camera::recomputeEyeFromOrbit() {
    float pitchRad = math::radians(pitch_);
    float yawRad = math::radians(yaw_);

    // For forward_ = (0, 0, -1), offset maps yaw around Y + pitch around X.
    // yaw=0 → camera at +Z relative to target, looking toward -Z (forward_).
    // pitch>0 → camera pitched upward (eye.y > target.y, looking down).
    math::Vec3 offset(
        std::cos(pitchRad) * std::sin(yawRad),
        std::sin(pitchRad),
        std::cos(pitchRad) * std::cos(yawRad)
    );
    eye_ = target_ + offset * distance_;

    const math::Vec3 newDelta = target_ - eye_;
    if (newDelta.length() > 1e-6f) {
        viewDir_ = newDelta * (1.0f / newDelta.length());
    }
}

void Camera::orbit(float dyawDeg, float dpitchDeg) {
    yaw_ += dyawDeg;
    pitch_ += dpitchDeg;
    pitch_ = math::clamp(pitch_, minPitchDeg_, maxPitchDeg_);
    yaw_ = math::clamp(yaw_, minYawDeg_, maxYawDeg_);
    recomputeEyeFromOrbit();
    view_ = math::Mat4::lookAt(eye_, target_, up_);
}

void Camera::setOrbitSnap(float pitchDeg, float yawDeg) {
    pitch_ = math::clamp(pitchDeg, minPitchDeg_, maxPitchDeg_);
    yaw_   = math::clamp(yawDeg,   minYawDeg_,   maxYawDeg_);
    recomputeEyeFromOrbit();
    view_ = math::Mat4::lookAt(eye_, target_, up_);
}

void Camera::setPitchLimits(float minDeg, float maxDeg) {
    assert(minDeg < maxDeg);
    minPitchDeg_ = minDeg;
    maxPitchDeg_ = maxDeg;
    pitch_ = math::clamp(pitch_, minPitchDeg_, maxPitchDeg_);
    recomputeEyeFromOrbit();
    view_ = math::Mat4::lookAt(eye_, target_, up_);
}

void Camera::setYawLimits(float minDeg, float maxDeg) {
    assert(minDeg < maxDeg);
    minYawDeg_ = minDeg;
    maxYawDeg_ = maxDeg;
    yaw_ = math::clamp(yaw_, minYawDeg_, maxYawDeg_);
    recomputeEyeFromOrbit();
    view_ = math::Mat4::lookAt(eye_, target_, up_);
}

void Camera::selfTest() {
    aout << "[camera] self-test starting" << std::endl;

    Camera ortho;
    ortho.setViewportSize(800, 400);
    ortho.setOrthographic(2.0f, ortho.aspect(), -1.0f, 1.0f);

    math::Vec4 clipCenter = ortho.projection() * math::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    assert(math::approxEqual(clipCenter.x, 0.0f, 1e-4f));
    assert(math::approxEqual(clipCenter.y, 0.0f, 1e-4f));
    assert(math::approxEqual(clipCenter.z, 0.0f, 1e-4f));

    math::Vec4 clipRight = ortho.projection() * math::Vec4(2.0f * ortho.aspect(), 2.0f, 0.0f, 1.0f);
    assert(math::approxEqual(clipRight.x, 1.0f, 1e-4f));
    assert(math::approxEqual(clipRight.y, 1.0f, 1e-4f));

    Camera persp;
    persp.setViewportSize(800, 600);
    persp.setPerspective(math::radians(90.0f), persp.aspect(), 1.0f, 100.0f);
    persp.setViewLookAt(
            math::Vec3(2.0f, 2.0f, 5.0f),
            math::Vec3(0.0f, 0.0f, 0.0f),
            math::Vec3(0.0f, 1.0f, 0.0f));
    assert(persp.pitch() >= Camera::kMinPitchDeg - 0.01f);
    assert(persp.pitch() <= Camera::kMaxPitchDeg + 0.01f);
    assert(persp.yaw() >= Camera::kMinYawDeg - 0.01f);
    assert(persp.yaw() <= Camera::kMaxYawDeg + 0.01f);

    math::Vec3 eye(2.0f, 3.0f, 5.0f);
    math::Vec3 target(0.0f, 0.0f, 0.0f);
    Camera lookAtCam;
    lookAtCam.setViewportSize(800, 600);
    lookAtCam.setPerspective(math::radians(60.0f), lookAtCam.aspect(), 0.1f, 100.0f);
    lookAtCam.setViewLookAt(eye, target, math::Vec3(0.0f, 1.0f, 0.0f));
    assert(math::approxEqual(lookAtCam.view().transformPoint(eye), math::Vec3(0.0f, 0.0f, 0.0f), 1e-4f));

    math::Mat4 viewInv = lookAtCam.view().inverted();
    math::Mat4 viewRoundTrip = lookAtCam.view() * viewInv;
    for (int i = 0; i < 16; ++i) {
        float expected = (i == 0 || i == 5 || i == 10 || i == 15) ? 1.0f : 0.0f;
        assert(math::approxEqual(viewRoundTrip.m[i], expected, 1e-3f));
    }

    aout << "[camera] self-test PASS" << std::endl;

    // --- Zoom + distance tests (uses valid pitch) ---
    {
        Camera cam;
        cam.setViewportSize(800, 600);
        cam.setPerspective(math::radians(60.0f), cam.aspect(), 0.1f, 100.0f);

        const float initDist = std::sqrt(4.0f + 25.0f);
        cam.setViewLookAt(math::Vec3(0.0f, 2.0f, 5.0f), math::Vec3(0.0f, 0.0f, 0.0f),
                          math::Vec3(0.0f, 1.0f, 0.0f));
        assert(math::approxEqual(cam.distance(), initDist, 1e-4f));
        assert(math::approxEqual(cam.eye().x, 0.0f, 1e-4f));

        cam.setDistance(3.0f);
        assert(math::approxEqual(cam.distance(), 3.0f, 1e-4f));
        math::Vec3 expectedEye = cam.target() - cam.viewDir() * 3.0f;
        assert(math::approxEqual(cam.eye(), expectedEye, 1e-4f));

        cam.setDistanceLimits(1.0f, 10.0f, initDist);
        cam.setDistance(0.5f);
        assert(math::approxEqual(cam.distance(), 1.0f, 1e-4f));
        cam.setDistance(20.0f);
        assert(math::approxEqual(cam.distance(), 10.0f, 1e-4f));

        cam.setDistance(4.0f);
        cam.zoomBy(2.0f);
        assert(math::approxEqual(cam.distance(), 2.0f, 1e-4f));
        cam.zoomBy(0.5f);
        assert(math::approxEqual(cam.distance(), 4.0f, 1e-4f));
        cam.zoomBy(100.0f);
        assert(math::approxEqual(cam.distance(), 1.0f, 1e-4f));
        cam.zoomBy(0.001f);
        assert(math::approxEqual(cam.distance(), 10.0f, 1e-4f));

        cam.resetDistance();
        assert(math::approxEqual(cam.distance(), initDist, 1e-4f));
    }

    // --- Non-axis-aligned eye round-trip ---
    {
        Camera cam;
        cam.setViewportSize(800, 600);
        cam.setPerspective(math::radians(60.0f), cam.aspect(), 0.1f, 100.0f);
        cam.setViewLookAt(math::Vec3(2.0f, 3.0f, 5.0f), math::Vec3(0.0f, 0.0f, 0.0f),
                          math::Vec3(0.0f, 1.0f, 0.0f));
        const float d0 = cam.distance();
        assert(math::approxEqual(d0, std::sqrt(4.0f + 9.0f + 25.0f), 1e-3f));
        cam.setDistance(d0 * 0.5f);
        assert(math::approxEqual(cam.distance(), d0 * 0.5f, 1e-3f));
        cam.resetDistance();
        assert(math::approxEqual(cam.distance(), d0, 1e-3f));
    }

    aout << "[camera] zoom self-test PASS" << std::endl;

    // --- Orbit clamp test ---
    {
        Camera cam;
        cam.setViewportSize(800, 600);
        cam.setPerspective(math::radians(60.0f), cam.aspect(), 0.1f, 100.0f);
        float d = std::sqrt(2.0f * 2.0f + 5.0f * 5.0f);
        cam.setViewLookAt(math::Vec3(0.0f, 2.0f, 5.0f), math::Vec3(0.0f, 0.0f, 0.0f),
                          math::Vec3(0.0f, 1.0f, 0.0f));
        assert(math::approxEqual(cam.pitch(), math::degrees(std::asin(2.0f / d)), 1.0f));
        assert(math::approxEqual(cam.yaw(), 0.0f, 1.0f));

        cam.orbit(100.0f, 100.0f);
        assert(cam.yaw() <= Camera::kMaxYawDeg + 0.01f);
        assert(cam.yaw() >= Camera::kMinYawDeg - 0.01f);
        assert(cam.pitch() <= Camera::kMaxPitchDeg + 0.01f);
        assert(cam.pitch() >= Camera::kMinPitchDeg - 0.01f);

        cam.orbit(-200.0f, -200.0f);
        assert(cam.yaw() <= Camera::kMaxYawDeg + 0.01f);
        assert(cam.yaw() >= Camera::kMinYawDeg - 0.01f);
        assert(cam.pitch() <= Camera::kMaxPitchDeg + 0.01f);
        assert(cam.pitch() >= Camera::kMinPitchDeg - 0.01f);
    }

    // --- Orbit round-trip ---
    {
        Camera cam;
        cam.setViewportSize(800, 600);
        cam.setPerspective(math::radians(60.0f), cam.aspect(), 0.1f, 100.0f);
        cam.setViewLookAt(math::Vec3(0.0f, 2.0f, 5.0f), math::Vec3(0.0f, 0.0f, 0.0f),
                          math::Vec3(0.0f, 1.0f, 0.0f));
        math::Mat4 view0 = cam.view();
        float pitch0 = cam.pitch();
        float yaw0 = cam.yaw();

        cam.orbit(0.0f, 0.0f);
        for (int i = 0; i < 16; ++i) {
            assert(math::approxEqual(cam.view().m[i], view0.m[i], 1e-4f));
        }

        cam.orbit(30.0f, 10.0f);
        assert(math::approxEqual(cam.yaw(), yaw0 + 30.0f, 1.0f));
        assert(math::approxEqual(cam.pitch(), pitch0 + 10.0f, 1.0f));

        cam.orbit(-30.0f, -10.0f);
        assert(math::approxEqual(cam.pitch(), pitch0, 2.0f));
        assert(math::approxEqual(cam.yaw(), yaw0, 2.0f));
    }

    aout << "[camera] orbit self-test PASS" << std::endl;

    // --- setOrbitSnap ---
    {
        Camera cam;
        cam.setViewportSize(800, 600);
        cam.setPerspective(math::radians(60.0f), cam.aspect(), 0.1f, 100.0f);
        cam.setViewLookAt(math::Vec3(0.0f, 2.0f, 5.0f), math::Vec3(0.0f, 0.0f, 0.0f),
                          math::Vec3(0.0f, 1.0f, 0.0f));
        cam.setOrbitSnap(Camera::kIsoPitchDeg, Camera::kIsoYawDeg);
        assert(math::approxEqual(cam.pitch(), Camera::kIsoPitchDeg, 1e-4f));
        assert(math::approxEqual(cam.yaw(),   Camera::kIsoYawDeg,   1e-4f));
        cam.setOrbitSnap(15.0f, 0.0f);
        assert(math::approxEqual(cam.pitch(), 15.0f, 1e-4f));
        assert(math::approxEqual(cam.yaw(),    0.0f, 1e-4f));
        // out-of-range pitch/yaw gets clamped
        cam.setOrbitSnap(0.0f, 200.0f);
        assert(math::approxEqual(cam.pitch(), cam.minPitchDeg(), 1e-4f));
        assert(math::approxEqual(cam.yaw(),   cam.maxYawDeg(),   1e-4f));
    }

    // --- setPitchLimits / setYawLimits ---
    {
        Camera cam;
        cam.setViewportSize(800, 600);
        cam.setPerspective(math::radians(60.0f), cam.aspect(), 0.1f, 100.0f);
        cam.setViewLookAt(math::Vec3(0.0f, 2.0f, 5.0f), math::Vec3(0.0f, 0.0f, 0.0f),
                          math::Vec3(0.0f, 1.0f, 0.0f));
        cam.setPitchLimits(30.0f, 50.0f);
        assert(math::approxEqual(cam.minPitchDeg(), 30.0f, 1e-6f));
        assert(math::approxEqual(cam.maxPitchDeg(), 50.0f, 1e-6f));
        // current pitch was ~22 (eye at y=2, dist~5.4 → asin(2/5.4) ≈ 21.8°)
        // → clamped to 30 after limits shrink
        assert(cam.pitch() >= 30.0f - 1e-4f);
        cam.setYawLimits(-20.0f, 20.0f);
        assert(math::approxEqual(cam.minYawDeg(), -20.0f, 1e-6f));
        assert(math::approxEqual(cam.maxYawDeg(),  20.0f, 1e-6f));
        cam.orbit(100.0f, 0.0f);
        assert(cam.yaw() <= 20.0f + 1e-4f);
    }

    // --- setFovYRadians ---
    {
        Camera cam;
        cam.setViewportSize(800, 600);
        cam.setPerspective(math::radians(60.0f), cam.aspect(), 0.1f, 100.0f);
        assert(math::approxEqual(cam.fovYRadians(), math::radians(60.0f), 1e-6f));
        cam.setFovYRadians(math::radians(45.0f));
        assert(math::approxEqual(cam.fovYRadians(), math::radians(45.0f), 1e-6f));
        // aspect / near / far preserved
        math::Vec4 clip = cam.projection() * math::Vec4(0, 0, -0.1f, 1.0f);
        // near plane z stays ~-1 in NDC after perspective divide
        // (sanity: projection still produces finite, non-zero w)
        assert(clip.w > 0.0f);
    }

    aout << "[camera] setters self-test PASS" << std::endl;
}
