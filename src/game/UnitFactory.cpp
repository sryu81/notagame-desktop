#include "UnitFactory.h"

#include "Engine.h"
#include "Unit.h"
#include "UnitDef.h"

namespace unit {

std::unique_ptr<game::Unit> spawn(
    SceneNode* parent,
    const UnitDef& def,
    const engine::RigResources& rigRes) {
    if (def.useRig) {
        SceneNode* rigRoot = engine::scenegraph::buildRigNode(
            *parent, rigRes, def.toRigDef(), math::Mat4::identity());
        auto c = std::make_unique<game::Unit>(rigRoot, def.faction, def.baseStats);
        if (def.clips.idle) {
            rigRoot->setSkeletalClip(def.clips.idle);
        }
        return c;
    }
    return std::make_unique<game::Unit>(nullptr, def.faction, def.baseStats);
}

} // namespace unit
