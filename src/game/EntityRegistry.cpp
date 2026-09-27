#include "EntityRegistry.h"

#include <cassert>

#include "Engine.h"
#include "Prop.h"
#include "Unit.h"

namespace game {

size_t EntityRegistry::add(std::unique_ptr<Object> entity, SceneNode* shadowNode) {
    if (!entity) return static_cast<size_t>(-1);
    size_t i = entities_.size();
    entities_.push_back(std::move(entity));
    if (shadowNode) {
        if (shadows_.size() <= i) shadows_.resize(i + 1, nullptr);
        shadows_[i] = shadowNode;
    }
    return i;
}

Unit* EntityRegistry::unitAt(size_t i) {
    if (i >= entities_.size()) return nullptr;
    return dynamic_cast<Unit*>(entities_[i].get());
}

Prop* EntityRegistry::propAt(size_t i) {
    if (i >= entities_.size()) return nullptr;
    return dynamic_cast<Prop*>(entities_[i].get());
}

SceneNode* EntityRegistry::shadowAt(size_t i) const {
    if (i >= shadows_.size()) return nullptr;
    return shadows_[i];
}

void EntityRegistry::setShadow(size_t i, SceneNode* sn) {
    if (i >= shadows_.size()) shadows_.resize(i + 1, nullptr);
    shadows_[i] = sn;
}

std::vector<Unit*> EntityRegistry::allUnits() {
    std::vector<Unit*> out;
    out.reserve(entities_.size());
    for (auto& e : entities_)
        if (auto* u = dynamic_cast<Unit*>(e.get())) out.push_back(u);
    return out;
}

size_t EntityRegistry::unitCount() const {
    size_t n = 0;
    for (auto& e : entities_)
        if (dynamic_cast<Unit*>(e.get())) ++n;
    return n;
}

void EntityRegistry::updateAll(float dt) {
    for (auto& e : entities_) {
        if (e) e->update(dt);
    }
}

void EntityRegistry::clear() {
    entities_.clear();
    shadows_.clear();
    clipPool_.clear();
}

void EntityRegistry::save(engine::SaveWriter& w) const {
    w.writeU32(static_cast<uint32_t>(entities_.size()));
    for (const auto& e : entities_) {
        if (e) {
            w.writeU32(1); // valid
            e->save(w);
        } else {
            w.writeU32(0); // null
        }
    }
}

void EntityRegistry::load(engine::SaveReader& r) {
    uint32_t n = 0;
    r.readU32(n);
    entities_.resize(n);
    shadows_.resize(n, nullptr);
    for (size_t i = 0; i < n; ++i) {
        uint32_t valid = 0;
        r.readU32(valid);
        if (valid) {
            // Need to know type — for now assume Unit (demand-driven)
            entities_[i] = std::make_unique<Unit>(nullptr, unit::Faction::Neutral,
                                                   unit::Stats{});
            entities_[i]->load(r);
        }
    }
}

void EntityRegistry::selfTest() {
    aout << "[entity-registry] self-test starting" << std::endl;

    {
        EntityRegistry reg;
        assert(reg.empty());
        assert(reg.size() == 0);
        assert(reg.unitCount() == 0);
    }

    {
        EntityRegistry reg;
        SceneNode root;

        auto u = std::make_unique<Unit>(&root, unit::Faction::Player,
                                        unit::Stats{15, 6, 4, 3, 1});
        auto* uptr = u.get();
        size_t idx = reg.add(std::move(u));
        assert(idx == 0);
        assert(reg.size() == 1);
        assert(!reg.empty());
        assert(reg.unitCount() == 1);
        assert(reg.unitAt(0) == uptr);
        assert(reg.at(0) == uptr);
        assert(reg.shadowAt(0) == nullptr);

        auto p = std::make_unique<Prop>(&root);
        auto* pptr = p.get();
        idx = reg.add(std::move(p));
        assert(idx == 1);
        assert(reg.size() == 2);
        assert(reg.unitCount() == 1);
        assert(reg.unitAt(0) == uptr);
        assert(reg.unitAt(1) == nullptr);
        assert(reg.propAt(1) == pptr);
    }

    {
        EntityRegistry reg;
        SceneNode root, shadow;

        reg.add(std::make_unique<Unit>(&root, unit::Faction::Enemy,
                                       unit::Stats{10, 5, 3, 2, 1}),
                &shadow);
        assert(reg.shadowAt(0) == &shadow);
    }

    {
        EntityRegistry reg;
        SceneNode root;

        reg.add(std::make_unique<Unit>(&root, unit::Faction::Player,
                                       unit::Stats{10, 5, 3, 3, 1}));
        reg.add(std::make_unique<Unit>(&root, unit::Faction::Enemy,
                                       unit::Stats{8, 4, 2, 2, 1}));

        int count = 0;
        reg.eachUnit([&](Unit&) { ++count; });
        assert(count == 2);

        auto all = reg.allUnits();
        assert(all.size() == 2);
    }

    {
        EntityRegistry reg;
        assert(reg.clipPool().empty());
        anim::Clip c;
        c.loop = true;
        reg.clipPool().push_back(c);
        assert(reg.clipPool().size() == 1);
    }

    {
        EntityRegistry reg;
        SceneNode root;
        reg.add(std::make_unique<Unit>(&root, unit::Faction::Ally,
                                       unit::Stats{12, 7, 3, 3, 1}));

        engine::SaveWriter w;
        reg.save(w);
        assert(w.size() > 0);

        const auto& bytes = w.bytes();
        engine::SaveReader r(bytes.data(), bytes.size());
        EntityRegistry loaded;
        loaded.load(r);
        assert(!r.failed());
        assert(loaded.size() == 1);
        assert(loaded.unitCount() == 1);
    }

    {
        EntityRegistry reg;
        reg.clear();
        assert(reg.empty());
        assert(reg.clipPool().empty());
    }

    aout << "[entity-registry] self-test PASS" << std::endl;
}

} // namespace game
