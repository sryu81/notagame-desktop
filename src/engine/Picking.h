#ifndef NOT_A_GAME_PICKING_H
#define NOT_A_GAME_PICKING_H

#include <vector>

#include "Math.h"
#include "Mesh.h"

namespace picking {

struct Ray {
    math::Vec3 origin;
    math::Vec3 direction; // assumed normalized
};

struct HitShape {
    math::Vec3 center;
    float radius = 0.0f;
};

struct Hit {
    int shapeIndex = -1;
    float distance = 0.0f;
    bool valid = false;
};

HitShape fromMesh(const Mesh &mesh, const math::Mat4 &world);

Ray fromNdc(const math::Vec3 &ndc, const math::Mat4 &inverseViewProjection);

Ray fromScreen(float x, float y, int width, int height,
               const math::Mat4 &inverseViewProjection);

Hit pickRay(const Ray &ray, const std::vector<HitShape> &shapes);

void selfTest();

} // namespace picking

#endif
