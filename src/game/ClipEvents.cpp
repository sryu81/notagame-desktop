#include "ClipEvents.h"

#include <cassert>
#include <cctype>

#include "Engine.h"

namespace game {

namespace {

struct Entry { const char* clip; float time; int id; };

const Entry kTable[] = {
    {"attack", 0.35f, kEventHit},
    {"walk",   0.0f,  kEventFootL},
    {"walk",   0.5f,  kEventFootR},
};

std::string lower(const std::string& s) {
    std::string out;
    for (char c : s) out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

}

void applyClipEvents(const std::string& clipName, anim::SkeletalClip& clip) {
    const std::string key = lower(clipName);
    clip.events.clear();
    for (const auto& e : kTable)
        if (key == e.clip) clip.events.push_back({e.time, e.id});
}

void clipEventsSelfTest() {
    aout << "[clip-events] self-test starting" << std::endl;
    anim::SkeletalClip walk, attack, idle;
    applyClipEvents("Walk", walk);
    applyClipEvents("Attack", attack);
    applyClipEvents("Idle", idle);
    assert(walk.events.size() == 2 && walk.events[1].id == kEventFootR);
    assert(attack.events.size() == 1 && attack.events[0].time == 0.35f && attack.events[0].id == kEventHit);
    assert(idle.events.empty());
    applyClipEvents("Walk", walk);
    assert(walk.events.size() == 2);
    aout << "[clip-events] self-test PASS" << std::endl;
}

} // namespace game
