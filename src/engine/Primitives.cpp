#include "Primitives.h"

#include <cassert>
#include <cmath>

#include "AndroidOut.h"
#include "Math.h"

namespace primitives {

Mesh makePlane(float size) {
    const float h = size * 0.5f;

    Mesh m;
    m.positions = {
            math::Vec3(-h, 0.0f, -h),
            math::Vec3( h, 0.0f, -h),
            math::Vec3( h, 0.0f,  h),
            math::Vec3(-h, 0.0f,  h),
    };
    m.uvs = {
            math::Vec2(0.0f, 1.0f),
            math::Vec2(1.0f, 1.0f),
            math::Vec2(1.0f, 0.0f),
            math::Vec2(0.0f, 0.0f),
    };
    m.normals = {
            math::Vec3(0.0f, 1.0f, 0.0f),
            math::Vec3(0.0f, 1.0f, 0.0f),
            math::Vec3(0.0f, 1.0f, 0.0f),
            math::Vec3(0.0f, 1.0f, 0.0f),
    };
    m.indices = {0, 1, 2, 0, 2, 3};
    return m;
}

Mesh makeCube(float size) {
    const float h = size * 0.5f;

    Mesh m;
    m.positions = {
            math::Vec3( h, -h, -h), math::Vec3( h,  h, -h), math::Vec3( h,  h,  h), math::Vec3( h, -h,  h),
            math::Vec3(-h, -h,  h), math::Vec3(-h,  h,  h), math::Vec3(-h,  h, -h), math::Vec3(-h, -h, -h),
            math::Vec3(-h,  h, -h), math::Vec3(-h,  h,  h), math::Vec3( h,  h,  h), math::Vec3( h,  h, -h),
            math::Vec3(-h, -h,  h), math::Vec3(-h, -h, -h), math::Vec3( h, -h, -h), math::Vec3( h, -h,  h),
            math::Vec3( h, -h,  h), math::Vec3( h,  h,  h), math::Vec3(-h,  h,  h), math::Vec3(-h, -h,  h),
            math::Vec3(-h, -h, -h), math::Vec3(-h,  h, -h), math::Vec3( h,  h, -h), math::Vec3( h, -h, -h),
    };
    m.uvs = {
            math::Vec2(0.0f, 1.0f), math::Vec2(1.0f, 1.0f), math::Vec2(1.0f, 0.0f), math::Vec2(0.0f, 0.0f),
            math::Vec2(0.0f, 1.0f), math::Vec2(1.0f, 1.0f), math::Vec2(1.0f, 0.0f), math::Vec2(0.0f, 0.0f),
            math::Vec2(0.0f, 1.0f), math::Vec2(1.0f, 1.0f), math::Vec2(1.0f, 0.0f), math::Vec2(0.0f, 0.0f),
            math::Vec2(0.0f, 1.0f), math::Vec2(1.0f, 1.0f), math::Vec2(1.0f, 0.0f), math::Vec2(0.0f, 0.0f),
            math::Vec2(0.0f, 1.0f), math::Vec2(1.0f, 1.0f), math::Vec2(1.0f, 0.0f), math::Vec2(0.0f, 0.0f),
            math::Vec2(0.0f, 1.0f), math::Vec2(1.0f, 1.0f), math::Vec2(1.0f, 0.0f), math::Vec2(0.0f, 0.0f),
    };
    m.normals = {
            math::Vec3( 1.0f, 0.0f, 0.0f), math::Vec3( 1.0f, 0.0f, 0.0f), math::Vec3( 1.0f, 0.0f, 0.0f), math::Vec3( 1.0f, 0.0f, 0.0f),
            math::Vec3(-1.0f, 0.0f, 0.0f), math::Vec3(-1.0f, 0.0f, 0.0f), math::Vec3(-1.0f, 0.0f, 0.0f), math::Vec3(-1.0f, 0.0f, 0.0f),
            math::Vec3(0.0f,  1.0f, 0.0f), math::Vec3(0.0f,  1.0f, 0.0f), math::Vec3(0.0f,  1.0f, 0.0f), math::Vec3(0.0f,  1.0f, 0.0f),
            math::Vec3(0.0f, -1.0f, 0.0f), math::Vec3(0.0f, -1.0f, 0.0f), math::Vec3(0.0f, -1.0f, 0.0f), math::Vec3(0.0f, -1.0f, 0.0f),
            math::Vec3(0.0f, 0.0f,  1.0f), math::Vec3(0.0f, 0.0f,  1.0f), math::Vec3(0.0f, 0.0f,  1.0f), math::Vec3(0.0f, 0.0f,  1.0f),
            math::Vec3(0.0f, 0.0f, -1.0f), math::Vec3(0.0f, 0.0f, -1.0f), math::Vec3(0.0f, 0.0f, -1.0f), math::Vec3(0.0f, 0.0f, -1.0f),
    };
    m.indices = {
             0,  1,  2,  0,  2,  3,
             4,  5,  6,  4,  6,  7,
             8,  9, 10,  8, 10, 11,
            12, 13, 14, 12, 14, 15,
            16, 17, 18, 16, 18, 19,
            20, 21, 22, 20, 22, 23,
    };
    return m;
}

Mesh makeUVSphere(float radius, int rings, int segments) {
    assert(rings >= 2);
    assert(segments >= 3);

    Mesh m;
    int vertCount = (rings + 1) * (segments + 1);
    m.positions.reserve(vertCount);
    m.uvs.reserve(vertCount);
    m.normals.reserve(vertCount);

    for (int i = 0; i <= rings; ++i) {
        float v = static_cast<float>(i) / static_cast<float>(rings);
        float theta = v * math::PI;
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);
        for (int j = 0; j <= segments; ++j) {
            float u = static_cast<float>(j) / static_cast<float>(segments);
            float phi = u * math::TWO_PI;
            float x = sinTheta * std::cos(phi);
            float y = cosTheta;
            float z = sinTheta * std::sin(phi);
            m.positions.push_back(math::Vec3(x * radius, y * radius, z * radius));
            m.uvs.push_back(math::Vec2(u, 1.0f - v));
            m.normals.push_back(math::Vec3(x, y, z));
        }
    }

    int rowStride = segments + 1;
    m.indices.reserve(6 * rings * segments);
    for (int i = 0; i < rings; ++i) {
        for (int j = 0; j < segments; ++j) {
            uint32_t v00 = static_cast<uint32_t>(i * rowStride + j);
            uint32_t v01 = static_cast<uint32_t>(i * rowStride + j + 1);
            uint32_t v10 = static_cast<uint32_t>((i + 1) * rowStride + j);
            uint32_t v11 = static_cast<uint32_t>((i + 1) * rowStride + j + 1);
            m.indices.push_back(v00);
            m.indices.push_back(v11);
            m.indices.push_back(v10);
            m.indices.push_back(v00);
            m.indices.push_back(v01);
            m.indices.push_back(v11);
        }
    }
    return m;
}

void selfTest() {
    aout << "[primitives] self-test starting" << std::endl;

    Mesh plane = makePlane(2.0f);
    assert(plane.positions.size() == 4);
    assert(plane.uvs.size() == 4);
    assert(plane.normals.size() == 4);
    assert(plane.indices.size() == 6);

    Mesh cube = makeCube(2.0f);
    assert(cube.positions.size() == 24);
    assert(cube.uvs.size() == 24);
    assert(cube.normals.size() == 24);
    assert(cube.indices.size() == 36);
    for (size_t i = 0; i < cube.normals.size(); ++i) {
        assert(math::approxEqual(cube.normals[i].lengthSq(), 1.0f, 1e-5f));
    }
    for (size_t face = 0; face < 6; ++face) {
        math::Vec3 first = cube.normals[face * 4];
        for (size_t k = 1; k < 4; ++k) {
            assert(math::approxEqual(cube.normals[face * 4 + k], first, 1e-5f));
        }
    }

    constexpr int rings = 16;
    constexpr int segments = 16;
    Mesh sphere = makeUVSphere(1.0f, rings, segments);
    int expectedSphereVerts = (rings + 1) * (segments + 1);
    int expectedSphereIndices = 6 * rings * segments;
    assert(static_cast<int>(sphere.positions.size()) == expectedSphereVerts);
    assert(static_cast<int>(sphere.uvs.size()) == expectedSphereVerts);
    assert(static_cast<int>(sphere.normals.size()) == expectedSphereVerts);
    assert(static_cast<int>(sphere.indices.size()) == expectedSphereIndices);
    assert(sphere.positions.size() == 289);
    assert(sphere.indices.size() == 1536);
    for (size_t i = 0; i < sphere.normals.size(); ++i) {
        assert(math::approxEqual(sphere.normals[i].lengthSq(), 1.0f, 1e-5f));
    }

    math::Vec3 northPole = sphere.positions[0];
    assert(math::approxEqual(northPole, math::Vec3(0.0f, 1.0f, 0.0f), 1e-5f));
    math::Vec3 southPole = sphere.positions[(rings) * (segments + 1)];
    assert(math::approxEqual(southPole, math::Vec3(0.0f, -1.0f, 0.0f), 1e-5f));
    assert(math::approxEqual(sphere.normals[0], math::Vec3(0.0f, 1.0f, 0.0f), 1e-5f));
    assert(math::approxEqual(sphere.normals[rings * (segments + 1)], math::Vec3(0.0f, -1.0f, 0.0f), 1e-5f));

    {
        Mesh hm = makeHeightmap(4, 3, 1.0f, [](int, int) { return 0.0f; });
        assert(hm.positions.size() == 5u * 4u);
        assert(hm.indices.size()  == 4u * 3u * 6u);
        for (const auto& p : hm.positions) {
            assert(math::approxEqual(p.y, 0.0f));
        }
        for (const auto& n : hm.normals) {
            assert(n.y > 0.5f);
        }
    }

    {
        Mesh hm = makeHeightmap(2, 2, 1.0f, [](int, int) { return 0.0f; });
        const math::Vec3& corner00 = hm.positions[0];
        assert(math::approxEqual(corner00.x, -1.0f, 1e-5f));
        assert(math::approxEqual(corner00.z, -1.0f, 1e-5f));
        const math::Vec3& corner22 = hm.positions[8];
        assert(math::approxEqual(corner22.x, 1.0f, 1e-5f));
        assert(math::approxEqual(corner22.z, 1.0f, 1e-5f));
    }

    aout << "[primitives] self-test PASS" << std::endl;
}

Mesh makeHeightmap(int width, int height, float cellSize,
                   const std::function<float(int, int)>& yAt) {
    Mesh m;
    const int vw = width + 1;
    const int vh = height + 1;
    const float xOff = -static_cast<float>(width) * cellSize * 0.5f;
    const float zOff = -static_cast<float>(height) * cellSize * 0.5f;

    m.positions.reserve(static_cast<size_t>(vw) * static_cast<size_t>(vh));
    m.uvs.reserve(static_cast<size_t>(vw) * static_cast<size_t>(vh));
    m.normals.reserve(static_cast<size_t>(vw) * static_cast<size_t>(vh));
    for (int z = 0; z < vh; ++z) {
        for (int x = 0; x < vw; ++x) {
            m.positions.push_back(math::Vec3(
                    static_cast<float>(x) * cellSize + xOff,
                    yAt(x, z),
                    static_cast<float>(z) * cellSize + zOff));
            m.uvs.push_back(math::Vec2(
                    static_cast<float>(x) / static_cast<float>(width),
                    static_cast<float>(z) / static_cast<float>(height)));
            m.normals.push_back(math::Vec3(0.0f, 1.0f, 0.0f));
        }
    }

    m.indices.reserve(static_cast<size_t>(width) * static_cast<size_t>(height) * 6u);
    for (int z = 0; z < height; ++z) {
        for (int x = 0; x < width; ++x) {
            const uint32_t i00 = static_cast<uint32_t>(z)       * static_cast<uint32_t>(vw) + static_cast<uint32_t>(x);
            const uint32_t i10 = i00 + 1u;
            const uint32_t i01 = static_cast<uint32_t>(z + 1)   * static_cast<uint32_t>(vw) + static_cast<uint32_t>(x);
            const uint32_t i11 = i01 + 1u;
            m.indices.push_back(i00); m.indices.push_back(i10); m.indices.push_back(i11);
            m.indices.push_back(i00); m.indices.push_back(i11); m.indices.push_back(i01);
        }
    }

    for (size_t i = 0; i < m.positions.size(); ++i) {
        const int x = static_cast<int>(i % static_cast<size_t>(vw));
        const int z = static_cast<int>(i / static_cast<size_t>(vw));
        if (x == vw - 1 || z == vh - 1) continue;
        const size_t ix = static_cast<size_t>(x);
        const size_t iz = static_cast<size_t>(z);
        const math::Vec3& a = m.positions[iz * static_cast<size_t>(vw) + ix];
        const math::Vec3& b = m.positions[iz * static_cast<size_t>(vw) + ix + 1u];
        const math::Vec3& c = m.positions[(iz + 1u) * static_cast<size_t>(vw) + ix];
        const math::Vec3 ex = b - a;
        const math::Vec3 ez = c - a;
        math::Vec3 n = math::cross(ez, ex);
        n = n.normalized();
        m.normals[iz * static_cast<size_t>(vw) + ix] = n;
    }

    return m;
}

}
