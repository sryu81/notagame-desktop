#include "Bone.h"

#include <cassert>
#include <cmath>

#include "AndroidOut.h"

namespace anim {

std::vector<math::Mat4> computeBoneMatrices(
    const Skeleton& skel,
    const std::vector<math::Mat4>& perBoneLocal) {
    const int n = static_cast<int>(skel.bones.size());
    assert(n > 0);
    assert(static_cast<int>(perBoneLocal.size()) == n);

    std::vector<math::Mat4> modelSpace(n);
    for (int i = 0; i < n; ++i) {
        if (skel.bones[i].parent >= 0) {
            const int p = skel.bones[i].parent;
            assert(p < i);
            modelSpace[i] = modelSpace[p] * perBoneLocal[i];
        } else {
            modelSpace[i] = perBoneLocal[i];
        }
    }

    std::vector<math::Mat4> result(n);
    for (int i = 0; i < n; ++i) {
        result[i] = modelSpace[i] * skel.bones[i].invBindPose;
    }
    return result;
}

void skinMesh(
    const std::vector<math::Vec3>& inPositions,
    const std::vector<math::Vec3>& inNormals,
    const std::vector<std::array<uint8_t, 4>>& boneIndices,
    const std::vector<std::array<float, 4>>& boneWeights,
    const std::vector<math::Mat4>& boneMatrices,
    std::vector<math::Vec3>& outPositions,
    std::vector<math::Vec3>& outNormals) {
    const size_t n = inPositions.size();
    assert(n == inNormals.size());
    assert(n == boneIndices.size());
    assert(n == boneWeights.size());

    outPositions.resize(n);
    outNormals.resize(n);

    for (size_t v = 0; v < n; ++v) {
        math::Vec3 pos(0.0f, 0.0f, 0.0f);
        math::Vec3 nrm(0.0f, 0.0f, 0.0f);
        float totalWeight = 0.0f;
        for (int i = 0; i < 4; ++i) {
            float w = boneWeights[v][i];
            if (w < 1e-6f) continue;
            uint8_t idx = boneIndices[v][i];
            totalWeight += w;
            const math::Mat4& m = boneMatrices[idx];
            const math::Vec4 p4 = m * math::Vec4(inPositions[v].x, inPositions[v].y, inPositions[v].z, 1.0f);
            pos.x += w * p4.x;
            pos.y += w * p4.y;
            pos.z += w * p4.z;
            const math::Vec3 n4(
                m.m[0] * inNormals[v].x + m.m[4] * inNormals[v].y + m.m[8]  * inNormals[v].z,
                m.m[1] * inNormals[v].x + m.m[5] * inNormals[v].y + m.m[9]  * inNormals[v].z,
                m.m[2] * inNormals[v].x + m.m[6] * inNormals[v].y + m.m[10] * inNormals[v].z);
            nrm.x += w * n4.x;
            nrm.y += w * n4.y;
            nrm.z += w * n4.z;
        }
        if (totalWeight > 0.0f) {
            pos = pos * (1.0f / totalWeight);
            nrm = nrm * (1.0f / totalWeight);
        }
        outPositions[v] = pos;
        outNormals[v] = nrm.normalized();
    }
}

void skinningSelfTest() {
    aout << "[skinning] self-test starting" << std::endl;

    // 1. Single bone identity skeleton.
    {
        Skeleton skel;
        skel.bones.push_back({-1, math::Mat4::identity()});

        std::vector<math::Mat4> local = {math::Mat4::identity()};
        std::vector<math::Mat4> mats = computeBoneMatrices(skel, local);
        assert(mats.size() == 1);
        assert(math::approxEqual(mats[0].m[0], 1.0f));
    }

    // 2. Two-bone hierarchy.
    {
        Skeleton skel;
        skel.bones.push_back({-1, math::Mat4::identity()});
        skel.bones.push_back({0,  math::Mat4::identity()});

        std::vector<math::Mat4> local = {
            math::Mat4::identity(),
            math::Mat4::translation(math::Vec3(1.0f, 0.0f, 0.0f))};
        std::vector<math::Mat4> mats = computeBoneMatrices(skel, local);
        assert(mats.size() == 2);
        assert(math::approxEqual(mats[0].m[12], 0.0f));
        assert(math::approxEqual(mats[1].m[12], 1.0f));
    }

    // 3. InvBindPose cancels bone local at rest pose.
    {
        Skeleton skel;
        skel.bones.push_back({-1,
            math::Mat4::translation(math::Vec3(2.0f, 0.0f, 0.0f)).inverted()});

        std::vector<math::Mat4> local = {
            math::Mat4::translation(math::Vec3(2.0f, 0.0f, 0.0f))};
        std::vector<math::Mat4> mats = computeBoneMatrices(skel, local);
        assert(mats.size() == 1);
        assert(math::approxEqual(mats[0].m[12], 0.0f, 1e-4f));
    }

    // 4. skinMesh: single bone identity, output equals input.
    {
        std::vector<math::Vec3> pos = {math::Vec3(1.0f, 2.0f, 3.0f)};
        std::vector<math::Vec3> nrm = {math::Vec3(0.0f, 0.0f, 1.0f)};
        std::vector<std::array<uint8_t, 4>> bi = {{{0, 0, 0, 0}}};
        std::vector<std::array<float, 4>> bw = {{{1.0f, 0.0f, 0.0f, 0.0f}}};
        std::vector<math::Mat4> bm = {math::Mat4::identity()};

        std::vector<math::Vec3> outPos, outNrm;
        skinMesh(pos, nrm, bi, bw, bm, outPos, outNrm);
        assert(outPos.size() == 1);
        assert(math::approxEqual(outPos[0].x, 1.0f));
        assert(math::approxEqual(outPos[0].y, 2.0f));
        assert(math::approxEqual(outPos[0].z, 3.0f));
    }

    // 5. skinMesh: weighted blend between two bones.
    {
        // Vert at origin, bone 0 translates +2 on X, bone 1 translates +10 on X.
        // Weighted 0.5/0.5 → result at (6, 0, 0).
        std::vector<math::Vec3> pos = {math::Vec3(0.0f, 0.0f, 0.0f)};
        std::vector<math::Vec3> nrm = {math::Vec3(0.0f, 1.0f, 0.0f)};
        std::vector<std::array<uint8_t, 4>> bi = {{{0, 1, 0, 0}}};
        std::vector<std::array<float, 4>> bw = {{{0.5f, 0.5f, 0.0f, 0.0f}}};
        std::vector<math::Mat4> bm = {
            math::Mat4::translation(math::Vec3(2.0f, 0.0f, 0.0f)),
            math::Mat4::translation(math::Vec3(10.0f, 0.0f, 0.0f))};

        std::vector<math::Vec3> outPos, outNrm;
        skinMesh(pos, nrm, bi, bw, bm, outPos, outNrm);
        assert(outPos.size() == 1);
        assert(math::approxEqual(outPos[0].x, 6.0f, 1e-4f));
    }

    aout << "[skinning] self-test PASS" << std::endl;
}

} // namespace anim
