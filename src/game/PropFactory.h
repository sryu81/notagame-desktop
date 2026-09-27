#ifndef NOT_A_GAME_PROP_FACTORY_H
#define NOT_A_GAME_PROP_FACTORY_H

#include <memory>

#include "PropDef.h"
#include "VisualDef.h"

class SceneNode;

namespace game {
class Prop;
}

namespace prop {

std::unique_ptr<game::Prop> spawn(
    SceneNode* parent,
    const PropDef& def,
    const engine::VisualDef& v);

} // namespace prop

#endif
