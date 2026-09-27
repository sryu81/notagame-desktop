#include "EquipSlots.h"

#include "AndroidOut.h"

#include <cassert>
#include <string>
#include <utility>

namespace engine {
namespace {

enum class Slot { Weapon, Armor, Trinket };

} // namespace

void equipSlotsSelfTest() {
    aout << "[equip-slots] self-test starting" << std::endl;

    {
        EquipSlots<std::string, Slot> eq({Slot::Weapon, Slot::Armor, Slot::Trinket});
        assert(eq.size() == 3);
        assert(eq.isEmpty(Slot::Weapon));
        assert(eq.isEmpty(Slot::Armor));
        assert(eq.isEmpty(Slot::Trinket));
        assert(eq.get(Slot::Weapon) == nullptr);

        assert(eq.equip(Slot::Weapon, std::string("excalibur")));
        assert(!eq.isEmpty(Slot::Weapon));
        const std::string* w = eq.get(Slot::Weapon);
        assert(w != nullptr);
        assert(*w == "excalibur");

        assert(!eq.equip(Slot::Weapon, std::string("another")));
        assert(*eq.get(Slot::Weapon) == "excalibur");

        auto taken = eq.unequip(Slot::Weapon);
        assert(taken.has_value());
        assert(*taken == "excalibur");
        assert(eq.isEmpty(Slot::Weapon));

        auto taken2 = eq.unequip(Slot::Weapon);
        assert(!taken2.has_value());
    }

    {
        EquipSlots<int, Slot> eq;
        assert(eq.size() == 0);
        assert(!eq.hasSlot(Slot::Weapon));
        assert(!eq.equip(Slot::Weapon, 1));

        eq = EquipSlots<int, Slot>({Slot::Weapon});
        assert(eq.hasSlot(Slot::Weapon));
        assert(eq.equip(Slot::Weapon, 42));
        assert(*eq.get(Slot::Weapon) == 42);
    }

    {
        EquipSlots<std::string, Slot> eq({Slot::Weapon, Slot::Armor});
        eq.equip(Slot::Weapon, "sword");
        eq.equip(Slot::Armor,  "plate");
        assert(*eq.get(Slot::Weapon) == "sword");
        assert(*eq.get(Slot::Armor)  == "plate");

        eq.unequip(Slot::Weapon);
        assert(eq.isEmpty(Slot::Weapon));
        assert(*eq.get(Slot::Armor) == "plate");
    }

    aout << "[equip-slots] self-test PASS" << std::endl;
}

} // namespace engine
