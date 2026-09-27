#ifndef NOT_A_GAME_SCENE_GRAPH_BUILDER_H
#define NOT_A_GAME_SCENE_GRAPH_BUILDER_H

#include "Math.h"
#include "VisualDef.h"

class SceneNode;

namespace engine {
namespace scenegraph {

SceneNode* buildMeshNode(
    SceneNode& parent,
    const VisualDef& v,
    const math::Mat4& xform);

SceneNode* buildSkinnedNode(
    SceneNode& parent,
    const VisualDef& v,
    const math::Mat4& xform);

SceneNode* buildRigNode(
    SceneNode& parent,
    const RigResources& res,
    const RigDef& r,
    const math::Mat4& xform);

void selfTest();

} // namespace scenegraph
} // namespace engine

#endif
