#include "Mesh.h"

#include <cassert>
#include <cmath>
#include <cstddef>

#include "AndroidOut.h"

math::Vec3 Mesh::boundingSphereCenter() const {
    if (positions.empty()) return math::Vec3(0.0f, 0.0f, 0.0f);
    math::Vec3 sum(0.0f, 0.0f, 0.0f);
    for (const auto &p : positions) sum = sum + p;
    return sum * (1.0f / static_cast<float>(positions.size()));
}

float Mesh::boundingSphereRadius() const {
    if (positions.empty()) return 0.0f;
    math::Vec3 c = boundingSphereCenter();
    float r2 = 0.0f;
    for (const auto &p : positions) {
        math::Vec3 d = p - c;
        float d2 = math::dot(d, d);
        if (d2 > r2) r2 = d2;
    }
    return std::sqrt(r2);
}

void Mesh::selfTest() {
    aout << "[mesh] self-test starting" << std::endl;

    Mesh quad;
    quad.positions = {
            math::Vec3(1.0f, 1.0f, 0.0f),
            math::Vec3(-1.0f, 1.0f, 0.0f),
            math::Vec3(-1.0f, -1.0f, 0.0f),
            math::Vec3(1.0f, -1.0f, 0.0f),
    };
    quad.uvs = {
            math::Vec2(0.0f, 0.0f),
            math::Vec2(1.0f, 0.0f),
            math::Vec2(1.0f, 1.0f),
            math::Vec2(0.0f, 1.0f),
    };
    quad.normals = {
            math::Vec3(0.0f, 0.0f, 1.0f),
            math::Vec3(0.0f, 0.0f, 1.0f),
            math::Vec3(0.0f, 0.0f, 1.0f),
            math::Vec3(0.0f, 0.0f, 1.0f),
    };
    quad.indices = {0, 1, 2, 0, 2, 3};

    assert(quad.positions.size() == quad.uvs.size());
    assert(quad.positions.size() == quad.normals.size());
    assert(quad.positions.size() == 4);
    assert(quad.indices.size() == 6);
    assert(quad.indices.size() % 3 == 0);

    constexpr size_t floatsPerVertex = 8;
    constexpr size_t bytesPerVertex = floatsPerVertex * sizeof(float);
    static_assert(bytesPerVertex == 32, "Vertex stride must be 32 bytes (3 pos + 2 uv + 3 normal floats)");

    size_t expectedVertexBytes = quad.positions.size() * bytesPerVertex;
    size_t expectedIndexBytes = quad.indices.size() * sizeof(uint32_t);
    assert(expectedVertexBytes == 128);
    assert(expectedIndexBytes == 24);

    aout << "[mesh] self-test PASS" << std::endl;
}
