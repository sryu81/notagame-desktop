#ifndef NOT_A_GAME_UNIT_DEF_H
#define NOT_A_GAME_UNIT_DEF_H

#include <string>

#include "Animation.h"
#include "AssetRegistry.h"
#include "Bone.h"
#include "Math.h"
#include "Unit.h"
#include "VisualDef.h"

namespace unit {

struct UnitDef {
    std::string             name;
    unit::Stats             baseStats;
    unit::Faction           faction        = unit::Faction::Neutral;
    asset::Handle           meshHandle     = asset::kNull;
    asset::Handle           materialHandle = asset::kNull;
    const anim::Skeleton*   skeleton       = nullptr;
    const Mesh*             mesh           = nullptr;
    const Model*            model          = nullptr;
    const Material*         material       = nullptr;
    float                   modelYawDeg    = 0.0f;
    float                   baseScale      = 1.0f;
    ClipSet                 clips;
    math::Vec3              baseColor      = {1.0f, 1.0f, 1.0f};
    math::Vec3              selectedColor  = {1.5f, 1.5f, 0.5f};
    bool                    useRig         = true;
    float                   rootDropY      = 0.5f;

    bool skinned() const { return skeleton != nullptr && mesh != nullptr; }

    engine::VisualDef toVisualDef() const {
        engine::VisualDef v;
        v.mesh      = mesh;
        v.model     = model;
        v.material  = material;
        v.skeleton  = skeleton;
        v.idleClip  = clips.idle;
        v.baseColor = baseColor;
        return v;
    }

    engine::RigDef toRigDef() const {
        engine::RigDef r;
        r.bodyColor     = baseColor;
        r.selectedColor = selectedColor;
        r.rootDropY     = rootDropY;
        return r;
    }
};

} // namespace unit

#endif
