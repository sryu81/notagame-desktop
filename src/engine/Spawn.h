#ifndef NOT_A_GAME_SPAWN_H
#define NOT_A_GAME_SPAWN_H

#include <memory>
#include <utility>

#include "Math.h"
#include "SceneGraphBuilder.h"
#include "VisualDef.h"

class SceneNode;

namespace engine {
namespace scenegraph {

template <typename T, typename... Args>
std::unique_ptr<T> spawn(
    SceneNode& parent,
    const VisualDef& v,
    const math::Mat4& xform,
    Args&&... extraArgs)
{
    SceneNode* node = v.skeleton
        ? buildSkinnedNode(parent, v, xform)
        : buildMeshNode(parent, v, xform);
    return std::make_unique<T>(node, std::forward<Args>(extraArgs)...);
}

void spawnSelfTest();

} // namespace scenegraph
} // namespace engine

#endif
