#include "Unit.h"

#include "BattleGrid.h"
#include "Engine.h"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace game {

Unit::Unit(SceneNode* node, unit::Faction faction, const unit::Stats& stats)
    : node_(node), faction_(faction), stats_(stats), hp_(stats.maxHp) {}

Unit::~Unit() {
    if (grid_) {
        const int x = static_cast<int>(cell_.x);
        const int y = static_cast<int>(cell_.y);
        if (grid_->inBounds(x, y))
            grid_->setCellUnit(x, y, nullptr);
    }
}

void Unit::setGrid(grid::BattleGrid* g) {
    grid_ = g;
    if (grid_) {
        const int x = static_cast<int>(cell_.x);
        const int y = static_cast<int>(cell_.y);
        if (grid_->inBounds(x, y))
            grid_->setCellUnit(x, y, this);
    }
}

void Unit::setCell(const math::Vec2& c) {
    if (grid_) {
        const int ox = static_cast<int>(cell_.x);
        const int oy = static_cast<int>(cell_.y);
        if (grid_->inBounds(ox, oy))
            grid_->setCellUnit(ox, oy, nullptr);
    }
    cell_ = c;
    if (grid_) {
        const int nx = static_cast<int>(cell_.x);
        const int ny = static_cast<int>(cell_.y);
        if (grid_->inBounds(nx, ny))
            grid_->setCellUnit(nx, ny, this);
    }
}

void Unit::takeDamage(int amount) {
    if (amount <= 0 || isDead()) return;
    hp_ -= amount;
    if (hp_ < 0) hp_ = 0;
    if (hp_ == 0) {
        selected_ = false;
        playClip(clips_.death);
    }
}

void Unit::heal(int amount) {
    if (amount <= 0 || isDead()) return;
    hp_ += amount;
    if (hp_ > stats_.maxHp) hp_ = stats_.maxHp;
}

void Unit::setSelected(bool sel) {
    selected_ = sel && !isDead();
}

bool Unit::isSkinned() const {
    const SceneNode* v = visualNode();
    return v != nullptr && v->skeleton() != nullptr;
}

void Unit::playClip(const anim::SkeletalClip* c) {
    if (c == nullptr || !isSkinned()) return;
    SceneNode* v = visualNode();
    v->setSkeletalClip(c);
    v->setTime(0.0f);
    v->setPlaybackRate(1.0f);
}

void Unit::applyStatus(unit::StatusEffect effect) {
    statuses_.push_back(effect);
}

void Unit::clearStatuses() {
    statuses_.clear();
}

void Unit::tickStatuses() {
    for (auto& s : statuses_) {
        if (s.type == unit::StatusType::Poisoned) {
            takeDamage(s.magnitude);
        }
        if (s.duration > 0) --s.duration;
    }
    statuses_.erase(
        std::remove_if(statuses_.begin(), statuses_.end(),
                       [](const unit::StatusEffect& s) { return s.duration <= 0; }),
        statuses_.end());
}

bool Unit::isStunned() const {
    return hasStatus(unit::StatusType::Stunned);
}

bool Unit::hasStatus(unit::StatusType t) const {
    for (const auto& s : statuses_) {
        if (s.type == t) return true;
    }
    return false;
}

void Unit::setBaseOffset(const math::Vec3& o) {
    baseOffset_ = o;
    if (node_) node_->setBaseOffset(o);
}

void Unit::update(float /*dt*/) {
    if (!isSkinned() || isDead() || clips_.hurt == nullptr) return;
    const SceneNode* v = visualNode();
    if (v->skeletalClip() == clips_.hurt && v->time() >= anim::duration(*clips_.hurt))
        playClip(clips_.idle);
}

void Unit::save(engine::SaveWriter& w) const {
    w.writeU32(unitId_);
    w.writeU32(static_cast<uint32_t>(faction_));
    w.writeI32(hp_);
    w.writeI32(stats_.maxHp);
    w.writeI32(stats_.atk);
    w.writeI32(stats_.def);
    w.writeI32(stats_.mov);
    w.writeI32(stats_.range);
    w.writeFloat(cell_.x);
    w.writeFloat(cell_.y);
    w.writeU32(static_cast<uint32_t>(inventory_.totalItems()));
    for (const auto& e : inventory_.entries()) {
        w.writeString(e.item.name);
        w.writeI32(e.item.value);
        w.writeI32(e.item.atk);
        w.writeI32(e.item.def);
        w.writeI32(e.item.heal);
    }
    for (uint32_t s = 0; s < static_cast<uint32_t>(EquipSlot::Count); ++s) {
        const auto slot = static_cast<EquipSlot>(s);
        const ItemDef* it = equipment_.get(slot);
        w.writeU32(it != nullptr ? 1u : 0u);
        if (it) {
            w.writeString(it->name);
            w.writeI32(it->value);
            w.writeI32(it->atk);
            w.writeI32(it->def);
            w.writeI32(it->heal);
        }
    }
    w.writeU32(static_cast<uint32_t>(statuses_.size()));
    for (const auto& s : statuses_) {
        w.writeU32(static_cast<uint32_t>(s.type));
        w.writeI32(s.duration);
        w.writeI32(s.magnitude);
    }
}

void Unit::load(engine::SaveReader& r) {
    uint32_t id = 0;
    r.readU32(id);
    unitId_ = static_cast<uint16_t>(id);
    uint32_t f = 0;
    r.readU32(f);
    faction_ = static_cast<unit::Faction>(f);
    r.readI32(hp_);
    r.readI32(stats_.maxHp);
    r.readI32(stats_.atk);
    r.readI32(stats_.def);
    r.readI32(stats_.mov);
    r.readI32(stats_.range);
    math::Vec2 cell = cell_;
    r.readFloat(cell.x);
    r.readFloat(cell.y);
    setCell(cell);
    uint32_t invCount = 0;
    r.readU32(invCount);
    inventory_.clear();
    for (uint32_t i = 0; i < invCount; ++i) {
        ItemDef it;
        r.readString(it.name);
        r.readI32(it.value);
        r.readI32(it.atk);
        r.readI32(it.def);
        r.readI32(it.heal);
        inventory_.add(it.name, std::move(it), 1);
    }
    equipment_ = engine::EquipSlots<ItemDef, EquipSlot>{
        EquipSlot::Head, EquipSlot::Body, EquipSlot::Hand, EquipSlot::Feet, EquipSlot::Accessory};
    for (uint32_t s = 0; s < static_cast<uint32_t>(EquipSlot::Count); ++s) {
        const auto slot = static_cast<EquipSlot>(s);
        uint32_t present = 0;
        r.readU32(present);
        if (present) {
            ItemDef it;
            r.readString(it.name);
            r.readI32(it.value);
            r.readI32(it.atk);
            r.readI32(it.def);
            r.readI32(it.heal);
            equipment_.equip(slot, std::move(it));
        }
    }
    uint32_t statusCount = 0;
    r.readU32(statusCount);
    statuses_.clear();
    statuses_.reserve(statusCount);
    for (uint32_t i = 0; i < statusCount; ++i) {
        uint32_t t = 0;
        r.readU32(t);
        unit::StatusEffect s;
        s.type = static_cast<unit::StatusType>(t);
        r.readI32(s.duration);
        r.readI32(s.magnitude);
        statuses_.push_back(s);
    }
}

void Unit::selfTest() {
    aout << "[unit] self-test starting" << std::endl;

    {
        unit::Stats s{20, 7, 3, 5, 1};
        Unit c(nullptr, unit::Faction::Player, s);
        assert(c.hp() == 20);
        assert(c.maxHp() == 20);
        assert(c.faction() == unit::Faction::Player);
        assert(c.stats().atk == 7);
        assert(c.inventory().totalItems() == 0);
        assert(c.equipment().isEmpty(EquipSlot::Hand));
        c.takeDamage(8);
        assert(c.hp() == 12);
        c.heal(100);
        assert(c.hp() == 20);
        c.takeDamage(999);
        assert(c.hp() == 0);
        assert(c.isDead());
        c.heal(5);
        assert(c.isDead());
        assert(!c.pickable());
        c.setSelected(true);
        assert(!c.isSelected());
    }

    {
        anim::Skeleton skel;
        skel.bones.resize(1);
        auto clip = [](float len, bool loop) {
            anim::SkeletalClip c;
            c.loop = loop;
            c.tracks.push_back({0, {{0.0f, math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)},
                                    {len, math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)}}});
            return c;
        };
        const anim::SkeletalClip idle = clip(1.0f, true), hurt = clip(0.4f, false),
                                 death = clip(0.8f, false);
        SceneNode root;
        Unit u(&root, unit::Faction::Enemy, unit::Stats{5, 1, 0, 3, 1});
        assert(u.visualNode() == &root);
        assert(!u.isSkinned());
        SceneNode* vis = root.addChild();
        vis->setSkeleton(&skel);
        u.setVisualNode(vis);
        u.setClips({&idle, nullptr, nullptr, &hurt, &death});
        assert(u.isSkinned());
        u.playClip(&hurt);
        assert(vis->skeletalClip() == &hurt);
        for (int i = 0; i < 5; ++i) { root.update(0.1f); u.update(0.1f); }
        assert(vis->skeletalClip() == &idle);
        u.setSelected(true);
        u.takeDamage(5);
        assert(vis->skeletalClip() == &death);
        assert(!u.isSelected() && !u.pickable());
        for (int i = 0; i < 20; ++i) { root.update(0.1f); u.update(0.1f); }
        assert(vis->skeletalClip() == &death);
    }

    {
        ItemDef sword{"Iron Sword", 100, 5, 0, 0};
        ItemDef shield{"Wood Shield", 50, 0, 3, 0};
        ItemDef potion{"Heal Pot", 25, 0, 0, 10};

        engine::Bag<ItemDef> bag;
        bag.add(sword.name, sword, 1);
        bag.add(potion.name, potion, 1);
        assert(bag.countOf("Iron Sword") == 1);
        assert(bag.countOf("Heal Pot") == 1);
        assert(bag.totalItems() == 2);
        assert(bag.remove("Iron Sword"));
        assert(bag.countOf("Iron Sword") == 0);
        assert(bag.totalItems() == 1);

        engine::EquipSlots<ItemDef, EquipSlot> eq{
            EquipSlot::Head, EquipSlot::Body, EquipSlot::Hand,
            EquipSlot::Feet, EquipSlot::Accessory};
        assert(eq.equip(EquipSlot::Hand, sword));
        assert(!eq.equip(EquipSlot::Hand, shield));
        assert(eq.get(EquipSlot::Hand) != nullptr);
        assert(eq.get(EquipSlot::Hand)->name == "Iron Sword");
        assert(!eq.isEmpty(EquipSlot::Hand));
        assert(eq.unequip(EquipSlot::Hand).has_value());
        assert(eq.isEmpty(EquipSlot::Hand));
    }

    {
        unit::Stats s{15, 5, 2, 4, 2};
        Unit c(nullptr, unit::Faction::Ally, s);
        c.setCell(math::Vec2(3.0f, 7.0f));
        ItemDef sword{"S", 10, 1, 0, 0};
        c.inventory().add(sword.name, sword, 1);
        c.equipment().equip(EquipSlot::Hand, sword);

        engine::SaveWriter w;
        c.save(w);
        const auto& buf = w.bytes();
        assert(!buf.empty());

        unit::Stats s2{1, 1, 1, 1, 1};
        Unit c2(nullptr, unit::Faction::Neutral, s2);
        {
            engine::SaveReader r(buf.data(), buf.size());
            c2.load(r);
        }
        assert(c2.faction() == unit::Faction::Ally);
        assert(c2.unitId() == 0);
        assert(c2.hp() == 15);
        assert(c2.stats().atk == 5);
        assert(c2.stats().range == 2);
        assert(math::approxEqual(c2.cell().x, 3.0f));
        assert(math::approxEqual(c2.cell().y, 7.0f));
        assert(c2.inventory().totalItems() == 1);
        assert(c2.equipment().get(EquipSlot::Hand) != nullptr);
        assert(c2.equipment().get(EquipSlot::Hand)->name == "S");
    }

    {
        unit::Stats s{10, 5, 2, 4, 1};
        Unit c(nullptr, unit::Faction::Enemy, s);
        c.applyStatus({unit::StatusType::Poisoned, 3, 2});
        c.applyStatus({unit::StatusType::Stunned, 1, 0});
        assert(c.isStunned());
        assert(c.hasStatus(unit::StatusType::Poisoned));
        assert(c.statuses().size() == 2);

        c.tickStatuses();
        assert(c.hp() == 8);
        c.tickStatuses();
        assert(c.hp() == 6);
        c.tickStatuses();
        c.tickStatuses();
        c.tickStatuses();
        assert(c.statuses().empty());
        assert(!c.isStunned());

        c.clearStatuses();
        c.applyStatus({unit::StatusType::Poisoned, 1, 1});
        Unit c2(nullptr, unit::Faction::Enemy, s);
        engine::SaveWriter w;
        c.save(w);
        engine::SaveReader r(w.bytes().data(), w.bytes().size());
        c2.load(r);
        assert(c2.hasStatus(unit::StatusType::Poisoned));
        assert(c2.statuses()[0].duration == 1);
        assert(c2.statuses()[0].magnitude == 1);
    }

    aout << "[unit] self-test PASS" << std::endl;
}

} // namespace game
