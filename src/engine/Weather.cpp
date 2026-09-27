#include "Weather.h"

#include "AndroidOut.h"

#include <cassert>
#include <cmath>
#include <cstdlib>

namespace engine {
namespace wx {

void initLightning(LightningState& s, int seed) {
    s.seed = seed;
    s.timer = 0.0f;
    s.nextDelay = 0.0f;
    s.flashIntensity = 0.0f;
}

bool updateLightning(LightningState& s, float dt, float& outFlash) {
    outFlash = 0.0f;
    if (s.flashIntensity > 0.0f) {
        s.flashIntensity -= s.flashDecay * dt;
        if (s.flashIntensity < 0.0f) s.flashIntensity = 0.0f;
        outFlash = s.flashIntensity;
        return true;
    }
    s.timer += dt;
    if (s.nextDelay <= 0.0f) {
        const int r = (s.seed * 1103515245 + (int)(s.timer * 1000.0f) + 12345) & 0x7fffffff;
        const int jitterMs = (int)(s.jitterRange * 1000.0f);
        s.nextDelay = s.baseDelay + (jitterMs > 0 ? (r % jitterMs) / 1000.0f : 0.0f);
    }
    if (s.timer >= s.nextDelay) {
        s.flashIntensity = s.peakIntensity;
        s.timer = 0.0f;
        s.nextDelay = -1.0f;
        outFlash = s.peakIntensity;
        return true;
    }
    return false;
}

math::Vec3 rainSpawnPos(const math::Vec3& cameraPos) {
    const float angle  = static_cast<float>(rand() % 1000) / 1000.0f * 6.2832f;
    const float radius = static_cast<float>(rand() % 1000) / 1000.0f * 8.0f;
    return math::Vec3(
        cameraPos.x + radius * cosf(angle),
        cameraPos.y + 6.0f,
        cameraPos.z + radius * sinf(angle));
}

void selfTest() {
    aout << "[weather] self-test starting" << std::endl;

    {
        LightningState s;
        initLightning(s, 42);
        assert(s.flashIntensity == 0.0f);
        float flash = -1.0f;
        for (int i = 0; i < 200 && flash <= 0.0f; i++) {
            updateLightning(s, 0.5f, flash);
        }
        assert(flash > 0.0f);
        assert(s.flashIntensity > 0.0f);
        updateLightning(s, 0.5f, flash);
        assert(flash < 1.0f);
    }
    {
        LightningState s;
        initLightning(s, 42);
        s.baseDelay = 1.0f;
        s.jitterRange = 0.0f;
        s.peakIntensity = 0.5f;
        float flash = -1.0f;
        int steps = 0;
        for (int i = 0; i < 20 && flash <= 0.0f; i++) {
            updateLightning(s, 0.1f, flash);
            ++steps;
        }
        assert(flash > 0.0f);
        assert(flash < 1.0f);
        assert(s.flashIntensity == 0.5f);
    }
    {
        LightningState s;
        initLightning(s, 42);
        s.flashDecay = 100.0f;
        s.baseDelay = 1.0f;
        s.jitterRange = 0.0f;
        s.peakIntensity = 1.0f;
        float flash = 0.0f;
        int i = 0;
        for (; i < 15 && flash <= 0.0f; ++i) {
            updateLightning(s, 0.1f, flash);
        }
        assert(flash > 0.0f);
        updateLightning(s, 0.05f, flash);
        assert(flash < 0.01f);
    }
    {
        LightningState s;
        initLightning(s, 42);
        s.baseDelay = 1.0f;
        s.jitterRange = 0.0f;
        s.peakIntensity = 2.0f;
        float flash = -1.0f;
        for (int i = 0; i < 30 && flash <= 0.0f; i++) {
            updateLightning(s, 0.1f, flash);
        }
        assert(flash > 0.0f);
        assert(s.flashIntensity == 2.0f);
        assert(s.flashIntensity > 1.0f);
    }

    aout << "[weather] self-test PASS" << std::endl;
}

}
}
