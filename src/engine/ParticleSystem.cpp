#include "ParticleSystem.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>

#include "AndroidOut.h"

namespace particle {

Emitter::Emitter(int maxParticles)
        : particles_(static_cast<size_t>(maxParticles)),
          maxParticles_(maxParticles) {
    assert(maxParticles > 0);
}

Emitter::~Emitter() = default;

void Emitter::emit(const math::Vec3& pos, const math::Vec3& vel,
                   const math::Vec4& color, float size, float lifetime) {
    if (activeCount_ >= maxParticles_) return;
    Particle& p = particles_[activeCount_];
    p.position = pos;
    p.velocity = vel;
    p.color = color;
    p.size = size;
    p.lifetime = lifetime;
    p.age = 0.0f;
    p.baseAlpha = color.w;
    p.baseSize = size;
    ++activeCount_;
}

void Emitter::burst(const math::Vec3& pos, int count, float spread,
                    const math::Vec4& color, float size, float lifetime) {
    const float sj = config_.sizeJitter;
    const float ljMin = config_.lifetimeJitterMin;
    const float ljRange = config_.lifetimeJitterRange;
    for (int i = 0; i < count && activeCount_ < maxParticles_; ++i) {
        float theta = static_cast<float>(i) / static_cast<float>(count) * 2.0f * math::PI;
        float r = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * spread;
        float upFraction = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        math::Vec3 vel(std::cos(theta) * r,
                       upFraction * spread * config_.upVelocityMult,
                       std::sin(theta) * r);
        const float sizeJit = sj > 0.0f
            ? (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * sj)
            : 0.0f;
        const float lifeJit = ljMin + (ljRange > 0.0f
            ? (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * ljRange)
            : 0.0f);
        emit(pos, vel, color, size + sizeJit, lifetime * lifeJit);
    }
}

void Emitter::update(float dt) {
    int writeIdx = 0;
    for (int i = 0; i < activeCount_; ++i) {
        Particle& p = particles_[i];
        p.age += dt;
        if (p.age >= p.lifetime) continue;
        float t = p.age / p.lifetime;
        p.position = p.position + p.velocity * dt;
        p.velocity = p.velocity + config_.gravity * dt;
        p.color.w = p.baseAlpha * math::clamp(1.0f - t, 0.0f, 1.0f);
        p.size = p.baseSize * (1.0f - t * config_.sizeShrinkFactor);
        if (writeIdx != i) particles_[writeIdx] = p;
        ++writeIdx;
    }
    activeCount_ = writeIdx;
}

void Emitter::clear() {
    activeCount_ = 0;
}

void Emitter::selfTest() {
    aout << "[particle] self-test starting" << std::endl;

    {
        Emitter e(4);
        assert(e.activeCount() == 0);
        assert(!e.isAlive());
        e.emit(math::Vec3(0.0f), math::Vec3(1.0f, 0.0f, 0.0f),
               math::Vec4(1.0f), 0.5f, 1.0f);
        assert(e.activeCount() == 1);
        assert(e.isAlive());
    }

    {
        Emitter e(3);
        e.emit({0,0,0}, {1,0,0}, {1,1,1,1}, 1.0f, 1.0f);
        e.emit({1,0,0}, {0,1,0}, {1,0,0,1}, 0.5f, 2.0f);
        e.emit({0,1,1}, {0,0,1}, {0,1,0,1}, 0.3f, 0.5f);
        assert(e.activeCount() == 3);
        e.update(0.6f);
        assert(e.activeCount() == 2);
    }

    {
        Emitter e(10);
        e.burst({0,0,0}, 5, 2.0f, {1,1,0,1}, 0.3f, 1.0f);
        assert(e.activeCount() == 5);
        for (int i = 0; i < 20; ++i) e.update(0.1f);
        assert(e.activeCount() == 0);
    }

    {
        Emitter e(4);
        e.burst({0,0,0}, 10, 1.0f, {1,1,1,1}, 0.5f, 1.0f);
        assert(e.activeCount() == 4);
    }

    {
        Emitter e(2);
        e.emit({0,0,0}, {1,0,0}, {1,0,0,1}, 1.0f, 1.0f);
        e.update(0.1f);
        e.clear();
        assert(e.activeCount() == 0);
        assert(!e.isAlive());
    }

    {
        Emitter e(1);
        e.emit({0,0,0}, {0,0,0}, {1,1,1,1}, 1.0f, 1.0f);
        e.update(0.5f);
        assert(e.activeCount() == 1);
        assert(e.particles()[0].color.w < 0.6f);
        assert(e.particles()[0].size < 0.8f);
    }

    {
        Emitter e(1);
        e.emit({0,0,0}, {0,0,0}, {1,1,1,0.5f}, 1.0f, 1.0f);
        e.update(0.5f);
        assert(math::approxEqual(e.particles()[0].color.w, 0.25f, 1e-5f));
        assert(math::approxEqual(e.particles()[0].size, 0.75f, 1e-5f));
    }

    {
        Emitter a(1), b(1);
        a.emit({0,0,0}, {0,0,0}, {1,1,1,1}, 1.0f, 1.0f);
        b.emit({0,0,0}, {0,0,0}, {1,1,1,1}, 1.0f, 1.0f);
        a.update(0.5f);
        for (int i = 0; i < 50; ++i) b.update(0.01f);
        assert(math::approxEqual(a.particles()[0].size, b.particles()[0].size, 1e-4f));
        assert(math::approxEqual(a.particles()[0].color.w, b.particles()[0].color.w, 1e-4f));
    }

    // Setters / config round-trip
    {
        Emitter e(4);
        e.setUpVelocityMult(3.0f);
        assert(math::approxEqual(e.upVelocityMult(), 3.0f, 1e-6f));
        e.setSizeJitter(0.25f);
        assert(math::approxEqual(e.sizeJitter(), 0.25f, 1e-6f));
        e.setLifetimeJitter(0.7f, 0.4f);
        assert(math::approxEqual(e.lifetimeJitterMin(), 0.7f, 1e-6f));
        assert(math::approxEqual(e.lifetimeJitterRange(), 0.4f, 1e-6f));
        e.setSizeShrinkFactor(0.8f);
        assert(math::approxEqual(e.sizeShrinkFactor(), 0.8f, 1e-6f));
        e.setGravity(math::Vec3(0.0f, -9.0f, 0.0f));
        assert(math::approxEqual(e.gravity().y, -9.0f, 1e-6f));
    }

    // Zero jitter: burst with sizeJitter=0 + lifetimeJitter(1.0, 0.0) → no variation
    {
        Emitter e(4);
        e.setSizeJitter(0.0f);
        e.setLifetimeJitter(1.0f, 0.0f);
        e.burst({0,0,0}, 4, 1.0f, {1,1,1,1}, 0.5f, 1.0f);
        assert(e.activeCount() == 4);
        for (int i = 0; i < 4; ++i) {
            assert(math::approxEqual(e.particles()[i].size, 0.5f, 1e-6f));
            assert(math::approxEqual(e.particles()[i].lifetime, 1.0f, 1e-6f));
        }
    }

    // setConfig bulk round-trip
    {
        Emitter e(2);
        Emitter::Config cfg;
        cfg.gravity = math::Vec3(0.0f, -5.0f, 0.0f);
        cfg.upVelocityMult = 2.0f;
        cfg.sizeJitter = 0.05f;
        cfg.lifetimeJitterMin = 0.8f;
        cfg.lifetimeJitterRange = 0.2f;
        cfg.sizeShrinkFactor = 0.3f;
        e.setConfig(cfg);
        const auto& c = e.config();
        assert(math::approxEqual(c.gravity.y, -5.0f, 1e-6f));
        assert(math::approxEqual(c.upVelocityMult, 2.0f, 1e-6f));
        assert(math::approxEqual(c.sizeJitter, 0.05f, 1e-6f));
        assert(math::approxEqual(c.lifetimeJitterMin, 0.8f, 1e-6f));
        assert(math::approxEqual(c.lifetimeJitterRange, 0.2f, 1e-6f));
        assert(math::approxEqual(c.sizeShrinkFactor, 0.3f, 1e-6f));
    }

    aout << "[particle] self-test PASS" << std::endl;
}

} // namespace particle
