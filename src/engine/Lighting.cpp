#include "Lighting.h"

#include <cassert>

#include "AndroidOut.h"

namespace lighting {

float lambert(const math::Vec3 &normal, const math::Vec3 &lightDir) {
    float d = normal.dot(lightDir);
    return d < 0.0f ? 0.0f : d;
}

void selfTest() {
    aout << "[lighting] self-test starting" << std::endl;

    assert(math::approxEqual(lambert(math::Vec3(0.0f, 1.0f, 0.0f), math::Vec3(0.0f, 1.0f, 0.0f)), 1.0f));
    assert(math::approxEqual(lambert(math::Vec3(0.0f, 1.0f, 0.0f), math::Vec3(0.0f, -1.0f, 0.0f)), 0.0f));
    assert(math::approxEqual(lambert(math::Vec3(1.0f, 0.0f, 0.0f), math::Vec3(0.0f, 1.0f, 0.0f)), 0.0f));
    assert(math::approxEqual(lambert(math::Vec3(1.0f, 0.0f, 0.0f), math::Vec3(1.0f, 0.0f, 0.0f)), 1.0f));
    assert(math::approxEqual(
            lambert(math::Vec3(0.70710678f, 0.70710678f, 0.0f), math::Vec3(0.0f, 1.0f, 0.0f)),
            0.70710678f, 1e-5f));

    aout << "[lighting] self-test PASS" << std::endl;
}

}
