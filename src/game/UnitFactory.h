#ifndef NOT_A_GAME_UNIT_FACTORY_H
#define NOT_A_GAME_UNIT_FACTORY_H

#include <memory>

#include "UnitDef.h"
#include "VisualDef.h"

class SceneNode;

namespace game {
class Unit;
}

namespace unit {

std::unique_ptr<game::Unit> spawn(
    SceneNode* parent,
    const UnitDef& def,
    const engine::RigResources& rigRes);

} // namespace unit

#endif
