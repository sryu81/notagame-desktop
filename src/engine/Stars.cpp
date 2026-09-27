#include "Stars.h"

#include "AndroidOut.h"

#include <cassert>
#include <cmath>
#include <cstring>

namespace engine {
namespace stars {

namespace {

constexpr uint32_t kMagic   = 0x53545253u;
constexpr uint32_t kVersion = 1u;
constexpr size_t   kHeader  = 12;
constexpr size_t   kStarSize = 20;

float readF32(const uint8_t* p) {
    float v;
    std::memcpy(&v, p, 4);
    return v;
}

}

bool loadStars(const uint8_t* bytes, size_t size, StarField& out) {
    out.stars.clear();
    if (!bytes || size < kHeader) return false;
    uint32_t magic, count, version;
    std::memcpy(&magic,   bytes + 0, 4);
    std::memcpy(&count,   bytes + 4, 4);
    std::memcpy(&version, bytes + 8, 4);
    if (magic != kMagic) return false;
    if (version != kVersion) return false;
    const size_t expected = kHeader + static_cast<size_t>(count) * kStarSize;
    if (size < expected) return false;
    out.stars.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t* p = bytes + kHeader + i * kStarSize;
        Star s;
        s.dir.x = readF32(p + 0);
        s.dir.y = readF32(p + 4);
        s.dir.z = readF32(p + 8);
        s.magnitude = readF32(p + 12);
        s.color.x = p[16] / 255.0f;
        s.color.y = p[17] / 255.0f;
        s.color.z = p[18] / 255.0f;
        out.stars.push_back(s);
    }
    return true;
}

void selfTest() {
    aout << "[stars] self-test starting" << std::endl;

    {
        StarField f;
        assert(!loadStars(nullptr, 0, f));
        assert(f.stars.empty());
    }

    {
        uint8_t buf[kHeader + 2 * kStarSize] = {};
        uint32_t magic = kMagic;
        uint32_t count = 2;
        uint32_t version = kVersion;
        std::memcpy(buf + 0, &magic, 4);
        std::memcpy(buf + 4, &count, 4);
        std::memcpy(buf + 8, &version, 4);
        float dir1[3] = {1.0f, 0.0f, 0.0f};
        float mag1    = -0.5f;
        std::memcpy(buf + 12 + 0, dir1, 12);
        std::memcpy(buf + 12 + 12, &mag1, 4);
        buf[12 + 16 + 0] = 255;
        buf[12 + 16 + 1] = 220;
        buf[12 + 16 + 2] = 200;
        float dir2[3] = {0.0f, 1.0f, 0.0f};
        float mag2    = 2.5f;
        std::memcpy(buf + 12 + kStarSize + 0, dir2, 12);
        std::memcpy(buf + 12 + kStarSize + 12, &mag2, 4);
        buf[12 + kStarSize + 16 + 0] = 100;
        buf[12 + kStarSize + 16 + 1] = 150;
        buf[12 + kStarSize + 16 + 2] = 255;

        StarField f;
        assert(loadStars(buf, sizeof(buf), f));
        assert(f.stars.size() == 2);
        assert(std::abs(f.stars[0].dir.x - 1.0f) < 1e-6f);
        assert(std::abs(f.stars[0].magnitude - -0.5f) < 1e-6f);
        assert(std::abs(f.stars[0].color.x - 1.0f) < 1e-3f);
        assert(std::abs(f.stars[1].dir.y - 1.0f) < 1e-6f);
        assert(std::abs(f.stars[1].color.z - 1.0f) < 1e-3f);
    }

    {
        uint8_t buf[kHeader] = {};
        uint32_t magic = kMagic, count = 1, version = kVersion;
        std::memcpy(buf, &magic, 4);
        std::memcpy(buf + 4, &count, 4);
        std::memcpy(buf + 8, &version, 4);
        StarField f;
        assert(!loadStars(buf, sizeof(buf), f));
    }

    {
        uint8_t buf[kHeader] = {};
        uint32_t bad = 0xDEADBEEF;
        std::memcpy(buf, &bad, 4);
        StarField f;
        assert(!loadStars(buf, sizeof(buf), f));
    }

    aout << "[stars] self-test PASS" << std::endl;
}

}
}
