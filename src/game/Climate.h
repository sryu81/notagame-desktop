#ifndef NOT_A_GAME_CLIMATE_H
#define NOT_A_GAME_CLIMATE_H

#include "Clock.h"
#include "Renderer.h"
#include "Sky.h"
#include "Weather.h"

namespace game {

struct ClimateState {
    engine::wx::Weather       current  = engine::wx::Weather::Clear;
    engine::wx::LightningState lightning;
    const engine::clock::Clock* clock  = nullptr;

    Renderer::StarRenderConfig stars;
    float starRotationDays     = 1.0f;

    void update(float dt);
    float dayPhase() const;
    bool  isRaining() const { return current != engine::wx::Weather::Clear; }
    void  startLightning();
    void  setWeather(engine::wx::Weather w, int seed = 0);
    void  applyToRenderer(Renderer& r) const;

    static void selfTest();
};

struct ClimatePalettes {
    engine::sky::SkyPalette     sky;
    engine::sky::WeatherPalette weather;
};

ClimatePalettes defaultPalettes();

}

#endif
