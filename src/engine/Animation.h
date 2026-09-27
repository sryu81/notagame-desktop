#ifndef NOT_A_GAME_ANIMATION_H
#define NOT_A_GAME_ANIMATION_H

#include <functional>
#include <string>
#include <vector>

#include "Bone.h"
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
    std::vector<AnimEvent> events;
    bool loop = true;
};

float duration(const SkeletalClip &clip);
float wrapTime(const SkeletalClip &clip, float time);
bool isLoopClipName(const std::string &name);
void crossedEvents(const std::vector<AnimEvent> &events, float duration,
                   float prevWrapped, float nowWrapped, std::vector<int> &outIds);

std::vector<math::Mat4> sampleSkeletal(const SkeletalClip &clip, float time, int boneCount);
std::vector<math::Mat4> sampleSkeletal(const SkeletalClip &clip, float time, const Skeleton &skel);

void selfTest();

void skeletalSelfTest();

} // namespace anim

#endif
