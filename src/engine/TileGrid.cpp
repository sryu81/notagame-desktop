#include "TileGrid.h"

#include "AndroidOut.h"
#include "Mesh.h"

#include <cassert>
#include <cmath>

namespace engine {

TileGrid::TileGrid(int width, int height, float cellSize)
        : width_(width), height_(height), cellSize_(cellSize) {
    cellY_.assign(static_cast<size_t>(width_) * static_cast<size_t>(height_), 0.0f);
}

bool TileGrid::inBounds(int x, int y) const {
    return x >= 0 && x < width_ && y >= 0 && y < height_;
}

void TileGrid::setCellY(int x, int y, float yElev) {
    assert(inBounds(x, y));
    cellY_[static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)] = yElev;
}

void TileGrid::setCellY(const std::vector<float>& heightmap) {
    const size_t expected = static_cast<size_t>(width_) * static_cast<size_t>(height_);
    assert(heightmap.size() == expected);
    cellY_ = heightmap;
}

void TileGrid::applyElevationFromMesh(const Mesh& mesh) {
    const int meshW = width_ + 1;
    const int meshH = height_ + 1;
    const size_t expectedVerts = static_cast<size_t>(meshW) * static_cast<size_t>(meshH);
    assert(mesh.positions.size() == expectedVerts);
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const auto& v00 = mesh.positions[static_cast<size_t>(y)       * meshW + x];
            const auto& v10 = mesh.positions[static_cast<size_t>(y)       * meshW + (x + 1)];
            const auto& v01 = mesh.positions[static_cast<size_t>(y + 1)   * meshW + x];
            const auto& v11 = mesh.positions[static_cast<size_t>(y + 1)   * meshW + (x + 1)];
            cellY_[static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)] =
                    (v00.y + v10.y + v01.y + v11.y) * 0.25f;
        }
    }
}

float TileGrid::cellY(int x, int y) const {
    assert(inBounds(x, y));
    return cellY_[static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)];
}

math::Vec2 TileGrid::worldToCell(const math::Vec3& world) const {
    const float xOffset = -static_cast<float>(width_) * cellSize_ * 0.5f;
    const float zOffset = -static_cast<float>(height_) * cellSize_ * 0.5f;
    return math::Vec2(
            std::floor((world.x - xOffset) / cellSize_),
            std::floor((world.z - zOffset) / cellSize_));
}

math::Vec3 TileGrid::cellToWorld(int x, int y) const {
    const float xOffset = -static_cast<float>(width_) * cellSize_ * 0.5f;
    const float zOffset = -static_cast<float>(height_) * cellSize_ * 0.5f;
    return math::Vec3(
            static_cast<float>(x) * cellSize_ + xOffset + cellSize_ * 0.5f,
            cellY(x, y),
            static_cast<float>(y) * cellSize_ + zOffset + cellSize_ * 0.5f);
}

void TileGrid::selfTest() {
    aout << "[tile-grid] self-test starting" << std::endl;

    {
        TileGrid g(6, 6, 1.0f);
        assert(g.width() == 6);
        assert(g.height() == 6);
        assert(g.cellSize() == 1.0f);
        assert(g.inBounds(0, 0));
        assert(g.inBounds(5, 5));
        assert(!g.inBounds(6, 0));
        assert(!g.inBounds(0, 6));
        assert(!g.inBounds(-1, 0));
    }

    {
        TileGrid g(6, 6, 1.0f);
        const math::Vec3 w00 = g.cellToWorld(0, 0);
        assert(math::approxEqual(w00.x, -2.5f, 1e-4f));
        assert(math::approxEqual(w00.z, -2.5f, 1e-4f));
        const math::Vec3 w33 = g.cellToWorld(3, 3);
        assert(math::approxEqual(w33.x, 0.5f, 1e-4f));
        assert(math::approxEqual(w33.z, 0.5f, 1e-4f));
        const math::Vec3 w55 = g.cellToWorld(5, 5);
        assert(math::approxEqual(w55.x, 2.5f, 1e-4f));
        assert(math::approxEqual(w55.z, 2.5f, 1e-4f));
        const math::Vec2 c00 = g.worldToCell(math::Vec3(0.0f, 0.0f, 0.0f));
        assert(math::approxEqual(c00.x, 3.0f, 1e-4f));
        assert(math::approxEqual(c00.y, 3.0f, 1e-4f));
    }

    {
        TileGrid g(4, 4, 1.0f);
        assert(math::approxEqual(g.cellY(0, 0), 0.0f));
        g.setCellY(0, 0, 0.5f);
        g.setCellY(2, 1, -0.25f);
        assert(math::approxEqual(g.cellY(0, 0), 0.5f));
        assert(math::approxEqual(g.cellY(2, 1), -0.25f));
        assert(math::approxEqual(g.cellToWorld(0, 0).y, 0.5f));
        assert(math::approxEqual(g.cellToWorld(2, 1).y, -0.25f));
    }

    {
        TileGrid g(6, 6, 2.0f);
        const math::Vec3 w11 = g.cellToWorld(1, 1);
        assert(math::approxEqual(w11.x, -3.0f, 1e-4f));
        assert(math::approxEqual(w11.z, -3.0f, 1e-4f));
    }

    {
        TileGrid g(3, 2, 1.0f);
        std::vector<float> hm = {0.1f, 0.2f, 0.3f,
                                 0.4f, 0.5f, 0.6f};
        g.setCellY(hm);
        assert(math::approxEqual(g.cellY(0, 0), 0.1f));
        assert(math::approxEqual(g.cellY(2, 0), 0.3f));
        assert(math::approxEqual(g.cellY(0, 1), 0.4f));
        assert(math::approxEqual(g.cellY(2, 1), 0.6f));
        assert(math::approxEqual(g.cellToWorld(1, 1).y, 0.5f));
    }

    {
        TileGrid g(2, 2, 1.0f);
        Mesh m;
        m.positions = {
            {0.0f, 0.0f, 0.0f}, {1.0f, 0.5f, 0.0f}, {2.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {2.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 2.0f}, {1.0f, 0.0f, 2.0f}, {2.0f, 2.0f, 2.0f},
        };
        g.applyElevationFromMesh(m);
        assert(math::approxEqual(g.cellY(0, 0), (0.0f + 0.5f + 0.0f + 1.0f) * 0.25f, 1e-5f));
        assert(math::approxEqual(g.cellY(1, 0), (0.5f + 1.0f + 1.0f + 0.0f) * 0.25f, 1e-5f));
        assert(math::approxEqual(g.cellY(0, 1), (0.0f + 1.0f + 0.0f + 0.0f) * 0.25f, 1e-5f));
        assert(math::approxEqual(g.cellY(1, 1), (1.0f + 0.0f + 0.0f + 2.0f) * 0.25f, 1e-5f));
    }

    aout << "[tile-grid] self-test PASS" << std::endl;
}

} // namespace engine
