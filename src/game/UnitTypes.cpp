#include "UnitTypes.h"

namespace game {

const std::vector<UnitType>& unitTypes() {
    static const std::vector<UnitType> kTypes = {
        {"knight",  "KNIGHT",  unit::Faction::Player, {15, 6, 4, 4, 1}, {0.90f, 0.55f, 0.25f}, false, "knight.glb"},
        {"cavalry", "CAVALRY", unit::Faction::Player, {14, 6, 3, 6, 1}, {0.85f, 0.70f, 0.30f}, false, "knight_mounted.glb"},
        {"mage",    "MAGE",    unit::Faction::Ally,   {10, 7, 2, 3, 2}, {0.35f, 0.45f, 0.95f}, false, "mage.glb"},
        {"archer",  "ARCHER",  unit::Faction::Player, {10, 5, 2, 3, 3}, {0.30f, 0.80f, 0.40f}, true,  "archer.glb"},
        {"goblin",  "GOBLIN",  unit::Faction::Enemy,  {11, 7, 3, 4, 1}, {0.55f, 0.75f, 0.20f}, false, "goblin.glb"},
        {"slinger", "SLINGER", unit::Faction::Enemy,  {9, 6, 1, 3, 3},  {0.70f, 0.60f, 0.25f}, false, "slinger.glb"},
    };
    return kTypes;
}

const UnitType* findUnitType(const std::string& id) {
    for (const auto& t : unitTypes()) if (id == t.id) return &t;
    return nullptr;
}

} // namespace game
