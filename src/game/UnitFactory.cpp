#include "UnitFactory.h"

#include "Engine.h"
#include "Unit.h"
#include "UnitDef.h"

namespace unit {

std::unique_ptr<game::Unit> spawn(
    SceneNode* parent,
    const UnitDef& def,
    const engine::RigResources& rigRes) {
    if (def.skinned()) {
        SceneNode* root = parent->addChild();
        root->setLocal(math::Mat4::scale(math::Vec3(def.baseScale)));
        const math::Mat4 yaw = math::Mat4::rotation(
            math::Quat::fromAxisAngle(math::Vec3(0, 1, 0), math::radians(def.modelYawDeg)));
        SceneNode* visual = engine::scenegraph::buildSkinnedNode(*root, def.toVisualDef(), yaw);
        auto c = std::make_unique<game::Unit>(root, def.faction, def.baseStats);
        c->setVisualNode(visual);
        c->setClips(def.clips);
        c->setBaseScale(def.baseScale);
        return c;
    }
    if (def.useRig) {
        SceneNode* rigRoot = engine::scenegraph::buildRigNode(
            *parent, rigRes, def.toRigDef(), math::Mat4::identity());
        auto c = std::make_unique<game::Unit>(rigRoot, def.faction, def.baseStats);
        c->setClips(def.clips);
        c->setBaseScale(def.baseScale);
        return c;
    }
    return std::make_unique<game::Unit>(nullptr, def.faction, def.baseStats);
}

} // namespace unit
