#include "AI.h"

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdio>
#include <deque>

#include "BattleGrid.h"
#include "Engine.h"
#include "UnitTypes.h"

namespace ai {

uint32_t Rng::next() {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    return state_;
}

namespace {

bool hostile(const game::Unit& a, const game::Unit& b) {
    return &a != &b && !b.isDead() && !rules::allied(a.faction(), b.faction());
}

int steps(const grid::BattleGrid& g, const game::Unit& u, const math::Vec2& c, const rules::Tunables& t) {
    return static_cast<int>(rules::pathTo(g, u, c, t).size()) - 1;
}

bool containsCell(const std::vector<math::Vec2>& cells, const math::Vec2& c) {
    for (const auto& x : cells) if (x.x == c.x && x.y == c.y) return true;
    return false;
}

}

bool attackableFrom(const grid::BattleGrid& g, const game::Unit& u, const math::Vec2& cell,
                    const game::Unit& enemy, const rules::Tunables& t) {
    return rules::distance(cell, enemy.cell()) <= u.stats().range &&
           rules::lineOfSight(g, cell, enemy.cell(), t);
}

int nearestEnemyDistance(const game::Unit& u, const math::Vec2& cell,
                         const std::vector<game::Unit*>& units) {
    int best = INT_MAX;
    for (const game::Unit* e : units) {
        if (!e || !hostile(u, *e)) continue;
        const int d = static_cast<int>(std::abs(cell.x - e->cell().x) + std::abs(cell.y - e->cell().y));
        best = std::min(best, d);
    }
    return best;
}

Plan planTurn(const game::Unit& u, const grid::BattleGrid& g, const std::vector<game::Unit*>& units,
              const rules::Tunables& t, Rng& rng, const math::Vec2* goal) {
    Plan plan;
    if (u.isDead() || u.isStunned()) return plan;
    std::vector<game::Unit*> enemies;
    for (game::Unit* e : units)
        if (e && hostile(u, *e)) enemies.push_back(e);
    const bool seeksGoal = goal && rules::allied(u.faction(), unit::Faction::Player);
    if (enemies.empty() && !seeksGoal) return plan;

    for (game::Unit* e : enemies)
        if (attackableFrom(g, u, u.cell(), *e, t) && (!plan.target || e->hp() < plan.target->hp()))
            plan.target = e;
    if (plan.target) return plan;

    struct Option { math::Vec2 cell; game::Unit* target; int hp; int steps; };
    std::vector<Option> opts;
    const auto cells = rules::reachable(g, u, t);
    for (const auto& c : cells)
        for (game::Unit* e : enemies)
            if (attackableFrom(g, u, c, *e, t)) opts.push_back({c, e, e->hp(), steps(g, u, c, t)});
    if (!opts.empty()) {
        auto better = [](const Option& a, const Option& b) {
            return a.hp != b.hp ? a.hp < b.hp : a.steps < b.steps;
        };
        const Option best = *std::min_element(opts.begin(), opts.end(), better);
        std::vector<Option> ties;
        for (const auto& o : opts)
            if (o.hp == best.hp && o.steps == best.steps) ties.push_back(o);
        const Option& pick = ties[rng.below(static_cast<int>(ties.size()))];
        plan.path = rules::pathTo(g, u, pick.cell, t);
        plan.target = pick.target;
        return plan;
    }

    auto metric = [&](const math::Vec2& c) {
        if (seeksGoal) return static_cast<int>(std::abs(c.x - goal->x) + std::abs(c.y - goal->y));
        return nearestEnemyDistance(u, c, units);
    };
    const int d0 = metric(u.cell());
    int bestD = d0, bestSteps = INT_MAX;
    std::vector<math::Vec2> ties;
    for (const auto& c : cells) {
        const int d = metric(c);
        if (d > bestD || d >= d0) continue;
        const int s = steps(g, u, c, t);
        if (d < bestD || s < bestSteps) {
            bestD = d;
            bestSteps = s;
            ties.clear();
        }
        if (s == bestSteps) ties.push_back(c);
    }
    if (!ties.empty()) plan.path = rules::pathTo(g, u, ties[rng.below(static_cast<int>(ties.size()))], t);
    return plan;
}

std::vector<SimUnit> defaultRoster() {
    std::vector<SimUnit> out;
    for (const char* id : {"knight", "goblin", "archer", "goblin", "mage", "slinger"}) {
        const game::UnitType* t = game::findUnitType(id);
        out.push_back({t->name, rules::allied(t->faction, unit::Faction::Player) ? unit::Faction::Player
                                                                                 : unit::Faction::Enemy, t->stats});
    }
    return out;
}

Battle setupBattle(const SimConfig& cfg, const std::vector<SimUnit>& roster) {
    Battle b;
    Rng rng(cfg.seed * 2654435761u + 1u);
    const int n = cfg.size;
    for (int attempt = 0;; ++attempt) {
        b.grid = std::make_unique<grid::BattleGrid>(n, n, 1.0f);
        for (int i = 0; i < cfg.walls; ++i)
            b.grid->setPassable(1 + rng.below(n - 2), rng.below(n), false);
        for (int i = 0; i < cfg.hills; ++i) {
            const int x = 1 + rng.below(n - 2), y = rng.below(n);
            if (b.grid->at(x, y).passable) b.grid->setCellY(x, y, 0.5f);
        }
        std::vector<int> seen(static_cast<size_t>(n) * n, 0);
        std::deque<int> open{0};
        seen[0] = 1;
        int reached = 1, passable = 0;
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) passable += b.grid->at(x, y).passable ? 1 : 0;
        while (!open.empty()) {
            const int c = open.front();
            open.pop_front();
            const int cx = c % n, cy = c / n;
            const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
            for (int d = 0; d < 4; ++d) {
                const int nx = cx + dx[d], ny = cy + dy[d];
                if (!b.grid->inBounds(nx, ny) || seen[ny * n + nx] || !b.grid->at(nx, ny).passable) continue;
                seen[ny * n + nx] = 1;
                ++reached;
                open.push_back(ny * n + nx);
            }
        }
        if (reached == passable || attempt > 50) break;
    }
    int players = 0, enemies = 0;
    for (const auto& r : roster) {
        const bool player = rules::allied(r.faction, unit::Faction::Player);
        const int row = player ? 1 + 2 * players++ : 2 + 2 * enemies++;
        auto u = std::make_unique<game::Unit>(nullptr, r.faction, r.stats);
        u->setCell(math::Vec2(player ? 0.0f : static_cast<float>(n - 1), static_cast<float>(row % n)));
        u->setGrid(b.grid.get());
        b.units.push_back(std::move(u));
        b.types.push_back(r.type);
    }
    return b;
}

int winnerOf(const std::vector<game::Unit*>& units) {
    bool player = false, enemy = false;
    for (const game::Unit* u : units) {
        if (!u || u->isDead()) continue;
        if (rules::allied(u->faction(), unit::Faction::Player)) player = true;
        else enemy = true;
    }
    if (player && enemy) return -1;
    return player ? 0 : 1;
}

std::string turnLog(int round, int unit, const game::Unit& u, int target, int damage) {
    char buf[64];
    snprintf(buf, sizeof(buf), "r%d u%d %d,%d t%d d%d;", round, unit, static_cast<int>(u.cell().x),
             static_cast<int>(u.cell().y), target, damage);
    return buf;
}

SimResult simulate(const SimConfig& cfg, const std::vector<SimUnit>& roster) {
    SimResult res;
    Battle b = setupBattle(cfg, roster);
    std::vector<game::Unit*> ptrs;
    for (auto& u : b.units) ptrs.push_back(u.get());
    Rng rng(cfg.seed);
    const grid::BattleGrid& g = *b.grid;
    for (int round = 1; round <= cfg.maxRounds && res.winner < 0; ++round) {
        res.rounds = round;
        for (size_t i = 0; i < ptrs.size() && res.winner < 0; ++i) {
            game::Unit& u = *ptrs[i];
            if (u.isDead()) continue;
            TurnRecord rec;
            rec.unit = static_cast<int>(i);
            rec.round = round;
            for (const game::Unit* e : ptrs)
                if (hostile(u, *e) && attackableFrom(g, u, u.cell(), *e, cfg.tun)) rec.couldAttack = true;
            const auto reach = rules::reachable(g, u, cfg.tun);
            const int d0 = nearestEnemyDistance(u, u.cell(), ptrs);
            for (const auto& c : reach)
                if (nearestEnemyDistance(u, c, ptrs) < d0) rec.couldApproach = true;

            const Plan plan = planTurn(u, g, ptrs, cfg.tun, rng);
            if (!plan.path.empty()) {
                bool ok = plan.path.front().x == u.cell().x && plan.path.front().y == u.cell().y &&
                          containsCell(reach, plan.path.back()) &&
                          static_cast<int>(plan.path.size()) - 1 <= u.stats().mov;
                for (size_t k = 1; k < plan.path.size(); ++k)
                    ok = ok && std::abs(plan.path[k].x - plan.path[k - 1].x) +
                                   std::abs(plan.path[k].y - plan.path[k - 1].y) == 1.0f;
                rec.legalMove = ok;
                rec.moved = true;
                u.setCell(plan.path.back());
            }
            rec.approached = nearestEnemyDistance(u, u.cell(), ptrs) < d0;
            int dmg = 0;
            if (plan.target) {
                rec.attacked = true;
                rec.legalAttack = rules::canAttack(g, u, *plan.target, cfg.tun) == rules::AttackCheck::Ok;
                dmg = rules::damage(u.stats(), plan.target->stats(),
                                    rules::contextFor(g, u, *plan.target, rng.next()), cfg.tun);
                plan.target->takeDamage(dmg);
                res.damageByType[b.types[i]] += dmg;
            }
            int targetIdx = -1;
            for (size_t k = 0; k < ptrs.size(); ++k) if (ptrs[k] == plan.target) targetIdx = static_cast<int>(k);
            res.log += turnLog(round, static_cast<int>(i), u, targetIdx, dmg);
            res.turns.push_back(rec);
            res.winner = winnerOf(ptrs);
        }
    }
    return res;
}

void selfTest() {
    aout << "[ai] self-test starting" << std::endl;

    {
        Rng a(5), b(5), c(6);
        for (int i = 0; i < 10; ++i) assert(a.next() == b.next());
        assert(Rng(5).next() != c.next());
        Rng r(1);
        for (int i = 0; i < 100; ++i) { const int v = r.below(3); assert(v >= 0 && v < 3); }
    }

    {
        grid::BattleGrid g(8, 1, 1.0f);
        game::Unit a(nullptr, unit::Faction::Player, unit::Stats{10, 5, 2, 3, 1});
        game::Unit e(nullptr, unit::Faction::Enemy, unit::Stats{10, 5, 2, 3, 1});
        a.setCell(math::Vec2(0, 0));
        e.setCell(math::Vec2(7, 0));
        a.setGrid(&g);
        e.setGrid(&g);
        std::vector<game::Unit*> all{&a, &e};
        Rng rng(1);
        Plan p = planTurn(a, g, all, {}, rng);
        assert(p.target == nullptr && p.path.size() == 4 && p.path.back().x == 3.0f);
        e.setCell(math::Vec2(4, 0));
        p = planTurn(a, g, all, {}, rng);
        assert(p.target == &e && p.path.back().x == 3.0f);
        a.setCell(math::Vec2(3, 0));
        p = planTurn(a, g, all, {}, rng);
        assert(p.target == &e && p.path.empty());
        a.applyStatus({unit::StatusType::Stunned, 1, 0});
        p = planTurn(a, g, all, {}, rng);
        assert(!p.target && p.path.empty());
        a.clearStatuses();
        e.takeDamage(99);
        a.setCell(math::Vec2(0, 0));
        const math::Vec2 goal(6, 0);
        p = planTurn(a, g, all, {}, rng, &goal);
        assert(!p.target && p.path.back().x == 3.0f);
    }

    {
        SimConfig cfg;
        cfg.seed = 3;
        const SimResult r1 = simulate(cfg, defaultRoster());
        const SimResult r2 = simulate(cfg, defaultRoster());
        assert(r1.log == r2.log && !r1.log.empty());
        assert(r1.winner >= 0 && r1.rounds <= cfg.maxRounds);
        for (const auto& t : r1.turns) assert(t.legalMove && t.legalAttack);
    }

    aout << "[ai] self-test PASS" << std::endl;
}

} // namespace ai
