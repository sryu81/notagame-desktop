#ifndef NOT_A_GAME_LEVEL_LOADER_H
#define NOT_A_GAME_LEVEL_LOADER_H

#include <cstddef>
#include <cstdint>
#include <string>
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
    std::string catalogId;
    uint16_t  unitId         = 0;
};

struct Prop {
    uint8_t     type = 0;
    uint8_t     x = 0;
    uint8_t     y = 0;
    bool        blocks = true;
    float       yaw = 0.0f;
    math::Vec3  pos{0.0f, 0.5f, 0.0f};
    float       scale = 1.0f;
    math::Vec3  color{1.0f, 1.0f, 1.0f};
};

enum class WinType : uint8_t { DefeatAll = 0, SurviveRounds = 1, ReachCell = 2 };

struct WinCondition {
    WinType type = WinType::DefeatAll;
    uint8_t a    = 0;
    uint8_t b    = 0;
};

struct Level {
    uint32_t              version   = 3;
    WinCondition          win;
    uint16_t              width     = 0;
    uint16_t              height    = 0;
    float                 cellSize  = 1.0f;
    std::vector<uint8_t>  terrain;
    std::vector<float>    cellY;
    std::vector<UnitSpawn> units;
    std::vector<Prop>     props;
};

bool load(const uint8_t* bytes, size_t size, Level& out);
std::vector<uint8_t> save(const Level& level);
const char* toString(WinType t);
std::string describe(const WinCondition& w);

void selfTest();

} // namespace level

#endif
