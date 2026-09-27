#ifndef NOT_A_GAME_MESH_H
#define NOT_A_GAME_MESH_H

#include <array>
#include <cstdint>
#include <vector>

#include "Math.h"

struct Mesh {
    std::vector<math::Vec3> positions;
    std::vector<math::Vec2> uvs;
    std::vector<math::Vec3> normals;
    std::vector<uint32_t> indices;
    std::vector<std::array<uint8_t, 4>> boneIndices;
    std::vector<std::array<float, 4>> boneWeights;

    bool hasBones() const {
        return !boneIndices.empty() && boneIndices.size() == positions.size();
    }

    math::Vec3 boundingSphereCenter() const;
    float boundingSphereRadius() const;

    static void selfTest();
};

#endif
