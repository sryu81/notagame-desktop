#include "Climate.h"

#include "Engine.h"

#include <cassert>

namespace game {

void ClimateState::update(float dt) {
    if (current == engine::wx::Weather::Storm) {
        float flash = 0.0f;
        engine::wx::updateLightning(lightning, dt, flash);
    }
}

float ClimateState::dayPhase() const {
    if (!clock) return 0.0f;
    constexpr double kSecondsPerDay = 24.0 * 60.0 * 60.0;
    double t = (clock->timeOfDaySeconds() * static_cast<double>(starRotationDays))
             / kSecondsPerDay;
    t = t - std::floor(t);
    return static_cast<float>(t);
}

void ClimateState::applyToRenderer(Renderer& r) const {
    r.setStarRenderConfig(stars);
}

void ClimateState::startLightning() {
    int seed = clock ? static_cast<int>(clock->totalSeconds() * 1000.0) ^ 0xC0FFEE
                     : 0xC0FFEE;
    engine::wx::initLightning(lightning, seed);
}

void ClimateState::setWeather(engine::wx::Weather w, int seed) {
    current = w;
    if (w == engine::wx::Weather::Storm) {
        engine::wx::initLightning(lightning, seed);
    }
}

ClimatePalettes defaultPalettes() {
    ClimatePalettes p;
    p.sky = engine::sky::SkyPalette {
        {0.02f, 0.03f, 0.10f},
        {0.95f, 0.55f, 0.35f},
        {0.45f, 0.70f, 0.95f},
        {0.95f, 0.40f, 0.25f},
        {0.10f, 0.12f, 0.20f},
        {0.55f, 0.55f, 0.55f},
        {1.0f, 0.95f, 0.85f},
        {0.75f, 0.80f, 1.0f},
    };
    p.weather = engine::sky::WeatherPalette {
        {0.45f, 0.48f, 0.55f},
        {0.18f, 0.20f, 0.25f},
        {0.30f, 0.30f, 0.32f},
        {0.15f, 0.15f, 0.18f},
        {0.65f, 0.75f, 0.90f},
        {1.0f, 0.95f, 0.85f},
    };
    return p;
}

void ClimateState::selfTest() {
    aout << "[climate] self-test starting" << std::endl;

    {
        ClimateState c;
        assert(c.current == engine::wx::Weather::Clear);
        assert(c.starRotationDays == 1.0f);
        assert(c.clock == nullptr);
        assert(math::approxEqual(c.dayPhase(), 0.0f));
    }

    {
        engine::clock::Clock clock;
        clock.setDateTime(1, 1, 1, 0, 0, 0);
        ClimateState c;
        c.starRotationDays = 1.0f;
        c.clock = &clock;

        assert(math::approxEqual(c.dayPhase(), 0.0f, 1e-4f));

        clock.setTimeOfDay(6, 0, 0);
        assert(math::approxEqual(c.dayPhase(), 0.25f, 1e-4f));

        clock.setTimeOfDay(12, 0, 0);
        assert(math::approxEqual(c.dayPhase(), 0.5f, 1e-4f));

        clock.setTimeOfDay(18, 0, 0);
        assert(math::approxEqual(c.dayPhase(), 0.75f, 1e-4f));

        clock.setTimeOfDay(0, 0, 0);
        assert(math::approxEqual(c.dayPhase(), 0.0f, 1e-4f));

        c.starRotationDays = 2.0f;
        clock.setTimeOfDay(3, 0, 0);
        assert(math::approxEqual(c.dayPhase(), 0.25f, 1e-4f));

        clock.pause();
        clock.addSeconds(3600.0);
        assert(math::approxEqual(c.dayPhase(), 0.25f + 2.0f / 24.0f, 1e-4f));
    }

    {
        engine::clock::Clock clock;
        ClimateState c;
        c.clock = &clock;
        c.current = engine::wx::Weather::Storm;
        c.startLightning();
        assert(c.isRaining());
    }

    aout << "[climate] self-test PASS" << std::endl;
}

}
