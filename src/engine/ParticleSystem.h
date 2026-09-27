#ifndef NOT_A_GAME_PARTICLE_SYSTEM_H
#define NOT_A_GAME_PARTICLE_SYSTEM_H

#include <vector>

#include "Math.h"

namespace particle {

struct Particle {
    math::Vec3 position;
    math::Vec3 velocity;
    math::Vec4 color;
    float size;
    float lifetime;
    float age;
};

class Emitter {
public:
    struct Config {
        math::Vec3 gravity{0.0f, -2.0f, 0.0f};
        float upVelocityMult = 1.5f;
        float sizeJitter = 0.1f;
        float lifetimeJitterMin = 0.5f;
        float lifetimeJitterRange = 0.5f;
        float sizeShrinkFactor = 0.5f;
    };

    Emitter(int maxParticles = 512);
    ~Emitter();

    void setConfig(const Config& c) { config_ = c; }
    const Config& config() const { return config_; }

    void setGravity(const math::Vec3& g) { config_.gravity = g; }
    math::Vec3 gravity() const { return config_.gravity; }

    void setUpVelocityMult(float v) { config_.upVelocityMult = v; }
    float upVelocityMult() const { return config_.upVelocityMult; }

    void setSizeJitter(float v) { config_.sizeJitter = v; }
    float sizeJitter() const { return config_.sizeJitter; }

    void setLifetimeJitter(float minV, float rangeV) {
        config_.lifetimeJitterMin = minV;
        config_.lifetimeJitterRange = rangeV;
    }
    float lifetimeJitterMin() const { return config_.lifetimeJitterMin; }
    float lifetimeJitterRange() const { return config_.lifetimeJitterRange; }

    void setSizeShrinkFactor(float v) { config_.sizeShrinkFactor = v; }
    float sizeShrinkFactor() const { return config_.sizeShrinkFactor; }

    void emit(const math::Vec3& pos, const math::Vec3& vel,
              const math::Vec4& color, float size, float lifetime);
    void burst(const math::Vec3& pos, int count, float spread,
               const math::Vec4& color, float size, float lifetime);
    void update(float dt);

    int activeCount() const { return activeCount_; }
    bool isAlive() const { return activeCount_ > 0; }
    const std::vector<Particle>& particles() const { return particles_; }

    void clear();

    static void selfTest();

private:
    std::vector<Particle> particles_;
    int maxParticles_;
    int activeCount_ = 0;
    Config config_;
};

} // namespace particle

#endif
