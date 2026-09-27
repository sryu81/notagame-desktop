#ifndef NOT_A_GAME_STARS_H
#define NOT_A_GAME_STARS_H

#include "Math.h"
#include <cstdint>
#include <vector>

namespace engine {
namespace stars {

struct Star {
    math::Vec3 dir;
    float magnitude;
    math::Vec3 color;
};

struct StarField {
    std::vector<Star> stars;
};

bool loadStars(const uint8_t* bytes, size_t size, StarField& out);

void selfTest();

}
}

#endif
