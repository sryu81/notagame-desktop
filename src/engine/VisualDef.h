#ifndef NOT_A_GAME_VISUALDEF_H
#define NOT_A_GAME_VISUALDEF_H

#include "Animation.h"
#include "Bone.h"
#include "Math.h"

struct Mesh;
struct Model;
struct Material;

namespace engine {

struct VisualDef {
    const Mesh*             mesh     = nullptr;
    const Model*            model    = nullptr;
    const Material*         material = nullptr;
    const anim::Skeleton*   skeleton = nullptr;
    const anim::SkeletalClip* idleClip = nullptr;
    math::Vec3              baseColor{1.0f, 1.0f, 1.0f};
};

struct RigResources {
    const Mesh*     cubeMesh  = nullptr;
    const Model*    cubeModel = nullptr;
    const Material* material  = nullptr;
};

struct RigDef {
    math::Vec3 bodyColor{1.0f, 1.0f, 1.0f};
    math::Vec3 selectedColor{1.5f, 1.5f, 0.5f};
    float      rootDropY = 0.5f;
};

} // namespace engine

#endif
