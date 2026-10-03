#include "Turn.h"

#include <cassert>
#include <cstdio>

#include "Engine.h"
#include "Unit.h"

namespace turn {

Manager::Manager() {
    durations_[unit::Faction::Player]  = 10.0f;
    durations_[unit::Faction::Enemy]   = 3.0f;
    durations_[unit::Faction::Ally]    = 3.0f;
    durations_[unit::Faction::Neutral] = 3.0f;
    reset();
}

float Manager::currentDuration() const {
    const unit::Faction f = currentFaction();
    auto it = durations_.find(f);
    if (it == durations_.end()) return 3.0f;
    return it->second;
}

void Manager::addUnit(game::Unit *u) {
    if (u != nullptr) units_.push_back(u);
}

void Manager::setUnits(const std::vector<game::Unit *> &units) {
    units_ = units;
    if (currentIndex_ >= units_.size()) currentIndex_ = 0;
    elapsed_ = 0.0f;
}

static bool isDeadOrNull(game::Unit *u) {
    return u == nullptr || u->isDead();
}

void Manager::update(float dt) {
    if (units_.empty()) return;
    if (dt < 0.0f) dt = 0.0f;
    elapsed_ += dt;
    float dur = currentDuration();
    while (timed_ && elapsed_ >= dur) {
        elapsed_ -= dur;
        advance();
        dur = currentDuration();
    }
    while (isDeadOrNull(units_[currentIndex_]) && hasAliveUnit()) {
        currentIndex_ = (currentIndex_ + 1) % units_.size();
        if (currentIndex_ == 0) ++round_;
        elapsed_ = 0.0f;
    }
    if (units_[currentIndex_] != nullptr) {
        //aout << "[turn] turn " << currentIndex_ << " / " << units_.size()
        //     << " faction=" << static_cast<int>(units_[currentIndex_]->faction())
        //     << std::endl;
    }
}

bool Manager::hasAliveUnit() const {
    for (auto *u : units_) if (u != nullptr && !u->isDead()) return true;
    return false;
}

game::Unit *Manager::currentUnit() const {
    if (units_.empty()) return nullptr;
    return units_[currentIndex_];
}

unit::Faction Manager::currentFaction() const {
    const game::Unit *u = currentUnit();
    if (u == nullptr) return unit::Faction::Neutral;
    return u->faction();
}

bool Manager::isPlayerTurn() const {
    return currentFaction() == unit::Faction::Player;
}

void Manager::reset() {
    round_ = 1;
    currentIndex_ = 0;
    elapsed_ = 0.0f;
}

void Manager::restore(int index, float elapsed) {
    if (!units_.empty() && index >= 0 && static_cast<size_t>(index) < units_.size())
        currentIndex_ = static_cast<size_t>(index);
    elapsed_ = elapsed >= 0.0f ? elapsed : 0.0f;
}

void Manager::advance() {
    auto step = [this]() {
        currentIndex_ = (currentIndex_ + 1) % units_.size();
        if (currentIndex_ == 0) ++round_;
    };
    step();
    size_t stunSkips = 0;
    for (size_t guard = 0; guard < 2 * units_.size(); ++guard) {
        game::Unit *u = units_[currentIndex_];
        if (isDeadOrNull(u)) {
            if (!hasAliveUnit()) break;
            step();
            continue;
        }
        if (!u->isStunned() || stunSkips++ >= units_.size()) break;
        u->tickStatuses();
        ++skipped_;
        step();
    }
}

void Manager::endTurn() {
    if (units_.empty()) return;
    advance();
    elapsed_ = 0.0f;
    if (units_[currentIndex_] != nullptr) {
        //aout << "[turn] end turn → " << currentIndex_ << " / " << units_.size()
        //     << " faction=" << static_cast<int>(units_[currentIndex_]->faction())
        //     << std::endl;
    }
}

void Manager::selfTest() {
    aout << "[turn] self-test starting" << std::endl;

    {
        game::Unit a(nullptr, unit::Faction::Player, unit::Stats{10, 5, 3, 4});
        game::Unit b(nullptr, unit::Faction::Enemy, unit::Stats{10, 5, 3, 4});
        game::Unit c(nullptr, unit::Faction::Enemy, unit::Stats{10, 5, 3, 4});
        Manager m;
        m.setUnits({&a, &b, &c});
        b.applyStatus({unit::StatusType::Stunned, 1, 0});
        m.endTurn();
        assert(m.currentUnit() == &c && m.skippedTurns() == 1 && !b.isStunned());
        m.endTurn();
        m.endTurn();
        assert(m.currentUnit() == &b && m.skippedTurns() == 1);
        c.applyStatus({unit::StatusType::Stunned, 2, 0});
        m.endTurn();
        assert(m.currentUnit() == &a && m.skippedTurns() == 2);
        m.endTurn();
        m.endTurn();
        assert(m.currentUnit() == &a && m.skippedTurns() == 3 && !c.isStunned());
        a.applyStatus({unit::StatusType::Stunned, 5, 0});
        b.applyStatus({unit::StatusType::Stunned, 5, 0});
        c.applyStatus({unit::StatusType::Stunned, 5, 0});
        m.endTurn();
        assert(m.skippedTurns() == 6);
    }

    {
        game::Unit a(nullptr, unit::Faction::Player, unit::Stats{10, 5, 3, 4});
        game::Unit b(nullptr, unit::Faction::Enemy, unit::Stats{10, 5, 3, 4});
        Manager m;
        m.setUnits({&a, &b});
        m.setTimed(false);
        m.update(100.0f);
        assert(m.currentUnit() == &a && m.round() == 1);
        m.endTurn();
        assert(m.currentUnit() == &b && m.round() == 1);
        m.endTurn();
        assert(m.currentUnit() == &a && m.round() == 2);
    }

    {
        Manager m;
        assert(m.empty());
        assert(m.currentUnit() == nullptr);
        assert(m.currentFaction() == unit::Faction::Neutral);
        assert(!m.isPlayerTurn());
        m.update(100.0f);
        assert(m.empty());
        assert(m.currentUnit() == nullptr);
    }

    {
        unit::Stats ps{10, 5, 3, 4};
        unit::Stats es{10, 5, 3, 4};
        SceneNode pnode;
        SceneNode enode;
        game::Unit player(&pnode, unit::Faction::Player, ps);
        game::Unit enemy(&enode, unit::Faction::Enemy, es);
        player.setCell(math::Vec2(0.0f, 0.0f));
        enemy.setCell(math::Vec2(1.0f, 0.0f));
        Manager m;
        m.addUnit(&player);
        m.addUnit(&enemy);
        assert(m.unitCount() == 2);
        assert(m.currentUnit() == &player);
        assert(m.currentFaction() == unit::Faction::Player);
        assert(m.isPlayerTurn());
        assert(m.turnIndex() == 0);
        assert(m.currentDuration() == 10.0f);

        m.update(3.0f);
        assert(m.turnIndex() == 0);
        assert(math::approxEqual(m.elapsed(), 3.0f));

        m.update(7.0f);
        assert(m.turnIndex() == 1);
        assert(m.currentUnit() == &enemy);
        assert(m.currentFaction() == unit::Faction::Enemy);
        assert(!m.isPlayerTurn());
        assert(math::approxEqual(m.currentDuration(), 3.0f));
        assert(math::approxEqual(m.elapsed(), 0.0f));

        m.update(1.5f);
        assert(m.turnIndex() == 1);
        assert(math::approxEqual(m.elapsed(), 1.5f));

        m.update(1.5f);
        assert(m.turnIndex() == 0);
        assert(m.currentUnit() == &player);
        assert(m.isPlayerTurn());
        assert(math::approxEqual(m.elapsed(), 0.0f));
    }

    {
        unit::Stats es{10, 5, 3, 4};
        SceneNode enode;
        game::Unit enemy(&enode, unit::Faction::Enemy, es);
        enemy.setCell(math::Vec2(0.0f, 0.0f));
        Manager m;
        m.addUnit(&enemy);
        assert(m.unitCount() == 1);
        assert(m.currentUnit() == &enemy);
        assert(m.currentFaction() == unit::Faction::Enemy);
        assert(!m.isPlayerTurn());

        m.update(99.0f);
        assert(m.turnIndex() == 0);
        assert(m.currentUnit() == &enemy);
        assert(math::approxEqual(m.elapsed(), 0.0f));
    }

    {
        Manager m;
        m.reset();
        assert(m.empty());
    }

    {
        unit::Stats ps{10, 5, 3, 4}, es{8, 4, 2, 3}, as{8, 4, 2, 3};
        SceneNode pn, en, an;
        game::Unit player(&pn, unit::Faction::Player, ps);
        game::Unit enemy (&en, unit::Faction::Enemy,  es);
        game::Unit ally  (&an, unit::Faction::Ally,   as);
        player.setCell(math::Vec2(0, 0));
        enemy.setCell(math::Vec2(1, 0));
        ally.setCell(math::Vec2(2, 0));
        Manager m;
        m.setUnits({&player, &enemy, &ally});
        assert(m.unitCount() == 3);
        assert(m.currentFaction() == unit::Faction::Player);
        assert(math::approxEqual(m.currentDuration(), 10.0f));

        m.update(10.0f);
        assert(m.currentFaction() == unit::Faction::Enemy);
        assert(math::approxEqual(m.currentDuration(), 3.0f));

        m.update(3.0f);
        assert(m.currentFaction() == unit::Faction::Ally);
        assert(math::approxEqual(m.currentDuration(), 3.0f));

        m.update(3.0f);
        assert(m.currentFaction() == unit::Faction::Player);
    }

    {
        unit::Stats ps{10, 5, 3, 4}, es{8, 4, 2, 3}, as{8, 4, 2, 3};
        SceneNode pn, en, an;
        game::Unit player(&pn, unit::Faction::Player, ps);
        game::Unit enemy (&en, unit::Faction::Enemy,  es);
        game::Unit ally  (&an, unit::Faction::Ally,   as);
        player.setCell(math::Vec2(0, 0));
        enemy.setCell(math::Vec2(1, 0));
        ally.setCell(math::Vec2(2, 0));
        Manager m;
        m.setUnits({&player, &enemy, &ally});

        enemy.takeDamage(100);
        assert(m.hasAliveUnit());

        m.update(10.0f);
        assert(m.currentFaction() == unit::Faction::Ally);
        assert(m.currentUnit() == &ally);
    }

    aout << "[turn] self-test PASS" << std::endl;
}

} // namespace turn
