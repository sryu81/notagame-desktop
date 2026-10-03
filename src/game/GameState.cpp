#include "GameState.h"

#include <algorithm>
#include <cassert>

#include "Engine.h"
#include "EntityRegistry.h"
#include "Unit.h"

namespace game {

void GameState::initDefaultRoster(size_t count) {
    roster_.clear();
    roster_.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        UnitSnapshot s;
        s.hp = s.maxHp;
        roster_.push_back(std::move(s));
    }
}

void GameState::captureFrom(EntityRegistry& reg) {
    roster_.clear();
    for (size_t i = 0; i < reg.size(); ++i) {
        auto* u = reg.unitAt(i);
        if (!u) continue;
        UnitSnapshot s;
        s.unitId = u->unitId();
        s.hp = u->hp();
        s.maxHp = u->maxHp();
        s.inventory = u->inventory();
        s.equipment = u->equipment();
        roster_.push_back(std::move(s));
    }
}

void GameState::restoreTo(EntityRegistry& reg) {
    for (size_t i = 0; i < roster_.size(); ++i) {
        const auto& s = roster_[i];
        Unit* u = nullptr;
        if (s.unitId != 0) {
            for (size_t k = 0; k < reg.size() && !u; ++k)
                if (reg.unitAt(k) && reg.unitAt(k)->unitId() == s.unitId) u = reg.unitAt(k);
        } else if (i < reg.size()) {
            u = reg.unitAt(i);
        }
        if (!u) continue;
        u->heal(u->maxHp());
        if (s.hp < s.maxHp) u->takeDamage(u->maxHp() - std::max(1, s.hp));
        u->inventory() = s.inventory;
        u->equipment() = s.equipment;
    }
}

void GameState::clear() {
    roster_.clear();
}

void GameState::save(engine::SaveWriter& w) const {
    w.writeU32(static_cast<uint32_t>(roster_.size()));
    for (const auto& s : roster_) {
        w.writeU32(s.unitId);
        w.writeU32(static_cast<uint32_t>(s.hp));
        w.writeU32(static_cast<uint32_t>(s.maxHp));
    }
}

void GameState::load(engine::SaveReader& r) {
    uint32_t n = 0;
    r.readU32(n);
    roster_.resize(n);
    for (auto& s : roster_) {
        uint32_t id = 0, hp = 0, maxHp = 0;
        r.readU32(id);
        s.unitId = static_cast<uint16_t>(id);
        r.readU32(hp);
        r.readU32(maxHp);
        s.hp = static_cast<int>(hp);
        s.maxHp = static_cast<int>(maxHp);
    }
}

void GameState::selfTest() {
    aout << "[game-state] self-test starting" << std::endl;

    {
        GameState gs;
        assert(gs.empty());
        assert(gs.rosterSize() == 0);
    }

    {
        GameState gs;
        gs.initDefaultRoster(3);
        assert(gs.rosterSize() == 3);
        assert(!gs.empty());
        for (size_t i = 0; i < 3; ++i) {
            assert(gs.entry(i).hp == gs.entry(i).maxHp);
            assert(gs.entry(i).inventory.empty());
        }
    }

    {
        GameState gs;
        gs.initDefaultRoster(2);
        gs.entry(0).hp = 5;
        gs.entry(1).hp = 8;

        engine::SaveWriter w;
        gs.save(w);
        assert(w.size() > 0);

        const auto& bytes = w.bytes();
        engine::SaveReader r(bytes.data(), bytes.size());
        GameState loaded;
        loaded.load(r);
        assert(!r.failed());
        assert(loaded.rosterSize() == 2);
        assert(loaded.entry(0).hp == 5);
        assert(loaded.entry(1).hp == 8);
    }

    {
        GameState gs;
        gs.initDefaultRoster(3);
        gs.entry(0).hp = 3;
        gs.entry(2).hp = 7;

        SceneNode root;
        EntityRegistry reg;
        reg.add(std::make_unique<Unit>(&root, unit::Faction::Player,
                                       unit::Stats{15, 6, 4, 3, 1}));
        reg.add(std::make_unique<Unit>(&root, unit::Faction::Enemy,
                                       unit::Stats{10, 5, 3, 2, 1}));
        reg.add(std::make_unique<Unit>(&root, unit::Faction::Ally,
                                       unit::Stats{12, 7, 3, 3, 1}));

        gs.restoreTo(reg);
        assert(reg.unitAt(0)->hp() == 3);
        assert(reg.unitAt(1)->hp() == 10);
        assert(reg.unitAt(2)->hp() == 7);

        reg.unitAt(0)->takeDamage(2);
        reg.unitAt(2)->heal(3);
        gs.captureFrom(reg);
        assert(gs.entry(0).hp == 1);
        assert(gs.entry(1).hp == 10);
        assert(gs.entry(2).hp == 10);
    }

    {
        GameState gs;
        gs.initDefaultRoster(1);

        SceneNode root;
        EntityRegistry reg;
        reg.add(std::make_unique<Unit>(&root, unit::Faction::Player,
                                       unit::Stats{20, 6, 4, 3, 1}));
        gs.restoreTo(reg);
        assert(reg.unitAt(0)->hp() == 20);
    }

    aout << "[game-state] self-test PASS" << std::endl;
}

} // namespace game
