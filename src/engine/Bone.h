#ifndef NOT_A_GAME_BONE_H
#define NOT_A_GAME_BONE_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "Math.h"

namespace anim {

struct Bone {
    int parent = -1;
    math::Mat4 invBindPose = math::Mat4::identity();
    math::Mat4 restLocal = math::Mat4::identity();
    std::string name;
};

struct Skeleton {
    std::vector<Bone> bones;
    int find(const std::string& name) const {
        for (size_t i = 0; i < bones.size(); ++i)
            if (bones[i].name == name) return static_cast<int>(i);
        return -1;
    }
};

std::vector<math::Mat4> computeBoneMatrices(
    const Skeleton& skel,
    const std::vector<math::Mat4>& perBoneLocal);

void skinMesh(
    const std::vector<math::Vec3>& inPositions,
    const std::vector<math::Vec3>& inNormals,
    const std::vector<std::array<uint8_t, 4>>& boneIndices,
    const std::vector<std::array<float, 4>>& boneWeights,
    const std::vector<math::Mat4>& boneMatrices,
    std::vector<math::Vec3>& outPositions,
    std::vector<math::Vec3>& outNormals);

void skinningSelfTest();

} // namespace anim

#endif
