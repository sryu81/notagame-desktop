#include "BattleData.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>

#include "GameState.h"
#include "ResultScene.h"
#include "Rules.h"
#include "Unit.h"
#include "UnitTypes.h"

namespace battle {

namespace {

constexpr float kPerCell = 0.3f;
constexpr float kResultDelay = 1.5f;
constexpr int   kMaxRounds = 30;

const char* kSpawned   = "SPAWNED FROM LEVEL";
const char* kReach     = "MOVES WITHIN REACH";
const char* kLegal     = "ATTACKS LEGAL";
const char* kStunSkips = "STUN SKIPS";
const char* kResult    = "RESULT REACHED";
const char* kRoster    = "ROSTER SYNCED BY ID";
const char* kSprites   = "SPRITE UNITS";

int indexOf(const game::Unit* u) {
    for (size_t i = 0; i < d().units.size(); ++i) if (d().units[i] == u) return static_cast<int>(i);
    return -1;
}

math::Vec3 unitPos(const game::Unit* u) {
    const math::Mat4 m = const_cast<game::Unit*>(u)->node()->world();
    return math::Vec3(m.m[12], m.m[13], m.m[14]);
}

bool stuns(const game::Unit* u) {
    const int i = indexOf(u);
    if (i < 0) return false;
    const game::UnitType* t = game::findUnitType(d().level.units[static_cast<size_t>(i)].catalogId);
    return t && t->stuns;
}

void enqueueMove(game::Unit* u, std::vector<math::Vec2> path) {
    if (path.size() < 2) return;
    const auto reach = rules::reachable(*d().grid, *u, d().tun);
    bool ok = static_cast<int>(path.size()) - 1 <= u->stats().mov;
    ok = ok && std::any_of(reach.begin(), reach.end(), [&](const math::Vec2& c) {
        return c.x == path.back().x && c.y == path.back().y;
    });
    ++d().tally.moves;
    if (!ok) ++d().tally.badMoves;
    d().actions->enqueue(std::make_unique<action::MoveAction>(u, d().grid.get(), std::move(path), kPerCell));
}

void enqueueAttack(game::Unit* a, game::Unit* t, Renderer* r) {
    ++d().tally.attacks;
    if (rules::canAttack(*d().grid, *a, *t, d().tun) != rules::AttackCheck::Ok) ++d().tally.badAttacks;
    const int dmg = rules::damage(a->stats(), t->stats(), rules::contextFor(*d().grid, *a, *t, d().rng->next()), d().tun);
    game::Unit* target = t;
    auto onHit = [target, dmg]() {
        const math::Vec3 p = unitPos(target);
        d().numbers.spawn(p + math::Vec3(0.0f, 1.6f, 0.0f), std::to_string(dmg),
                          math::Vec4(1.0f, 0.35f, 0.25f, 1.0f), 1.0f, 0.9f);
        d().particles.burst(p + math::Vec3(0.0f, 0.6f, 0.0f), 12, 2.0f, math::Vec4(1.0f, 0.6f, 0.1f, 1.0f), 0.25f, 0.6f);
    };
    if (a->stats().range >= 2) {
        auto shot = std::make_unique<action::RangedAttackAction>(
            a, t, dmg, d().grid.get(), d().projRoot,
            SceneNode::Drawable{d().assets->mesh("cube"), d().assets->model("cube"),
                                math::Vec3(1.0f, 0.9f, 0.3f), d().litMat},
            true, 8.0f);
        if (stuns(a)) shot->setStatus(unit::StatusEffect{unit::StatusType::Stunned, 1, 0});
        shot->setHitCallback(onHit);
        d().actions->enqueue(std::move(shot));
    } else {
        auto hit = std::make_unique<action::AttackAction>(a, t, dmg);
        hit->setHitCallback(onHit);
        d().actions->enqueue(std::move(hit));
    }
    if (r) {
        r->camera().frame({unitPos(a), unitPos(t)}, 0.25f);
        d().framing = true;
    }
}

void beginTurn(Renderer& r) {
    d().turnUnit = d().turns->currentUnit();
    d().turnState = {};
    d().framing = false;
    d().mode = Mode::Menu;
    d().aiPhase = 0;
    d().highlight.clear();
    d().overlay->clearChildren();
    game::Unit* u = d().turnUnit;
    if (!u) return;
    if (u->isStunned()) ++d().tally.stunnedActed;
    u->tickStatuses();
    d().selected = u;
    r.camera().setTarget(unitPos(u));
    r.camera().resetDistance();
    if (u->isDead()) d().flow->acted();
}

void stepAI(Renderer& r) {
    game::Unit* u = d().turnUnit;
    if (d().aiPhase == 0) {
        const math::Vec2 goal(d().level.win.a, d().level.win.b);
        const bool reach = d().level.win.type == level::WinType::ReachCell;
        d().aiPlan = ai::planTurn(*u, *d().grid, d().units, d().tun, *d().rng, reach ? &goal : nullptr);
        if (!d().aiPlan.path.empty()) enqueueMove(u, d().aiPlan.path);
        d().aiPhase = 1;
        return;
    }
    if (d().aiPhase == 1) {
        if (d().aiPlan.target && !d().aiPlan.target->isDead()) enqueueAttack(u, d().aiPlan.target, &r);
        d().flow->acted();
        d().aiPhase = 2;
    }
}

void finishChecks(game::GameState& gs) {
    d().resultChecked = true;
    d().tally.skips = d().turns->skippedTurns();
    auto& c = *d().checks;
    c.check(kSpawned, d().spawnedOk);
    c.check(kReach, d().tally.moves > 0 && d().tally.badMoves == 0);
    c.check(kLegal, d().tally.attacks > 0 && d().tally.badAttacks == 0);
    c.check(kStunSkips, d().tally.skips > 0 && d().tally.stunnedActed == 0);
    c.check(kResult, d().outcome != flow::Outcome::Ongoing);
    gs.captureFrom(*d().registry);
    bool synced = gs.rosterSize() == d().units.size();
    for (size_t i = 0; i < gs.rosterSize() && synced; ++i) {
        const auto& s = gs.entry(i);
        const game::Unit* u = nullptr;
        for (const game::Unit* x : d().units) if (x->unitId() == s.unitId) u = x;
        synced = s.unitId != 0 && u && u->hp() == s.hp;
    }
    c.check(kRoster, synced);
    constexpr int kActed = (1 << 1) | (1 << 2) | (1 << 3);
    const bool sprites = d().spritesOk && d().sprites.allBound(d().units) &&
                         (d().sprites.seenClips() & kActed) == kActed;
    c.check(kSprites, sprites);
    if (!sprites)
        aout << "[battle] sprites ok=" << d().spritesOk << " bound=" << d().sprites.boundCount()
             << " clips=" << d().sprites.seenClips() << std::endl;
    aout << "[battle] " << flow::toString(d().outcome) << " round " << d().turns->round() << " moves "
         << d().tally.moves << " attacks " << d().tally.attacks << " skips " << d().tally.skips << std::endl;
    aout << "[battle] auto-drive done: " << c.passedCount() << "/" << c.size() << " checks green" << std::endl;
}

}

void addChecklistRows(game::LabChecklist& c) {
    for (const char* row : {kSpawned, kReach, kLegal, kStunSkips, kResult, kRoster, kSprites}) c.add(row);
}

bool playerControls(const game::Unit* u) {
    return u && !d().autoBattle && rules::allied(u->faction(), unit::Faction::Player);
}

void showHighlight(const std::vector<math::Vec2>& cells, const math::Vec3& color) {
    d().highlight = cells;
    d().overlay->clearChildren();
    for (const auto& c : cells) {
        const math::Vec3 p = d().grid->cellToWorld(static_cast<int>(c.x), static_cast<int>(c.y));
        SceneNode* n = d().overlay->addChild();
        n->setLocal(math::Mat4::translation(math::Vec3(p.x, p.y + 0.1f, p.z)) *
                    math::Mat4::scale(math::Vec3(0.6f, 1.0f, 0.6f)));
        n->addDrawable({d().assets->mesh("plane"), d().assets->model("plane"), color, d().unlitMat});
    }
}

void onMenuAction(gui::Action a) {
    game::Unit* u = d().turnUnit;
    if (!playerControls(u) || !d().actions->isIdle() || d().mode == Mode::Busy) return;
    switch (a) {
        case gui::Action::Move:
            showHighlight(rules::reachable(*d().grid, *u, d().tun), math::Vec3(0.25f, 0.55f, 1.0f));
            d().mode = Mode::ChooseMove;
            break;
        case gui::Action::Attack: {
            std::vector<math::Vec2> cells;
            for (const game::Unit* e : d().units)
                if (rules::canAttack(*d().grid, *u, *e, d().tun) == rules::AttackCheck::Ok) cells.push_back(e->cell());
            showHighlight(cells, math::Vec3(1.0f, 0.3f, 0.25f));
            d().mode = Mode::ChooseTarget;
            break;
        }
        case gui::Action::Wait:
            showHighlight({}, math::Vec3(0));
            d().mode = Mode::Busy;
            d().flow->acted();
            break;
    }
}

void playerMove(const math::Vec2& cell) {
    game::Unit* u = d().turnUnit;
    enqueueMove(u, rules::pathTo(*d().grid, *u, cell, d().tun));
    d().turnState.moved = true;
    showHighlight({}, math::Vec3(0));
    d().mode = Mode::Menu;
}

void playerAttack(game::Unit* target, Renderer& r) {
    enqueueAttack(d().turnUnit, target, &r);
    d().turnState.acted = true;
    showHighlight({}, math::Vec3(0));
    d().mode = Mode::Busy;
    d().flow->acted();
}

void updateTurns(float dt, Renderer& r, game::GameState& gs) {
    if (!d().turns || d().units.empty()) return;
    if (d().outcome != flow::Outcome::Ongoing) {
        if (!d().actions->isIdle()) return;
        if (!d().resultChecked) finishChecks(gs);
        d().resultTimer += dt;
        if (d().resultTimer >= kResultDelay) {
            char summary[64];
            snprintf(summary, sizeof(summary), "%s  ROUND %d", levelName(d().levelId), d().turns->round());
            game::GameState* g = &gs;
            const flow::Outcome o = d().outcome;
            const std::string s = summary;
            r.sceneManager().replace(std::make_unique<ResultScene>(*g, o, s));
            d().resultTimer = -1e9f;
        }
        return;
    }
    if (d().flow->update()) d().turnUnit = nullptr;
    if (!d().actions->isIdle()) return;
    d().outcome = flow::evaluate(d().level.win, d().units, d().turns->round());
    if (d().outcome == flow::Outcome::Ongoing && d().turns->round() > kMaxRounds) d().outcome = flow::Outcome::Defeat;
    if (d().outcome != flow::Outcome::Ongoing) return;
    if (d().turnUnit != d().turns->currentUnit()) beginTurn(r);
    if (!d().turnUnit || d().flow->waiting()) return;
    if (!playerControls(d().turnUnit)) stepAI(r);
}

void saveGame() {
    if (d().savePath.empty() || !d().turns) return;
    uint32_t state = d().rng->state();
    flow::BattleSnapshot snap(d().units, d().turns.get(), &state);
    aout << (save::writeAll(d().savePath.c_str(), snap) ? "[save] saved " : "[save] FAILED ") << d().savePath
         << std::endl;
}

void loadGame() {
    if (d().savePath.empty() || !d().turns || !d().actions->isIdle()) return;
    uint32_t state = 0;
    flow::BattleSnapshot snap(d().units, d().turns.get(), &state);
    if (!save::loadAll(d().savePath.c_str(), snap) || !snap.loaded()) {
        aout << "[save] nothing to load at " << d().savePath << std::endl;
        return;
    }
    d().rng->setState(state);
    for (game::Unit* u : d().units) {
        const math::Vec3 p = d().grid->cellToWorld(static_cast<int>(u->cell().x), static_cast<int>(u->cell().y));
        u->node()->setLocal(math::Mat4::translation(p) * math::Mat4::scale(math::Vec3(u->baseScale())));
    }
    d().turnUnit = nullptr;
    d().flow = std::make_unique<flow::BattleFlow>(*d().turns, *d().actions);
    aout << "[save] loaded " << d().units.size() << " units" << std::endl;
}

} // namespace battle
