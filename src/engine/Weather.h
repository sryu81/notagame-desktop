#ifndef NOT_A_GAME_WEATHER_H
#define NOT_A_GAME_WEATHER_H

#include "Math.h"

#include <cstdint>

namespace engine {
namespace wx {

enum class Weather : uint8_t { Clear, Rain, Storm };

struct LightningState {
    float timer = 0.0f;
    float nextDelay = 0.0f;
    float flashIntensity = 0.0f;
    float flashDecay      = 6.0f;
    float baseDelay       = 3.0f;
    float jitterRange     = 7.0f;
    float peakIntensity   = 1.0f;
    int   seed            = 0;
};

void initLightning(LightningState& s, int seed = 0);

bool updateLightning(LightningState& s, float dt, float& outFlash);

math::Vec3 rainSpawnPos(const math::Vec3& cameraPos);

void selfTest();

}
}

#endif
