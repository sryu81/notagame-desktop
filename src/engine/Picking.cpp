#include "Picking.h"

#include "AndroidOut.h"
#include "Camera.h"
#include "Primitives.h"

#include <cassert>
#include <cmath>
#include <limits>

namespace picking {

HitShape fromMesh(const Mesh &mesh, const math::Mat4 &world) {
    HitShape s;
    const math::Vec4 c4 = world * math::Vec4(mesh.boundingSphereCenter(), 1.0f);
    s.center = math::Vec3(c4.x, c4.y, c4.z);
    // Uniform-scale assumption: length of the first rotation column
    // (or any non-translation column) is the scale factor. For the demo
    // scene all transforms are uniform scale, so this is correct.
    const float *m = world.m;
    const float sx = std::sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
    s.radius = mesh.boundingSphereRadius() * sx;
    return s;
}

Ray fromNdc(const math::Vec3 &ndc, const math::Mat4 &inverseViewProjection) {
    const math::Vec4 nearH = inverseViewProjection * math::Vec4(ndc.x, ndc.y, -1.0f, 1.0f);
    const math::Vec4 farH  = inverseViewProjection * math::Vec4(ndc.x, ndc.y,  1.0f, 1.0f);
    const math::Vec3 nearW = math::Vec3(nearH.x, nearH.y, nearH.z) / nearH.w;
    const math::Vec3 farW  = math::Vec3(farH.x,  farH.y,  farH.z)  / farH.w;
    Ray r;
    r.origin = nearW;
    r.direction = (farW - nearW).normalized();
    return r;
}

Ray fromScreen(float x, float y, int width, int height,
               const math::Mat4 &inverseViewProjection) {
    const float ndcX = (2.0f * x / static_cast<float>(width))  - 1.0f;
    const float ndcY = 1.0f - (2.0f * y / static_cast<float>(height));
    return fromNdc(math::Vec3(ndcX, ndcY, 0.0f), inverseViewProjection);
}

namespace {

bool raySphere(const Ray &ray, const HitShape &s, float &t) {
    const math::Vec3 oc = ray.origin - s.center;
    const float b = math::dot(oc, ray.direction);
    const float c = math::dot(oc, oc) - s.radius * s.radius;
    const float disc = b * b - c;
    if (disc < 0.0f) return false;
    const float sq = std::sqrt(disc);
    float t0 = -b - sq;
    if (t0 < 0.0f) t0 = -b + sq;
    if (t0 < 0.0f) return false;
    t = t0;
    return true;
}

} // namespace

Hit pickRay(const Ray &ray, const std::vector<HitShape> &shapes) {
    Hit best;
    best.distance = std::numeric_limits<float>::infinity();
    for (size_t i = 0; i < shapes.size(); i++) {
        float t = 0.0f;
        if (!raySphere(ray, shapes[i], t)) continue;
        if (t < best.distance) {
            best.valid = true;
            best.shapeIndex = static_cast<int>(i);
            best.distance = t;
        }
    }
    return best;
}

namespace {

constexpr float kEps = 1e-4f;

bool approxVec3(const math::Vec3 &a, const math::Vec3 &b, float eps = kEps) {
    return math::approxEqual(a.x, b.x, eps)
        && math::approxEqual(a.y, b.y, eps)
        && math::approxEqual(a.z, b.z, eps);
}

} // namespace

void selfTest() {
    aout << "[picking] self-test starting" << std::endl;

    // 1. Screen-center NDC ray from a camera at a valid-pitch eye
    //    looking at origin: the ray should point from eye toward target.
    {
        Camera cam;
        cam.setPerspective(math::radians(60.0f), 1.0f, 0.1f, 100.0f);
        cam.setViewLookAt(math::Vec3(0.0f, 1.5f, 5.0f),
                          math::Vec3(0.0f, 0.0f, 0.0f),
                          math::Vec3(0.0f, 1.0f, 0.0f));
        cam.setViewportSize(800, 600);

        const math::Mat4 invVp = cam.viewProjection().inverted();

        Ray center = picking::fromScreen(400.0f, 300.0f, 800, 600, invVp);
        // Direction should match normalized eye→target vector
        const math::Vec3 expectedDir = (math::Vec3(0.0f, 0.0f, 0.0f) - cam.eye()).normalized();
        assert(math::approxEqual(center.direction.x, expectedDir.x, kEps));
        assert(math::approxEqual(center.direction.y, expectedDir.y, kEps));
        assert(math::approxEqual(center.direction.z, expectedDir.z, kEps));
        // Origin at near plane along view direction
        const math::Vec3 expectedOrigin = cam.eye() + expectedDir * 0.1f;
        assert(approxVec3(center.origin, expectedOrigin, 5e-2f));
        // Ray passes through target at the correct distance (near→far t range)
        const float t = math::dot(math::Vec3(0.0f, 0.0f, 0.0f) - center.origin, center.direction);
        assert(t > 0.0f);
        assert(approxVec3(center.origin + center.direction * t,
                          math::Vec3(0.0f, 0.0f, 0.0f), 1e-1f));
    }

    // 2. Bounding sphere: cube of size 2 centered at origin, then translated
    //    to (-1.5, 0, 0) and scaled 0.8. World sphere center = (-1.5, 0, 0),
    //    radius = 0.8 * sqrt(3) ≈ 1.3856.
    {
        Mesh cubeMesh = primitives::makeCube(2.0f);
        const math::Vec3 localCenter = cubeMesh.boundingSphereCenter();
        const float localRadius = cubeMesh.boundingSphereRadius();
        assert(approxVec3(localCenter, math::Vec3(0.0f, 0.0f, 0.0f), 1e-4f));
        assert(math::approxEqual(localRadius, std::sqrt(3.0f), 1e-4f));

        const math::Mat4 world =
                math::Mat4::translation(math::Vec3(-1.5f, 0.0f, 0.0f))
                * math::Mat4::scale(math::Vec3(0.8f, 0.8f, 0.8f));
        HitShape shape = picking::fromMesh(cubeMesh, world);
        assert(approxVec3(shape.center, math::Vec3(-1.5f, 0.0f, 0.0f), 1e-4f));
        assert(math::approxEqual(shape.radius, 0.8f * std::sqrt(3.0f), 1e-4f));
    }

    // 3. Ray from (0, 0, 5) toward (-1.5, 0, 0) hits the cube (index 0),
    //    misses the sphere (index 1).
    {
        Ray ray;
        ray.origin = math::Vec3(0.0f, 0.0f, 5.0f);
        ray.direction = math::Vec3(-1.5f, 0.0f, -5.0f).normalized();

        Mesh cubeMesh = primitives::makeCube(2.0f);
        Mesh sphereMesh = primitives::makeUVSphere(1.0f, 16, 24);
        const math::Mat4 cubeWorld =
                math::Mat4::translation(math::Vec3(-1.5f, 0.0f, 0.0f))
                * math::Mat4::scale(math::Vec3(0.8f, 0.8f, 0.8f));
        const math::Mat4 sphereWorld =
                math::Mat4::translation(math::Vec3(1.5f, 0.0f, 0.0f))
                * math::Mat4::scale(math::Vec3(0.8f, 0.8f, 0.8f));

        std::vector<HitShape> shapes = {
            picking::fromMesh(cubeMesh, cubeWorld),
            picking::fromMesh(sphereMesh, sphereWorld),
        };
        Hit h = picking::pickRay(ray, shapes);
        assert(h.valid);
        assert(h.shapeIndex == 0);
        assert(h.distance > 0.0f);
    }

    // 4. Ray straight ahead (-Z) from (0, 0, 5) misses both shapes
    //    (gap between them is on the X axis at y=0).
    {
        Ray ray;
        ray.origin = math::Vec3(0.0f, 0.0f, 5.0f);
        ray.direction = math::Vec3(0.0f, 0.0f, -1.0f);

        Mesh cubeMesh = primitives::makeCube(2.0f);
        Mesh sphereMesh = primitives::makeUVSphere(1.0f, 16, 24);
        const math::Mat4 cubeWorld =
                math::Mat4::translation(math::Vec3(-1.5f, 0.0f, 0.0f))
                * math::Mat4::scale(math::Vec3(0.8f, 0.8f, 0.8f));
        const math::Mat4 sphereWorld =
                math::Mat4::translation(math::Vec3(1.5f, 0.0f, 0.0f))
                * math::Mat4::scale(math::Vec3(0.8f, 0.8f, 0.8f));

        std::vector<HitShape> shapes = {
            picking::fromMesh(cubeMesh, cubeWorld),
            picking::fromMesh(sphereMesh, sphereWorld),
        };
        Hit h = picking::pickRay(ray, shapes);
        assert(!h.valid);
    }

    // 5. pickRay returns the closest hit when multiple shapes overlap.
    {
        Ray ray;
        ray.origin = math::Vec3(0.0f, 0.0f, 5.0f);
        ray.direction = math::Vec3(0.0f, 0.0f, -1.0f);

        // Two concentric spheres at (0, 0, 0): inner radius 1, outer radius 3.
        // Ray hits both; outer should win because its center is further
        // along the ray (no — both centers are at origin; only the front
        // surfaces matter: outer front at z=2, inner front at z=0).
        // Wait: ray from (0,0,5) toward -Z, t=3 hits outer at z=2, t=5
        // hits inner at z=0. Outer is closer.
        HitShape inner;
        inner.center = math::Vec3(0.0f, 0.0f, 0.0f);
        inner.radius = 1.0f;
        HitShape outer;
        outer.center = math::Vec3(0.0f, 0.0f, 0.0f);
        outer.radius = 3.0f;
        std::vector<HitShape> shapes = {inner, outer};

        Hit h = picking::pickRay(ray, shapes);
        assert(h.valid);
        assert(h.shapeIndex == 1); // outer (closer hit)
        assert(math::approxEqual(h.distance, 2.0f, 1e-4f));
    }

    aout << "[picking] self-test PASS" << std::endl;
}

} // namespace picking
