#include "PropFactory.h"

#include "Engine.h"
#include "Prop.h"

namespace prop {

std::unique_ptr<game::Prop> spawn(
    SceneNode* parent,
    const PropDef& def,
    const engine::VisualDef& v) {
    engine::VisualDef vv = v;
    vv.baseColor = def.color;
    SceneNode* n = engine::scenegraph::buildMeshNode(
        *parent, vv, math::Mat4::scale(def.scale));
    auto p = std::make_unique<game::Prop>(n);
    p->setScale(def.scale);
    return p;
}

} // namespace prop
