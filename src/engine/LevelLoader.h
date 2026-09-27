#ifndef NOT_A_GAME_LEVEL_LOADER_H
#define NOT_A_GAME_LEVEL_LOADER_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "Math.h"

namespace level {

struct UnitSpawn {
    uint8_t   faction        = 0;
    uint8_t   x              = 0;
    uint8_t   y              = 0;
    bool      skinned        = false;
    math::Vec3 baseColor{1.0f, 1.0f, 1.0f};
    math::Vec3 selectedColor{1.5f, 1.5f, 0.5f};
    int       maxHp          = 0;
    int       atk            = 0;
    int       def            = 0;
    int       mov            = 0;
    int       range          = 0;
};

struct Prop {
    uint8_t     type = 0;
    math::Vec3  pos{0.0f, 0.5f, 0.0f};
    float       scale = 1.0f;
    math::Vec3  color{1.0f, 1.0f, 1.0f};
};

struct Level {
    uint16_t              width     = 0;
    uint16_t              height    = 0;
    float                 cellSize  = 1.0f;
    std::vector<uint8_t>  terrain;
    std::vector<float>    cellY;
    std::vector<UnitSpawn> units;
    std::vector<Prop>     props;
};

bool load(const uint8_t* bytes, size_t size, Level& out);

void selfTest();

} // namespace level

#endif
