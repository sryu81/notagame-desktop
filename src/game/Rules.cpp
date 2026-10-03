#include "Rules.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <deque>

#include "BattleGrid.h"
#include "Engine.h"

namespace rules {

namespace {

struct Search {
    int w = 0;
    std::vector<int> dist;
    std::vector<int> parent;
};

int cellIndex(const grid::BattleGrid& g, int x, int y) { return y * g.width() + x; }

bool enemyAt(const grid::BattleGrid& g, const game::Unit& u, int x, int y) {
    const game::Unit* o = g.at(x, y).blockingUnit;
    return o != nullptr && o != &u && !o->isDead() && !allied(u.faction(), o->faction());
}

bool enterable(const grid::BattleGrid& g, const game::Unit& u, int fx, int fy, int x, int y,
               const Tunables& t) {
    if (!g.inBounds(x, y) || !g.at(x, y).passable || enemyAt(g, u, x, y)) return false;
    return std::abs(g.cellY(x, y) - g.cellY(fx, fy)) <= t.stepLimit + 1e-4f;
}

Search search(const grid::BattleGrid& g, const game::Unit& u, const Tunables& t) {
    Search s;
    s.w = g.width();
    const int n = g.width() * g.height();
    s.dist.assign(n, -1);
    s.parent.assign(n, -1);
    const int sx = static_cast<int>(u.cell().x);
    const int sy = static_cast<int>(u.cell().y);
    if (!g.inBounds(sx, sy)) return s;
    s.dist[cellIndex(g, sx, sy)] = 0;
    std::deque<int> open{cellIndex(g, sx, sy)};
    static const int kDx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int kDy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    const int dirs = t.diagonal ? 8 : 4;
    while (!open.empty()) {
        const int ci = open.front();
        open.pop_front();
        if (s.dist[ci] >= u.stats().mov) continue;
        const int cx = ci % s.w, cy = ci / s.w;
        for (int d = 0; d < dirs; ++d) {
            const int nx = cx + kDx[d], ny = cy + kDy[d];
            if (!enterable(g, u, cx, cy, nx, ny, t)) continue;
            if (d >= 4 && (!enterable(g, u, cx, cy, nx, cy, t) || !enterable(g, u, cx, cy, cx, ny, t)))
                continue;
            const int ni = cellIndex(g, nx, ny);
            if (s.dist[ni] >= 0) continue;
            s.dist[ni] = s.dist[ci] + 1;
            s.parent[ni] = ci;
            open.push_back(ni);
        }
    }
    return s;
}

bool stoppable(const grid::BattleGrid& g, const game::Unit& u, int x, int y) {
    const game::Unit* o = g.at(x, y).blockingUnit;
    return o == nullptr || o == &u || o->isDead();
}

}

bool allied(unit::Faction a, unit::Faction b) {
    if (a == b) return true;
    auto friendly = [](unit::Faction f) { return f == unit::Faction::Player || f == unit::Faction::Ally; };
    return friendly(a) && friendly(b);
}

int distance(const math::Vec2& a, const math::Vec2& b) {
    return std::max(static_cast<int>(std::abs(a.x - b.x)), static_cast<int>(std::abs(a.y - b.y)));
}

std::vector<math::Vec2> reachable(const grid::BattleGrid& g, const game::Unit& u, const Tunables& t) {
    const Search s = search(g, u, t);
    std::vector<math::Vec2> out;
    for (int i = 0; i < static_cast<int>(s.dist.size()); ++i) {
        const int x = i % s.w, y = i / s.w;
        if (s.dist[i] > 0 && stoppable(g, u, x, y))
            out.push_back(math::Vec2(static_cast<float>(x), static_cast<float>(y)));
    }
    return out;
}

std::vector<math::Vec2> pathTo(const grid::BattleGrid& g, const game::Unit& u,
                               const math::Vec2& goal, const Tunables& t) {
    const int gx = static_cast<int>(goal.x), gy = static_cast<int>(goal.y);
    if (!g.inBounds(gx, gy) || !stoppable(g, u, gx, gy)) return {};
    const Search s = search(g, u, t);
    int i = cellIndex(g, gx, gy);
    if (s.dist[i] <= 0) return {};
    std::vector<math::Vec2> path;
    for (; i >= 0; i = s.parent[i])
        path.push_back(math::Vec2(static_cast<float>(i % s.w), static_cast<float>(i / s.w)));
    std::reverse(path.begin(), path.end());
    return path;
}

bool lineOfSight(const grid::BattleGrid& g, const math::Vec2& a, const math::Vec2& b,
                 const Tunables& t) {
    int ax = static_cast<int>(a.x), ay = static_cast<int>(a.y);
    int bx = static_cast<int>(b.x), by = static_cast<int>(b.y);
    if (!g.inBounds(ax, ay) || !g.inBounds(bx, by)) return false;
    if (ax == bx && ay == by) return true;
    if (bx < ax || (bx == ax && by < ay)) {
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    const float eyeA = g.cellY(ax, ay) + t.eyeHeight;
    const float eyeB = g.cellY(bx, by) + t.eyeHeight;
    const int dx = bx - ax, dy = by - ay;
    const int n = 4 * std::max(std::abs(dx), std::abs(dy));
    for (int i = 0; i < n; ++i) {
        const float s = (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
        const int cx = static_cast<int>(std::floor(static_cast<float>(ax) + s * dx + 0.5f));
        const int cy = static_cast<int>(std::floor(static_cast<float>(ay) + s * dy + 0.5f));
        if ((cx == ax && cy == ay) || (cx == bx && cy == by)) continue;
        if (!g.at(cx, cy).passable) return false;
        if (g.cellY(cx, cy) > eyeA + (eyeB - eyeA) * s) return false;
    }
    return true;
}

int damage(const unit::Stats& attacker, const unit::Stats& target,
           const DamageContext& ctx, const Tunables& t) {
    int base = attacker.atk - target.def;
    if (t.elevationBonus && ctx.attackerY - ctx.targetY >= t.highGround - 1e-4f) base += t.elevationAtk;
    if (t.variance) base += static_cast<int>(ctx.roll % 3u) - 1;
    if (ctx.cover) base = static_cast<int>(std::floor(static_cast<float>(base) * (1.0f - t.coverPct) + 0.5f));
    return std::max(1, base);
}

DamageContext contextFor(const grid::BattleGrid& g, const game::Unit& attacker,
                         const game::Unit& target, uint32_t roll) {
    DamageContext c;
    const int ax = static_cast<int>(attacker.cell().x), ay = static_cast<int>(attacker.cell().y);
    const int tx = static_cast<int>(target.cell().x), ty = static_cast<int>(target.cell().y);
    if (g.inBounds(ax, ay)) c.attackerY = g.cellY(ax, ay);
    if (g.inBounds(tx, ty)) {
        c.targetY = g.cellY(tx, ty);
        c.cover = g.at(tx, ty).cover;
    }
    c.roll = roll;
    return c;
}

AttackCheck canAttack(const grid::BattleGrid& g, const game::Unit& attacker,
                      const game::Unit& target, const Tunables& t) {
    if (attacker.isDead() || target.isDead()) return AttackCheck::Dead;
    if (allied(attacker.faction(), target.faction())) return AttackCheck::Allied;
    if (attacker.isStunned()) return AttackCheck::Stunned;
    if (distance(attacker.cell(), target.cell()) > attacker.stats().range) return AttackCheck::OutOfRange;
    if (!lineOfSight(g, attacker.cell(), target.cell(), t)) return AttackCheck::NoLineOfSight;
    return AttackCheck::Ok;
}

const char* toString(AttackCheck c) {
    switch (c) {
        case AttackCheck::Ok:            return "OK";
        case AttackCheck::Dead:          return "DEAD";
        case AttackCheck::Allied:        return "ALLIED";
        case AttackCheck::Stunned:       return "STUNNED";
        case AttackCheck::OutOfRange:    return "OUT OF RANGE";
        case AttackCheck::NoLineOfSight: return "NO LOS";
    }
    return "?";
}

void selfTest() {
    aout << "[rules] self-test starting" << std::endl;

    assert(allied(unit::Faction::Player, unit::Faction::Ally));
    assert(!allied(unit::Faction::Player, unit::Faction::Enemy));
    assert(allied(unit::Faction::Enemy, unit::Faction::Enemy));

    {
        grid::BattleGrid g(7, 7, 1.0f);
        for (int x = 0; x < 3; ++x) g.setPassable(x, 1, false);
        for (int x = 1; x < 4; ++x) g.setPassable(x, 4, false);
        game::Unit u(nullptr, unit::Faction::Player, unit::Stats{10, 5, 3, 3, 1});
        u.setCell(math::Vec2(3, 3));
        assert(reachable(g, u).size() == 17);
        const auto p = pathTo(g, u, math::Vec2(0, 3));
        assert(p.size() == 4);
        assert(pathTo(g, u, math::Vec2(0, 0)).empty());
        Tunables diag;
        diag.diagonal = true;
        assert(reachable(g, u, diag).size() > 17);
    }

    {
        grid::BattleGrid g(5, 1, 1.0f);
        game::Unit u(nullptr, unit::Faction::Player, unit::Stats{10, 5, 3, 4, 1});
        game::Unit ally(nullptr, unit::Faction::Ally, unit::Stats{});
        game::Unit enemy(nullptr, unit::Faction::Enemy, unit::Stats{});
        u.setCell(math::Vec2(0, 0));
        ally.setCell(math::Vec2(1, 0));
        u.setGrid(&g);
        ally.setGrid(&g);
        auto cells = reachable(g, u);
        assert(cells.size() == 3);
        assert(pathTo(g, u, math::Vec2(1, 0)).empty());
        assert(pathTo(g, u, math::Vec2(2, 0)).size() == 3);
        enemy.setCell(math::Vec2(3, 0));
        enemy.setGrid(&g);
        assert(reachable(g, u).size() == 1);
        enemy.takeDamage(999);
        assert(reachable(g, u).size() == 3);
    }

    {
        grid::BattleGrid g(4, 1, 1.0f);
        g.setCellY(1, 0, 0.5f);
        g.setCellY(2, 0, 1.5f);
        game::Unit u(nullptr, unit::Faction::Player, unit::Stats{10, 5, 3, 4, 1});
        assert(reachable(g, u).size() == 1);
    }

    {
        grid::BattleGrid g(6, 6, 1.0f);
        assert(lineOfSight(g, math::Vec2(0, 0), math::Vec2(0, 0)));
        assert(lineOfSight(g, math::Vec2(0, 0), math::Vec2(5, 5)));
        assert(!lineOfSight(g, math::Vec2(-1, 0), math::Vec2(0, 0)));
        assert(!lineOfSight(g, math::Vec2(0, 0), math::Vec2(6, 0)));
        g.setPassable(2, 0, false);
        assert(!lineOfSight(g, math::Vec2(0, 0), math::Vec2(4, 0)));
        assert(!lineOfSight(g, math::Vec2(4, 0), math::Vec2(0, 0)));
        assert(lineOfSight(g, math::Vec2(0, 0), math::Vec2(0, 4)));
        g.setPassable(2, 0, true);
        g.setCellY(2, 0, 1.0f);
        assert(!lineOfSight(g, math::Vec2(0, 0), math::Vec2(4, 0)));
        g.setCellY(0, 0, 2.0f);
        assert(lineOfSight(g, math::Vec2(0, 0), math::Vec2(4, 0)));
    }

    {
        const unit::Stats a{10, 8, 2, 4, 1}, d{10, 5, 4, 3, 1};
        Tunables t;
        assert(damage(a, d) == 4);
        assert(damage(a, d, {1.0f, 0.0f, false, 0}) == 6);
        assert(damage(a, d, {0.0f, 0.0f, true, 0}) == 3);
        t.coverPct = 0.5f;
        assert(damage(a, d, {0.0f, 0.0f, true, 0}, t) == 2);
        t.variance = true;
        assert(damage(a, d, {0.0f, 0.0f, false, 0}, t) == 3);
        assert(damage(a, d, {0.0f, 0.0f, false, 2}, t) == 5);
        assert(damage({10, 1, 0, 1, 1}, d) == 1);
    }

    {
        grid::BattleGrid g(5, 5, 1.0f);
        game::Unit a(nullptr, unit::Faction::Player, unit::Stats{10, 5, 3, 4, 2});
        game::Unit b(nullptr, unit::Faction::Enemy, unit::Stats{10, 5, 3, 4, 1});
        a.setCell(math::Vec2(0, 0));
        b.setCell(math::Vec2(2, 0));
        assert(canAttack(g, a, b) == AttackCheck::Ok);
        b.setCell(math::Vec2(3, 0));
        assert(canAttack(g, a, b) == AttackCheck::OutOfRange);
        b.setCell(math::Vec2(2, 0));
        g.setPassable(1, 0, false);
        assert(canAttack(g, a, b) == AttackCheck::NoLineOfSight);
        a.applyStatus({unit::StatusType::Stunned, 1, 0});
        assert(canAttack(g, a, b) == AttackCheck::Stunned);
    }

    aout << "[rules] self-test PASS" << std::endl;
}

} // namespace rules
