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

struct ClipSet {
    const anim::SkeletalClip* idle    = nullptr;
    const anim::SkeletalClip* walk    = nullptr;
    const anim::SkeletalClip* attack  = nullptr;
    const anim::SkeletalClip* hurt    = nullptr;
};

struct UnitDef {
    std::string             name;
    unit::Stats             baseStats;
    unit::Faction           faction        = unit::Faction::Neutral;
    asset::Handle           meshHandle     = asset::kNull;
    asset::Handle           materialHandle = asset::kNull;
    const anim::Skeleton*   skeleton       = nullptr;
    ClipSet                 clips;
    math::Vec3              baseColor      = {1.0f, 1.0f, 1.0f};
    math::Vec3              selectedColor  = {1.5f, 1.5f, 0.5f};
    bool                    useRig         = true;
    float                   rootDropY      = 0.5f;

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
