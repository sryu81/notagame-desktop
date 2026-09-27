#ifndef NOT_A_GAME_SKY_H
#define NOT_A_GAME_SKY_H

#include "Math.h"
#include "Weather.h"

namespace engine {
namespace sky {

using wx::Weather;

struct SkyPalette {
    math::Vec3 nightSky;
    math::Vec3 dawnSky;
    math::Vec3 noonSky;
    math::Vec3 duskSky;
    math::Vec3 nightAmbient;
    math::Vec3 dayAmbient;
    math::Vec3 sunColor;
    math::Vec3 milkyWayTint;
};

struct WeatherPalette {
    math::Vec3 rainSky;
    math::Vec3 stormSky;
    math::Vec3 rainAmbient;
    math::Vec3 stormAmbient;
    math::Vec3 rainColor;
    math::Vec3 lightningColor;
};

struct SunState {
    math::Vec3 lightDir;
    float      ambient;
    math::Vec3 ambientColor;
    math::Vec3 skyColor;
    math::Vec3 sunColor;
    float      starIntensity;
    float      sunHeight;
};

struct SkyConfig {
    float nightBlendShift = 0.1f;
    float nightBlendScale = 4.0f;
    float dawnStart       = -0.3f;
    float dawnWidth       =  0.3f;
    float noonScale       =  2.0f;
    float ambientDawnMix  =  0.25f;
    float ambientSunScale =  0.5f;
    float ambientBaseline =  0.3f;
    float rainSkyMix      =  0.7f;
    float rainAmbientMix  =  0.7f;
    float rainStarDim     =  0.3f;
    float stormSkyMix     =  0.85f;
    float stormAmbientMix =  0.85f;
    float stormStarDim    =  0.1f;
};

SunState computeSunState(float t, const SkyPalette& sky, Weather weather, const WeatherPalette& wx, const SkyConfig& cfg = SkyConfig{}, const math::Vec3& skyAxis = math::Vec3(1.0f, 0.0f, 0.0f));

void selfTest();

}
}

#endif
