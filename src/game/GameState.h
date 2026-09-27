#ifndef NOT_A_GAME_GAME_STATE_H
#define NOT_A_GAME_GAME_STATE_H

#include <vector>

#include "Bag.h"
#include "EquipSlots.h"
#include "Item.h"
#include "Saveable.h"

namespace game { class EntityRegistry; }

namespace game {

struct UnitSnapshot {
    int hp = 10;
    int maxHp = 10;
    engine::Bag<game::ItemDef> inventory;
    engine::EquipSlots<game::ItemDef, game::EquipSlot> equipment{
        game::EquipSlot::Head, game::EquipSlot::Body, game::EquipSlot::Hand,
        game::EquipSlot::Feet, game::EquipSlot::Accessory};
};

class GameState : public engine::Saveable {
public:
    void initDefaultRoster(size_t count);

    size_t rosterSize() const { return roster_.size(); }
    UnitSnapshot& entry(size_t i) { return roster_[i]; }
    const UnitSnapshot& entry(size_t i) const { return roster_[i]; }

    void captureFrom(EntityRegistry& reg);
    void restoreTo(EntityRegistry& reg);

    void clear();
    bool empty() const { return roster_.empty(); }

    void save(engine::SaveWriter& w) const override;
    void load(engine::SaveReader& r) override;

    static void selfTest();

private:
    std::vector<UnitSnapshot> roster_;
};

} // namespace game

#endif
