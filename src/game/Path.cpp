#include "Path.h"

#include "BattleGrid.h"
#include "Engine.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>

namespace path {

namespace {

float heuristic(const math::Vec2 &a, const math::Vec2 &b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

constexpr float kCliffThreshold = 0.4f;

struct Node {
    int x;
    int y;
    float g;
    float f;
    bool operator>(const Node &other) const { return f > other.f; }
};

} // namespace

std::vector<math::Vec2> findPath(const grid::BattleGrid &grid, math::Vec2 start, math::Vec2 goal) {
    const int w = grid.width();
    const int h = grid.height();
    if (w <= 0 || h <= 0) return {};
    if (!grid.inBounds(static_cast<int>(start.x), static_cast<int>(start.y))) return {};
    if (!grid.inBounds(static_cast<int>(goal.x), static_cast<int>(goal.y))) return {};
    if (!grid.at(static_cast<int>(start.x), static_cast<int>(start.y)).passable) return {};
    if (!grid.at(static_cast<int>(goal.x), static_cast<int>(goal.y)).passable) return {};

    const int sx = static_cast<int>(start.x);
    const int sy = static_cast<int>(start.y);
    const int gx = static_cast<int>(goal.x);
    const int gy = static_cast<int>(goal.y);

    if (sx == gx && sy == gy) return {math::Vec2(sx, sy)};
    if (grid.at(gx, gy).blockingUnit != nullptr) return {};

    const size_t N = static_cast<size_t>(w) * static_cast<size_t>(h);
    auto idx = [w](int x, int y) { return static_cast<size_t>(y) * static_cast<size_t>(w)
                                       + static_cast<size_t>(x); };

    const float inf = std::numeric_limits<float>::infinity();
    std::vector<float> gScore(N, inf);
    std::vector<int32_t> parentX(N, -1);
    std::vector<int32_t> parentY(N, -1);
    std::vector<uint8_t> closed(N, 0);

    gScore[idx(sx, sy)] = 0.0f;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    open.push({sx, sy, 0.0f, heuristic(math::Vec2(sx, sy), math::Vec2(gx, gy))});

    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};

    while (!open.empty()) {
        const Node current = open.top();
        open.pop();

        const size_t ci = idx(current.x, current.y);
        if (closed[ci]) continue;
        closed[ci] = 1;

        if (current.x == gx && current.y == gy) {
            std::vector<math::Vec2> path;
            int cx = gx;
            int cy = gy;
            while (true) {
                path.push_back(math::Vec2(cx, cy));
                if (cx == sx && cy == sy) break;
                const size_t pi = idx(cx, cy);
                const int nx = parentX[pi];
                const int ny = parentY[pi];
                assert(nx >= 0 && ny >= 0);
                cx = nx;
                cy = ny;
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        for (int dir = 0; dir < 4; dir++) {
            const int nx = current.x + dx[dir];
            const int ny = current.y + dy[dir];
            if (!grid.inBounds(nx, ny)) continue;
            if (!grid.at(nx, ny).passable) continue;
            if (grid.at(nx, ny).blockingUnit != nullptr) continue;
            if (std::abs(grid.cellY(nx, ny) - grid.cellY(current.x, current.y)) > kCliffThreshold)
                continue;
            const size_t ni = idx(nx, ny);
            if (closed[ni]) continue;

            const float tentativeG = current.g + 1.0f;
            if (tentativeG < gScore[ni]) {
                gScore[ni] = tentativeG;
                parentX[ni] = current.x;
                parentY[ni] = current.y;
                const float h = heuristic(math::Vec2(nx, ny), math::Vec2(gx, gy));
                open.push({nx, ny, tentativeG, tentativeG + h});
            }
        }
    }

    return {};
}

void selfTest() {
    aout << "[path] self-test starting" << std::endl;

    {
        grid::BattleGrid g(6, 6, 1.0f);
        const auto p = findPath(g, math::Vec2(1, 1), math::Vec2(1, 1));
        assert(p.size() == 1);
        assert(p[0].x == 1 && p[0].y == 1);
    }

    {
        grid::BattleGrid g(6, 6, 1.0f);
        const auto p = findPath(g, math::Vec2(1, 1), math::Vec2(2, 1));
        assert(p.size() == 2);
        assert(p[0].x == 1 && p[0].y == 1);
        assert(p[1].x == 2 && p[1].y == 1);
    }

    {
        grid::BattleGrid g(6, 6, 1.0f);
        const auto p = findPath(g, math::Vec2(0, 0), math::Vec2(3, 2));
        assert(p.size() == 6);
        assert(p.front().x == 0 && p.front().y == 0);
        assert(p.back().x == 3 && p.back().y == 2);
    }

    {
        grid::BattleGrid g(6, 6, 1.0f);
        g.setPassable(2, 2, false);
        g.setPassable(3, 2, false);
        g.setPassable(2, 3, false);
        g.setPassable(3, 3, false);
        const auto p = findPath(g, math::Vec2(1, 3), math::Vec2(4, 3));
        assert(!p.empty());
        assert(p.front().x == 1 && p.front().y == 3);
        assert(p.back().x == 4 && p.back().y == 3);
        for (const auto &c : p) {
            assert(!(c.x >= 2 && c.x <= 3 && c.y >= 2 && c.y <= 3));
        }
    }

    {
        grid::BattleGrid g(3, 3, 1.0f);
        g.setPassable(0, 1, false);
        g.setPassable(2, 1, false);
        g.setPassable(1, 0, false);
        g.setPassable(1, 2, false);
        const auto p = findPath(g, math::Vec2(1, 1), math::Vec2(1, 1));
        assert(p.size() == 1);
        const auto p2 = findPath(g, math::Vec2(0, 0), math::Vec2(2, 2));
        assert(p2.empty());
    }

    {
        grid::BattleGrid g(4, 1, 1.0f);
        g.setCellY(0, 0, 0.0f);
        g.setCellY(1, 0, 0.0f);
        g.setCellY(2, 0, 0.5f);
        g.setCellY(3, 0, 0.0f);
        const auto p = findPath(g, math::Vec2(0, 0), math::Vec2(3, 0));
        assert(p.empty());
    }

    {
        grid::BattleGrid g(4, 1, 1.0f);
        g.setCellY(0, 0, 0.0f);
        g.setCellY(1, 0, 0.1f);
        g.setCellY(2, 0, 0.2f);
        g.setCellY(3, 0, 0.0f);
        const auto p = findPath(g, math::Vec2(0, 0), math::Vec2(3, 0));
        assert(p.size() == 4);
    }

    {
        grid::BattleGrid g(5, 5, 1.0f);
        auto* dummy = reinterpret_cast<game::Unit*>(static_cast<uintptr_t>(0x1));
        g.setCellUnit(2, 2, dummy);
        const auto p = findPath(g, math::Vec2(0, 0), math::Vec2(4, 4));
        assert(!p.empty());
        for (const auto& c : p) {
            assert(!(static_cast<int>(c.x) == 2 && static_cast<int>(c.y) == 2));
        }
    }

    {
        grid::BattleGrid g(5, 5, 1.0f);
        auto* dummy = reinterpret_cast<game::Unit*>(static_cast<uintptr_t>(0x1));
        g.setCellUnit(2, 2, dummy);
        const auto p = findPath(g, math::Vec2(2, 2), math::Vec2(2, 2));
        assert(p.size() == 1);
    }

    {
        grid::BattleGrid g(5, 5, 1.0f);
        auto* dummy = reinterpret_cast<game::Unit*>(static_cast<uintptr_t>(0x1));
        g.setCellUnit(2, 2, dummy);
        const auto p = findPath(g, math::Vec2(0, 0), math::Vec2(2, 2));
        assert(p.empty());
    }

    aout << "[path] self-test PASS" << std::endl;
}

} // namespace path
