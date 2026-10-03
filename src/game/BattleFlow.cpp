#include "BattleFlow.h"

#include <cassert>

#include "Action.h"
#include "BattleGrid.h"
#include "Engine.h"
#include "Rules.h"
#include "Turn.h"
#include "Unit.h"

namespace flow {

namespace {

constexpr uint32_t kMagic   = 0x42534E31u;
constexpr uint32_t kVersion = 1u;

bool player(const game::Unit& u) { return rules::allied(u.faction(), unit::Faction::Player); }

}

Outcome evaluate(const level::WinCondition& win, const std::vector<game::Unit*>& units, int round) {
    bool players = false, enemies = false, reached = false;
    for (const game::Unit* u : units) {
        if (!u || u->isDead()) continue;
        if (player(*u)) {
            players = true;
            if (static_cast<int>(u->cell().x) == win.a && static_cast<int>(u->cell().y) == win.b) reached = true;
        } else {
            enemies = true;
        }
    }
    if (!players) return Outcome::Defeat;
    switch (win.type) {
        case level::WinType::DefeatAll:
            return enemies ? Outcome::Ongoing : Outcome::Victory;
        case level::WinType::SurviveRounds:
            return (!enemies || round > win.a) ? Outcome::Victory : Outcome::Ongoing;
        case level::WinType::ReachCell:
            return reached ? Outcome::Victory : Outcome::Ongoing;
    }
    return Outcome::Ongoing;
}

const char* toString(Outcome o) {
    switch (o) {
        case Outcome::Ongoing: return "ONGOING";
        case Outcome::Victory: return "VICTORY";
        case Outcome::Defeat:  return "DEFEAT";
    }
    return "?";
}

BattleFlow::BattleFlow(turn::Manager& turns, action::Manager& actions)
        : turns_(turns), actions_(actions) {
    turns_.setTimed(false);
}

bool BattleFlow::update() {
    if (!acted_ || !actions_.isIdle()) return false;
    acted_ = false;
    turns_.endTurn();
    return true;
}

BattleSnapshot::BattleSnapshot(std::vector<game::Unit*> units, turn::Manager* turns, uint32_t* rngState)
        : units_(std::move(units)), turns_(turns), rng_(rngState) {}

void BattleSnapshot::save(engine::SaveWriter& w) const {
    w.writeU32(kMagic);
    w.writeU32(kVersion);
    w.writeU32(static_cast<uint32_t>(units_.size()));
    for (const game::Unit* u : units_) u->save(w);
    w.writeU32(turns_ ? static_cast<uint32_t>(turns_->turnIndex()) : 0u);
    w.writeFloat(turns_ ? turns_->elapsed() : 0.0f);
    w.writeI32(turns_ ? turns_->round() : 1);
    w.writeU32(rng_ ? *rng_ : 0u);
}

void BattleSnapshot::load(engine::SaveReader& r) {
    loaded_ = false;
    uint32_t magic = 0, version = 0, n = 0;
    if (!r.readU32(magic) || !r.readU32(version) || magic != kMagic || version != kVersion) return;
    if (!r.readU32(n) || n != units_.size()) return;
    for (uint32_t i = 0; i < n; ++i) {
        game::Unit tmp(nullptr, unit::Faction::Neutral, unit::Stats{});
        tmp.load(r);
        game::Unit* target = units_[i];
        for (game::Unit* u : units_)
            if (tmp.unitId() != 0 && u->unitId() == tmp.unitId()) target = u;
        engine::SaveWriter copy;
        tmp.save(copy);
        engine::SaveReader again(copy.bytes().data(), copy.bytes().size());
        target->load(again);
    }
    uint32_t index = 0, state = 0;
    float elapsed = 0.0f;
    int32_t round = 1;
    r.readU32(index);
    r.readFloat(elapsed);
    r.readI32(round);
    r.readU32(state);
    if (r.failed()) return;
    if (turns_) {
        turns_->restore(static_cast<int>(index), elapsed);
        turns_->setRound(round);
    }
    if (rng_) *rng_ = state;
    loaded_ = true;
}

void selfTest() {
    aout << "[flow] self-test starting" << std::endl;

    game::Unit p(nullptr, unit::Faction::Player, unit::Stats{5, 3, 1, 3, 1});
    game::Unit e(nullptr, unit::Faction::Enemy, unit::Stats{5, 3, 1, 3, 1});
    p.setCell(math::Vec2(0, 0));
    e.setCell(math::Vec2(3, 0));
    std::vector<game::Unit*> all{&p, &e};
    const level::WinCondition defeatAll{level::WinType::DefeatAll, 0, 0};
    const level::WinCondition survive{level::WinType::SurviveRounds, 3, 0};
    const level::WinCondition reach{level::WinType::ReachCell, 2, 0};
    assert(evaluate(defeatAll, all, 1) == Outcome::Ongoing);
    assert(evaluate(survive, all, 3) == Outcome::Ongoing && evaluate(survive, all, 4) == Outcome::Victory);
    assert(evaluate(reach, all, 9) == Outcome::Ongoing);
    p.setCell(math::Vec2(2, 0));
    assert(evaluate(reach, all, 1) == Outcome::Victory);
    e.takeDamage(99);
    assert(evaluate(defeatAll, all, 1) == Outcome::Victory);
    p.takeDamage(99);
    assert(evaluate(defeatAll, all, 1) == Outcome::Defeat && evaluate(reach, all, 1) == Outcome::Defeat);

    {
        grid::BattleGrid g(4, 1, 1.0f);
        game::Unit a(nullptr, unit::Faction::Player, unit::Stats{10, 5, 3, 4, 1});
        game::Unit b(nullptr, unit::Faction::Enemy, unit::Stats{10, 5, 3, 4, 1});
        a.setUnitId(3);
        b.setUnitId(8);
        a.setCell(math::Vec2(0, 0));
        b.setCell(math::Vec2(3, 0));
        a.setGrid(&g);
        b.setGrid(&g);
        turn::Manager tm;
        tm.setUnits({&a, &b});
        tm.endTurn();
        tm.endTurn();
        uint32_t rng = 1234;
        b.takeDamage(4);
        a.setCell(math::Vec2(1, 0));
        BattleSnapshot snap({&a, &b}, &tm, &rng);
        engine::SaveWriter w;
        snap.save(w);

        grid::BattleGrid g2(4, 1, 1.0f);
        game::Unit a2(nullptr, unit::Faction::Player, unit::Stats{10, 5, 3, 4, 1});
        game::Unit b2(nullptr, unit::Faction::Enemy, unit::Stats{10, 5, 3, 4, 1});
        a2.setUnitId(3);
        b2.setUnitId(8);
        a2.setCell(math::Vec2(0, 0));
        a2.setGrid(&g2);
        b2.setCell(math::Vec2(3, 0));
        b2.setGrid(&g2);
        turn::Manager tm2;
        tm2.setUnits({&a2, &b2});
        uint32_t rng2 = 0;
        BattleSnapshot back({&b2, &a2}, &tm2, &rng2);
        engine::SaveReader r(w.bytes().data(), w.bytes().size());
        back.load(r);
        assert(back.loaded());
        assert(b2.hp() == 6 && a2.cell().x == 1.0f && tm2.round() == 2 && rng2 == 1234);
        assert(g2.at(1, 0).blockingUnit == &a2 && g2.at(0, 0).blockingUnit == nullptr);
    }

    aout << "[flow] self-test PASS" << std::endl;
}

} // namespace flow
