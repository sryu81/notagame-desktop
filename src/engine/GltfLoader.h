#ifndef NOT_A_GAME_GLTF_LOADER_H
#define NOT_A_GAME_GLTF_LOADER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

#include "Animation.h"
#include "Bone.h"
#include "Mesh.h"

namespace gltf {

struct LoadResult {
    Mesh mesh;
    anim::Skeleton skeleton;
    std::vector<std::pair<std::string, anim::SkeletalClip>> clips;
};

bool load(const uint8_t* bytes, size_t size, LoadResult& out);

bool sortJointsParentFirst(LoadResult& r);

void selfTest();

} // namespace gltf

#endif
