#include "Sky.h"

#include "AndroidOut.h"

#include <cassert>
#include <cmath>

namespace engine {
namespace sky {

namespace {

float wrap01(float t) {
    t = t - std::floor(t);
    if (t < 0.0f) t += 1.0f;
    return t;
}

float nightBlend(float sunHeight, const SkyConfig& cfg) {
    return math::saturate(1.0f - (sunHeight + cfg.nightBlendShift) * cfg.nightBlendScale);
}

math::Vec3 pickSky(float sunHeight, const SkyPalette& p, const SkyConfig& cfg) {
    if (sunHeight < 0.0f) {
        const float k = math::saturate((sunHeight - cfg.dawnStart) / cfg.dawnWidth);
        return math::mix(p.nightSky, p.dawnSky, k);
    }
    return math::mix(p.dawnSky, p.noonSky, math::saturate(sunHeight * cfg.noonScale));
}

}

SunState computeSunState(float t, const SkyPalette& sky, Weather weather, const WeatherPalette& wx, const SkyConfig& cfg, const math::Vec3& skyAxis) {
    t = wrap01(t);
    const math::Vec3 worldUp(0.0f, 1.0f, 0.0f);
    math::Vec3 north = skyAxis - worldUp * math::dot(skyAxis, worldUp);
    const float nLen = north.length();
    if (nLen < 1e-4f) north = math::Vec3(1.0f, 0.0f, 0.0f);
    else north = north / nLen;
    const float phase = math::TWO_PI * t;
    const math::Vec3 lightDir = worldUp * (-std::cos(phase)) + north * std::sin(phase);
    const float sunHeight = math::dot(lightDir, worldUp);

    SunState s;
    s.lightDir    = lightDir;
    s.sunHeight   = sunHeight;
    s.sunColor    = sky.sunColor;
    s.starIntensity = nightBlend(sunHeight, cfg);
    s.skyColor    = pickSky(sunHeight, sky, cfg);
    s.ambientColor = (sunHeight > 0.0f) ? sky.dayAmbient : math::mix(sky.nightAmbient, sky.dawnSky, cfg.ambientDawnMix);
    s.ambient     = math::saturate(sunHeight * cfg.ambientSunScale + cfg.ambientBaseline);

    if (weather == Weather::Rain) {
        s.skyColor     = math::mix(s.skyColor, wx.rainSky, cfg.rainSkyMix);
        s.ambientColor = math::mix(s.ambientColor, wx.rainAmbient, cfg.rainAmbientMix);
        s.starIntensity *= cfg.rainStarDim;
    } else if (weather == Weather::Storm) {
        s.skyColor     = math::mix(s.skyColor, wx.stormSky, cfg.stormSkyMix);
        s.ambientColor = math::mix(s.ambientColor, wx.stormAmbient, cfg.stormAmbientMix);
        s.starIntensity *= cfg.stormStarDim;
    }

    return s;
}

void selfTest() {
    aout << "[sky] self-test starting" << std::endl;

    SkyPalette sky {
        {0.02f, 0.03f, 0.10f},
        {0.95f, 0.55f, 0.35f},
        {0.45f, 0.70f, 0.95f},
        {0.95f, 0.40f, 0.25f},
        {0.10f, 0.12f, 0.20f},
        {0.55f, 0.55f, 0.55f},
        {1.0f, 0.95f, 0.85f},
        {0.75f, 0.80f, 1.0f},
    };
    WeatherPalette wx {
        {0.45f, 0.48f, 0.55f},
        {0.18f, 0.20f, 0.25f},
        {0.30f, 0.30f, 0.32f},
        {0.15f, 0.15f, 0.18f},
        {0.65f, 0.75f, 0.90f},
        {1.0f, 1.0f, 0.95f},
    };

    {
        const SunState s = computeSunState(0.0f, sky, Weather::Clear, wx);
        assert(s.sunHeight < 0.0f);
        assert(s.starIntensity > 0.9f);
    }
    {
        const SunState s = computeSunState(0.25f, sky, Weather::Clear, wx);
        assert(std::abs(s.sunHeight) < 0.1f);
        assert(math::dot(s.lightDir, math::Vec3(1,0,0)) > 0.9f);
        assert(s.starIntensity < 0.9f);
    }
    {
        const SunState s = computeSunState(0.5f, sky, Weather::Clear, wx);
        assert(s.sunHeight > 0.9f);
        assert(s.starIntensity < 0.1f);
    }
    {
        const SunState s = computeSunState(0.75f, sky, Weather::Clear, wx);
        assert(std::abs(s.sunHeight) < 0.1f);
        assert(math::dot(s.lightDir, math::Vec3(1,0,0)) < -0.9f);
    }
    {
        const SunState a = computeSunState(0.25f, sky, Weather::Clear, wx, SkyConfig{}, math::Vec3(1,0,0));
        const SunState b = computeSunState(0.25f, sky, Weather::Clear, wx, SkyConfig{}, math::Vec3(0,0,1));
        assert(math::dot(a.lightDir, math::Vec3(1,0,0)) > 0.9f);
        assert(std::abs(math::dot(a.lightDir, math::Vec3(0,0,1))) < 1e-4f);
        assert(math::dot(b.lightDir, math::Vec3(0,0,1)) > 0.9f);
        assert(std::abs(math::dot(b.lightDir, math::Vec3(1,0,0))) < 1e-4f);
    }
    {
        const SunState a = computeSunState(0.30f, sky, Weather::Clear, wx);
        const SunState b = computeSunState(1.30f, sky, Weather::Clear, wx);
        assert(std::abs(a.sunHeight - b.sunHeight) < 1e-4f);
        assert(std::abs(a.lightDir.x - b.lightDir.x) < 1e-4f);
    }
    {
        const SunState c = computeSunState(0.1f, sky, Weather::Clear, wx);
        const SunState r = computeSunState(0.1f, sky, Weather::Rain, wx);
        assert(r.starIntensity < c.starIntensity);
        assert(r.starIntensity < 0.5f);
    }
    {
        SkyConfig steep;
        steep.nightBlendScale = 8.0f;
        const SunState a = computeSunState(0.25f, sky, Weather::Clear, wx, SkyConfig{});
        const SunState b = computeSunState(0.25f, sky, Weather::Clear, wx, steep);
        assert(a.starIntensity > b.starIntensity);
        assert(b.starIntensity < 0.5f);
    }
    {
        SkyConfig bright;
        bright.ambientBaseline = 0.5f;
        bright.ambientSunScale = 1.0f;
        const SunState a = computeSunState(0.5f, sky, Weather::Clear, wx, SkyConfig{});
        const SunState b = computeSunState(0.5f, sky, Weather::Clear, wx, bright);
        assert(b.ambient > a.ambient);
    }
    {
        SkyConfig fastDawn;
        fastDawn.dawnWidth = 0.1f;
        const SunState a = computeSunState(0.24f, sky, Weather::Clear, wx, SkyConfig{});
        const SunState b = computeSunState(0.24f, sky, Weather::Clear, wx, fastDawn);
        const float aNightness = a.skyColor.length();
        const float bNightness = b.skyColor.length();
        assert(std::abs(aNightness - bNightness) > 0.001f);
    }

    aout << "[sky] self-test PASS" << std::endl;
}

}
}
