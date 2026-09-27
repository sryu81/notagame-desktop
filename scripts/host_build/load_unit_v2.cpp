#include "GltfLoader.h"
#include "Animation.h"
#include "Bone.h"
#include "Math.h"
#include "AndroidOut.h"
#include <cassert>
#include <cmath>
#include <cstdio>

#ifndef UNIT_V2_GLB_PATH
#error "define UNIT_V2_GLB_PATH"
#endif

int main() {
    FILE* f = fopen(UNIT_V2_GLB_PATH, "rb");
    if (!f) { fprintf(stderr, "open failed: %s\n", UNIT_V2_GLB_PATH); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    auto* data = new unsigned char[sz];
    fread(data, 1, sz, f);
    fclose(f);

    gltf::LoadResult r;
    bool ok = gltf::load(data, static_cast<size_t>(sz), r);
    delete[] data;
    assert(ok && "gltf::load returned false");

    // skeleton: 3 bones in a chain leg→torso→head
    assert(r.skeleton.bones.size() == 3 && "expected 3 bones");
    assert(r.skeleton.bones[0].parent == -1 && "leg is root");
    assert(r.skeleton.bones[1].parent ==  0 && "torso parent = leg");
    assert(r.skeleton.bones[2].parent ==  1 && "head parent = torso");

    // 2 clips: idle + attack
    assert(r.clips.size() == 2 && "expected 2 clips");

    const anim::SkeletalClip* idle = nullptr;
    for (auto& [name, clip] : r.clips)
        if (name == "idle") { idle = &clip; break; }
    assert(idle && "idle clip not found");
    assert(!idle->tracks.empty() && "idle has no tracks");

    // sampleSkeletal(idle, 0.5, 3) → bone 1 (torso) Y ≈ 0.15
    auto mats = anim::sampleSkeletal(*idle, 0.5f, 3);
    float torsoY = mats[1].m[13];
    assert(std::abs(torsoY - 0.15f) < 1e-4f && "torso Y at t=0.5 should be 0.15");

    // mesh has bone data
    assert(!r.mesh.boneIndices.empty() && "mesh missing bone indices");
    assert(!r.mesh.boneWeights.empty() && "mesh missing bone weights");
    assert(r.mesh.positions.size() == 6 && "expected 6 verts");
    assert(r.mesh.indices.size()  == 12 && "expected 12 indices");

    fprintf(stderr, "A2 PASS: 3 bones, 2 clips, idle torsoY(0.5)=%.4f, mesh verts=%zu\n",
            torsoY, r.mesh.positions.size());
    return 0;
}
