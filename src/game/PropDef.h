#ifndef NOT_A_GAME_PROP_DEF_H
#define NOT_A_GAME_PROP_DEF_H

#include <string>

#include "AssetRegistry.h"
#include "Math.h"

namespace prop {

struct PropDef {
    std::string   name;
    asset::Handle meshHandle     = asset::kNull;
    asset::Handle materialHandle = asset::kNull;
    math::Vec3    color          = {0.5f, 0.5f, 0.5f};
    math::Vec3    scale          = {1.0f, 1.0f, 1.0f};
    bool          solid          = true;
};

} // namespace prop

#endif
