#include "GltfLoader.h"
#include "Animation.h"
#include "Bone.h"
#include "Mesh.h"
#include "Math.h"
#include "AndroidOut.h"
#include <cstdio>
#include <cmath>

int main() {
    FILE* f = fopen("/tmp/opencode/gltftest/minimal_rigged.glb", "rb");
    if (!f) { fprintf(stderr, "open failed\n"); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* data = new unsigned char[size];
    fread(data, 1, size, f);
    fclose(f);

    gltf::LoadResult r;
    bool ok = gltf::load(data, size, r);
    fprintf(stderr, "load() returned %s\n", ok ? "true" : "false");
    if (!ok) { delete[] data; return 2; }

    fprintf(stderr, "mesh.positions=%zu  indices=%zu  boneIndices=%zu  boneWeights=%zu\n",
            r.mesh.positions.size(), r.mesh.indices.size(),
            r.mesh.boneIndices.size(), r.mesh.boneWeights.size());
    fprintf(stderr, "skeleton.bones=%zu\n", r.skeleton.bones.size());
    for (size_t i = 0; i < r.skeleton.bones.size(); ++i) {
        fprintf(stderr, "  bone[%zu] parent=%d  invBind=(%.1f,%.1f,%.1f,%.1f)\n",
                i, r.skeleton.bones[i].parent,
                r.skeleton.bones[i].invBindPose.m[0],
                r.skeleton.bones[i].invBindPose.m[5],
                r.skeleton.bones[i].invBindPose.m[10],
                r.skeleton.bones[i].invBindPose.m[15]);
    }
    fprintf(stderr, "clips=%zu\n", r.clips.size());
    for (auto& [name, clip] : r.clips) {
        fprintf(stderr, "  clip '%s' loop=%d tracks=%zu\n",
                name.c_str(), (int)clip.loop, clip.tracks.size());
        for (auto& t : clip.tracks) {
            fprintf(stderr, "    track bone=%d keyframes=%zu\n",
                    t.boneIndex, t.keyframes.size());
            for (auto& kf : t.keyframes) {
                fprintf(stderr, "      t=%.2f  T=(%.2f,%.2f,%.2f)  R=(%.2f,%.2f,%.2f,%.2f)  S=(%.2f,%.2f,%.2f)\n",
                        kf.time,
                        kf.translation.x, kf.translation.y, kf.translation.z,
                        kf.rotation.x, kf.rotation.y, kf.rotation.z, kf.rotation.w,
                        kf.scale.x, kf.scale.y, kf.scale.z);
            }
        }
    }

    if (!r.clips.empty() && !r.clips[0].second.tracks.empty()) {
        std::vector<math::Mat4> sampled = anim::sampleSkeletal(
            r.clips[0].second, 0.5f, static_cast<int>(r.skeleton.bones.size()));
        fprintf(stderr, "sampled at t=0.5:\n");
        for (size_t i = 0; i < sampled.size(); ++i) {
            math::Vec3 t(sampled[i].m[12], sampled[i].m[13], sampled[i].m[14]);
            fprintf(stderr, "  bone[%zu].translation=(%.4f, %.4f, %.4f)\n", i, t.x, t.y, t.z);
        }
        math::Vec3 b0(sampled[0].m[12], sampled[0].m[13], sampled[0].m[14]);
        if (std::abs(b0.x - 0.25f) < 1e-4f &&
            std::abs(b0.y - 0.0f) < 1e-4f &&
            std::abs(b0.z - 0.0f) < 1e-4f) {
            fprintf(stderr, "ANIM INTERP PASS\n");
        } else {
            fprintf(stderr, "ANIM INTERP FAIL: expected (0.25, 0, 0), got (%.4f, %.4f, %.4f)\n",
                    b0.x, b0.y, b0.z);
            delete[] data; return 3;
        }
    }

    delete[] data;
    fprintf(stderr, "ALL PASS\n");
    return 0;
}
