#ifndef NOT_A_GAME_LIGHTING_H
#define NOT_A_GAME_LIGHTING_H

#include "Math.h"

namespace lighting {

float lambert(const math::Vec3 &normal, const math::Vec3 &lightDir);

void selfTest();

}

#endif
