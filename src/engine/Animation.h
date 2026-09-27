#ifndef NOT_A_GAME_ANIMATION_H
#define NOT_A_GAME_ANIMATION_H

#include <functional>
#include <vector>

#include "Math.h"

namespace anim {

struct Keyframe {
    float time = 0.0f;
    math::Vec3 translation{0.0f, 0.0f, 0.0f};
    math::Quat rotation = math::Quat::identity();
    math::Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct AnimEvent {
    float time = 0.0f;
    int id = 0;
};

struct Clip {
    std::vector<Keyframe> keyframes;
    std::vector<AnimEvent> events;
    bool loop = true;
};

math::Mat4 sample(const Clip &clip, float time);

float wrapTime(const Clip &clip, float time);

struct BoneTrack {
    int boneIndex = -1;
    std::vector<Keyframe> keyframes;
};

struct SkeletalClip {
    std::vector<BoneTrack> tracks;
    bool loop = true;
};

std::vector<math::Mat4> sampleSkeletal(const SkeletalClip &clip, float time, int boneCount);

void selfTest();

void skeletalSelfTest();

} // namespace anim

#endif
