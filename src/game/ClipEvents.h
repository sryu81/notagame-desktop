#ifndef NOT_A_GAME_CLIP_EVENTS_H
#define NOT_A_GAME_CLIP_EVENTS_H

#include <string>

#include "Animation.h"

namespace game {

inline constexpr int kEventHit   = 1;
inline constexpr int kEventFootL = 2;
inline constexpr int kEventFootR = 3;

void applyClipEvents(const std::string& clipName, anim::SkeletalClip& clip);

void clipEventsSelfTest();

} // namespace game

#endif
