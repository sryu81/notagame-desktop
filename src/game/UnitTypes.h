#ifndef NOT_A_GAME_UNIT_TYPES_H
#define NOT_A_GAME_UNIT_TYPES_H

#include <string>
#include <vector>

#include "Math.h"
#include "Unit.h"

namespace game {

struct UnitType {
    const char*   id;
    const char*   name;
    unit::Faction faction;
    unit::Stats   stats;
    math::Vec3    color;
    bool          stuns;
};

const std::vector<UnitType>& unitTypes();
const UnitType* findUnitType(const std::string& id);

} // namespace game

#endif
