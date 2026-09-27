#include "Animation.h"

#include "AndroidOut.h"

#include <cassert>
#include <cmath>

namespace anim {

float wrapTime(const Clip &clip, float time) {
    if (clip.keyframes.empty()) return 0.0f;
    const float last = clip.keyframes.back().time;
    if (last <= 0.0f) return 0.0f;
    if (clip.loop) {
        float t = std::fmod(time, last);
        if (t < 0.0f) t += last;
        return t;
    }
    if (time < 0.0f) return 0.0f;
    if (time > last) return last;
    return time;
}

static float wrapTimeSimple(const std::vector<Keyframe> &keyframes, float time, bool loop) {
    if (keyframes.empty()) return 0.0f;
    const float last = keyframes.back().time;
    if (last <= 0.0f) return 0.0f;
    if (loop) {
        float t = std::fmod(time, last);
        if (t < 0.0f) t += last;
        return t;
    }
    if (time < 0.0f) return keyframes.front().time;
    if (time > last) return keyframes.back().time;
    return time;
}

math::Mat4 sample(const Clip &clip, float time) {
    if (clip.keyframes.empty()) return math::Mat4::identity();
    if (clip.keyframes.size() == 1) {
        const auto &k = clip.keyframes[0];
        return math::Mat4::translation(k.translation)
               * math::Mat4::rotation(k.rotation)
               * math::Mat4::scale(k.scale);
    }

    const float t = wrapTime(clip, time);

    size_t i1 = 1;
    while (i1 < clip.keyframes.size() && clip.keyframes[i1].time < t) ++i1;
    if (i1 >= clip.keyframes.size()) i1 = clip.keyframes.size() - 1;
    const size_t i0 = (i1 == 0) ? 0 : i1 - 1;
    const Keyframe &k0 = clip.keyframes[i0];
    const Keyframe &k1 = clip.keyframes[i1];

    float alpha = 0.0f;
    const float span = k1.time - k0.time;
    if (span > 0.0f) alpha = (t - k0.time) / span;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    math::Vec3 t3 = k0.translation * (1.0f - alpha) + k1.translation * alpha;
    math::Quat q = math::Quat::slerp(k0.rotation, k1.rotation, alpha);
    math::Vec3 s3 = k0.scale * (1.0f - alpha) + k1.scale * alpha;

    return math::Mat4::translation(t3)
           * math::Mat4::rotation(q)
           * math::Mat4::scale(s3);
}

static math::Mat4 sampleKeyframes(const std::vector<Keyframe> &keyframes, float time, bool loop) {
    if (keyframes.empty()) return math::Mat4::identity();
    if (keyframes.size() == 1) {
        const auto &k = keyframes[0];
        return math::Mat4::translation(k.translation)
               * math::Mat4::rotation(k.rotation)
               * math::Mat4::scale(k.scale);
    }

    const float t = wrapTimeSimple(keyframes, time, loop);

    size_t i1 = 1;
    while (i1 < keyframes.size() && keyframes[i1].time < t) ++i1;
    if (i1 >= keyframes.size()) i1 = keyframes.size() - 1;
    const size_t i0 = (i1 == 0) ? 0 : i1 - 1;
    const Keyframe &k0 = keyframes[i0];
    const Keyframe &k1 = keyframes[i1];

    float alpha = 0.0f;
    const float span = k1.time - k0.time;
    if (span > 0.0f) alpha = (t - k0.time) / span;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    math::Vec3 t3 = k0.translation * (1.0f - alpha) + k1.translation * alpha;
    math::Quat q = math::Quat::slerp(k0.rotation, k1.rotation, alpha);
    math::Vec3 s3 = k0.scale * (1.0f - alpha) + k1.scale * alpha;

    return math::Mat4::translation(t3) * math::Mat4::rotation(q) * math::Mat4::scale(s3);
}

std::vector<math::Mat4> sampleSkeletal(const SkeletalClip &clip, float time, int boneCount) {
    std::vector<math::Mat4> result(boneCount, math::Mat4::identity());
    for (const auto &track : clip.tracks) {
        if (track.boneIndex < 0 || track.boneIndex >= boneCount) continue;
        result[track.boneIndex] = sampleKeyframes(track.keyframes, time, clip.loop);
    }
    return result;
}

std::vector<math::Mat4> sampleSkeletal(const SkeletalClip &clip, float time, const Skeleton &skel) {
    const int boneCount = static_cast<int>(skel.bones.size());
    std::vector<math::Mat4> result(boneCount);
    for (int i = 0; i < boneCount; ++i) result[i] = skel.bones[i].restLocal;
    for (const auto &track : clip.tracks) {
        if (track.boneIndex < 0 || track.boneIndex >= boneCount) continue;
        result[track.boneIndex] = sampleKeyframes(track.keyframes, time, clip.loop);
    }
    return result;
}

void selfTest() {
    aout << "[anim] self-test starting" << std::endl;

    // 1. Empty clip → identity.
    {
        Clip clip;
        const math::Mat4 m = sample(clip, 0.0f);
        assert(math::approxEqual(m.m[0], 1.0f, 1e-5f));
        assert(math::approxEqual(m.m[15], 1.0f, 1e-5f));
    }

    // 2. Single keyframe → exact TRS.
    {
        Clip clip;
        clip.keyframes.push_back(
            {0.0f,
             math::Vec3(1.0f, 2.0f, 3.0f),
             math::Quat::identity(),
             math::Vec3(2.0f, 2.0f, 2.0f)});
        const math::Mat4 m = sample(clip, 0.0f);
        assert(math::approxEqual(m.m[12], 1.0f));
        assert(math::approxEqual(m.m[13], 2.0f));
        assert(math::approxEqual(m.m[14], 3.0f));
        assert(math::approxEqual(m.m[0], 2.0f));
        assert(math::approxEqual(m.m[5], 2.0f));
        assert(math::approxEqual(m.m[10], 2.0f));
    }

    // 3. Two keyframes lerp at t=0.5.
    {
        Clip clip;
        clip.keyframes.push_back(
            {0.0f, math::Vec3(0.0f, 0.0f, 0.0f), math::Quat::identity(), math::Vec3(1.0f)});
        clip.keyframes.push_back(
            {1.0f, math::Vec3(2.0f, 0.0f, 0.0f), math::Quat::identity(), math::Vec3(3.0f)});
        const math::Mat4 m = sample(clip, 0.5f);
        assert(math::approxEqual(m.m[12], 1.0f, 1e-4f));
        assert(math::approxEqual(m.m[13], 0.0f, 1e-4f));
        assert(math::approxEqual(m.m[14], 0.0f, 1e-4f));
        assert(math::approxEqual(m.m[0], 2.0f, 1e-4f));
        assert(math::approxEqual(m.m[5], 2.0f, 1e-4f));
        assert(math::approxEqual(m.m[10], 2.0f, 1e-4f));
    }

    // 4. wrapTime: t=1.5 with period 1.0 wraps to 0.5.
    {
        Clip clip;
        clip.keyframes.push_back({0.0f, math::Vec3(), math::Quat::identity(), math::Vec3(1.0f)});
        clip.keyframes.push_back({1.0f, math::Vec3(10.0f, 0.0f, 0.0f), math::Quat::identity(), math::Vec3(1.0f)});
        clip.loop = true;
        assert(math::approxEqual(wrapTime(clip, 1.5f), 0.5f, 1e-5f));
        assert(math::approxEqual(wrapTime(clip, -0.3f), 0.7f, 1e-5f));
    }

    // 5. wrapTime: non-looping clamps.
    {
        Clip clip;
        clip.keyframes.push_back({0.0f, math::Vec3(), math::Quat::identity(), math::Vec3(1.0f)});
        clip.keyframes.push_back({2.0f, math::Vec3(), math::Quat::identity(), math::Vec3(1.0f)});
        clip.loop = false;
        assert(math::approxEqual(wrapTime(clip, -1.0f), 0.0f, 1e-5f));
        assert(math::approxEqual(wrapTime(clip, 5.0f), 2.0f, 1e-5f));
    }

    // 6. sample at t beyond end of non-looping clip returns last keyframe's TRS.
    {
        Clip clip;
        clip.keyframes.push_back({0.0f, math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)});
        clip.keyframes.push_back({1.0f, math::Vec3(0.0f, 0.0f, 5.0f), math::Quat::identity(), math::Vec3(1.0f)});
        clip.loop = false;
        const math::Mat4 m = sample(clip, 100.0f);
        assert(math::approxEqual(m.m[14], 5.0f, 1e-4f));
    }

    // 7. Full-revolution spin test.
    {
        Clip clip;
        clip.loop = true;
        const math::Vec3 yAxis(0.0f, 1.0f, 0.0f);
        clip.keyframes.push_back({0.0f, math::Vec3(0.0f), math::Quat::identity(), math::Vec3(1.0f)});
        clip.keyframes.push_back({1.0f, math::Vec3(0.0f), math::Quat::fromAxisAngle(yAxis, 0.5f * math::PI), math::Vec3(1.0f)});
        clip.keyframes.push_back({2.0f, math::Vec3(0.0f), math::Quat::fromAxisAngle(yAxis, 1.0f * math::PI), math::Vec3(1.0f)});
        clip.keyframes.push_back({3.0f, math::Vec3(0.0f), math::Quat::fromAxisAngle(yAxis, 1.5f * math::PI), math::Vec3(1.0f)});
        clip.keyframes.push_back({4.0f, math::Vec3(0.0f), math::Quat::fromAxisAngle(yAxis, 2.0f * math::PI), math::Vec3(1.0f)});

        const math::Vec3 p(1.0f, 0.0f, 0.0f);
        const math::Mat4 m0 = sample(clip, 0.0f);
        const math::Mat4 m2 = sample(clip, 2.0f);
        const math::Mat4 m4 = sample(clip, 4.0f);
        const math::Vec4 pAt0 = m0 * math::Vec4(p.x, p.y, p.z, 1.0f);
        const math::Vec4 pAt2 = m2 * math::Vec4(p.x, p.y, p.z, 1.0f);
        const math::Vec4 pAt4 = m4 * math::Vec4(p.x, p.y, p.z, 1.0f);
        assert(math::approxEqual(pAt0.x, 1.0f, 1e-4f));
        assert(math::approxEqual(pAt0.z, 0.0f, 1e-4f));
        assert(math::approxEqual(pAt2.x, -1.0f, 1e-4f));
        assert(math::approxEqual(pAt2.z, 0.0f, 1e-4f));
        assert(math::approxEqual(pAt4.x, 1.0f, 1e-4f));
        assert(math::approxEqual(pAt4.z, 0.0f, 1e-4f));
    }

    aout << "[anim] self-test PASS" << std::endl;
}

void skeletalSelfTest() {
    aout << "[skeletal] self-test starting" << std::endl;

    // 1. Empty SkeletalClip → identity for all bones.
    {
        SkeletalClip clip;
        clip.loop = true;
        auto result = sampleSkeletal(clip, 0.5f, 3);
        assert(result.size() == 3);
        for (int i = 0; i < 3; ++i)
            assert(math::approxEqual(result[i].m[0], 1.0f, 1e-5f));
    }

    // 2. Single bone track interpolates correctly.
    {
        SkeletalClip clip;
        clip.tracks.push_back({0, {
            {0.0f, math::Vec3(0.0f, 0.0f, 0.0f), math::Quat::identity(), math::Vec3(1.0f)},
            {1.0f, math::Vec3(5.0f, 0.0f, 0.0f), math::Quat::identity(), math::Vec3(1.0f)}}});
        clip.loop = true;
        auto result = sampleSkeletal(clip, 0.5f, 2);
        assert(result.size() == 2);
        assert(math::approxEqual(result[0].m[12], 2.5f, 1e-4f));
        assert(math::approxEqual(result[1].m[12], 0.0f, 1e-4f));
    }

    // 3. Looping wraps correctly.
    {
        SkeletalClip clip;
        clip.tracks.push_back({0, {
            {0.0f, math::Vec3(0.0f, 0.0f, 0.0f), math::Quat::identity(), math::Vec3(1.0f)},
            {2.0f, math::Vec3(10.0f, 0.0f, 0.0f), math::Quat::identity(), math::Vec3(1.0f)}}});
        clip.loop = true;
        auto result = sampleSkeletal(clip, 3.0f, 1);
        assert(math::approxEqual(result[0].m[12], 5.0f, 1e-4f));
    }

    // 4. Out-of-range boneIndex is skipped.
    {
        SkeletalClip clip;
        clip.tracks.push_back({99, {
            {0.0f, math::Vec3(1.0f, 2.0f, 3.0f), math::Quat::identity(), math::Vec3(1.0f)}}});
        clip.loop = true;
        auto result = sampleSkeletal(clip, 0.0f, 2);
        assert(result.size() == 2);
        assert(math::approxEqual(result[0].m[12], 0.0f, 1e-5f));
    }

    {
        Skeleton skel;
        skel.bones.push_back({-1, math::Mat4::identity(), math::Mat4::translation(math::Vec3(0.0f, 2.0f, 0.0f))});
        skel.bones.push_back({0, math::Mat4::identity(), math::Mat4::translation(math::Vec3(0.0f, 1.5f, 0.0f))});
        SkeletalClip clip;
        clip.tracks.push_back({0, {
            {0.0f, math::Vec3(1.0f, 0.0f, 0.0f), math::Quat::identity(), math::Vec3(1.0f)}}});
        auto result = sampleSkeletal(clip, 0.0f, skel);
        assert(result.size() == 2);
        assert(math::approxEqual(result[0].m[12], 1.0f, 1e-5f));
        assert(math::approxEqual(result[1].m[13], 1.5f, 1e-5f));
    }

    aout << "[skeletal] self-test PASS" << std::endl;
}

} // namespace anim
