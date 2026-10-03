#ifndef NOT_A_GAME_UNIT_H
#define NOT_A_GAME_UNIT_H

#include <vector>

#include "Bag.h"
#include "EquipSlots.h"
#include "Item.h"
#include "Math.h"
#include "Object.h"

class SceneNode;
namespace anim { class Clip; struct SkeletalClip; }
namespace grid { class BattleGrid; }

namespace unit {

enum class Faction { Player, Enemy, Ally, Neutral };

struct Stats {
    int maxHp = 10;
    int atk = 5;
    int def = 3;
    int mov = 4;
    int range = 1;
};

struct ClipSet {
    const anim::SkeletalClip* idle    = nullptr;
    const anim::SkeletalClip* walk    = nullptr;
    const anim::SkeletalClip* attack  = nullptr;
    const anim::SkeletalClip* hurt    = nullptr;
    const anim::SkeletalClip* death   = nullptr;
};

enum class StatusType { Poisoned, Stunned };

struct StatusEffect {
    StatusType type;
    int duration;   // turns remaining
    int magnitude;  // damage per tick (Poisoned); ignored for Stunned
};

} // namespace unit

namespace game {

class Unit : public Object {
public:
    Unit(SceneNode* node, unit::Faction faction, const unit::Stats& stats);
    ~Unit() override;

    SceneNode* node() override { return node_; }
    SceneNode* visualNode() const { return visual_ ? visual_ : node_; }
    void setVisualNode(SceneNode* n) { visual_ = n; }
    void update(float dt) override;
    void save(engine::SaveWriter& w) const override;
    void load(engine::SaveReader& r) override;

    unit::Faction                      faction()    const { return faction_; }
    const unit::Stats&                 stats()      const { return stats_; }
    int                                hp()         const { return hp_; }
    int                                maxHp()      const { return stats_.maxHp; }
    bool                               isDead()     const { return hp_ <= 0; }
    bool                               pickable()   const { return node_ != nullptr && !isDead(); }
    const math::Vec2&                  cell()       const { return cell_; }
    void                               setCell(const math::Vec2& c);
    void                               setGrid(grid::BattleGrid* g);
    void                               takeDamage(int amount);
    void                               heal(int amount);

    engine::Bag<ItemDef>&                       inventory()  { return inventory_; }
    const engine::Bag<ItemDef>&                 inventory()  const { return inventory_; }
    engine::EquipSlots<ItemDef, EquipSlot>&     equipment()  { return equipment_; }
    const engine::EquipSlots<ItemDef, EquipSlot>& equipment() const { return equipment_; }

    void                               setBaseOffset(const math::Vec3& o);
    const math::Vec3&                  baseOffset() const { return baseOffset_; }
    void                               setBaseScale(float s) { baseScale_ = s; }
    float                              baseScale()  const { return baseScale_; }

    void                               setClips(const unit::ClipSet& c) { clips_ = c; }
    const unit::ClipSet&               clips()     const { return clips_; }
    bool                               isSkinned() const;
    void                               playClip(const anim::SkeletalClip* c);

    void                               setIdleClip(const anim::Clip* c) { idleClip_ = c; }
    const anim::Clip*                  idleClip()  const { return idleClip_; }
    void                               setSelected(bool sel);
    bool                               isSelected() const { return selected_; }

    void                               applyStatus(unit::StatusEffect effect);
    void                               clearStatuses();
    void                               tickStatuses();
    bool                               isStunned() const;
    bool                               hasStatus(unit::StatusType t) const;
    const std::vector<unit::StatusEffect>& statuses() const { return statuses_; }

    static void selfTest();

private:
    SceneNode*                              node_     = nullptr;
    SceneNode*                              visual_   = nullptr;
    grid::BattleGrid*                       grid_     = nullptr;
    math::Vec3                              baseOffset_{0.0f};
    float                                   baseScale_ = 1.0f;
    unit::ClipSet                           clips_;
    unit::Faction                           faction_;
    unit::Stats                             stats_;
    int                                     hp_;
    math::Vec2                              cell_{0.0f, 0.0f};
    bool                                    selected_ = false;
    engine::Bag<ItemDef>                    inventory_;
    engine::EquipSlots<ItemDef, EquipSlot>  equipment_{
        EquipSlot::Head, EquipSlot::Body, EquipSlot::Hand,
        EquipSlot::Feet, EquipSlot::Accessory};
    const anim::Clip*                       idleClip_ = nullptr;
    std::vector<unit::StatusEffect>         statuses_;
};

} // namespace game

#endif
