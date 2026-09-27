#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "GltfLoader.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <map>
#include <set>
#include <string>

#include "AndroidOut.h"
#include "Math.h"

namespace gltf {

// ── interpolation helpers ────────────────────────────────────────────────

static math::Vec3 sampleVec3Track(
    const std::vector<float>& times,
    const std::vector<math::Vec3>& values,
    float t,
    math::Vec3 def)
{
    if (times.empty()) return def;
    if (t <= times.front()) return values.front();
    if (t >= times.back())  return values.back();
    for (size_t i = 0; i + 1 < times.size(); ++i) {
        if (t <= times[i + 1]) {
            float alpha = (t - times[i]) / (times[i + 1] - times[i]);
            return math::mix(values[i], values[i + 1], alpha);
        }
    }
    return values.back();
}

static math::Quat sampleQuatTrack(
    const std::vector<float>& times,
    const std::vector<math::Quat>& values,
    float t,
    math::Quat def)
{
    if (times.empty()) return def;
    if (t <= times.front()) return values.front();
    if (t >= times.back())  return values.back();
    for (size_t i = 0; i + 1 < times.size(); ++i) {
        if (t <= times[i + 1]) {
            float alpha = (t - times[i]) / (times[i + 1] - times[i]);
            return math::Quat::nlerp(values[i], values[i + 1], alpha);
        }
    }
    return values.back();
}

// ── mesh ────────────────────────────────────────────────────────────────

static bool extractMesh(cgltf_data* data, Mesh& out) {
    if (data->meshes_count == 0) return false;
    cgltf_primitive* prim = &data->meshes[0].primitives[0];
    if (prim->type != cgltf_primitive_type_triangles) return false;

    cgltf_accessor* posAcc = nullptr;
    cgltf_accessor* uvAcc  = nullptr;
    cgltf_accessor* nrmAcc = nullptr;
    cgltf_accessor* jntAcc = nullptr;
    cgltf_accessor* wgtAcc = nullptr;

    for (cgltf_size i = 0; i < prim->attributes_count; ++i) {
        cgltf_attribute& a = prim->attributes[i];
        if      (a.type == cgltf_attribute_type_position && a.index == 0) posAcc = a.data;
        else if (a.type == cgltf_attribute_type_texcoord && a.index == 0) uvAcc  = a.data;
        else if (a.type == cgltf_attribute_type_normal   && a.index == 0) nrmAcc = a.data;
        else if (a.type == cgltf_attribute_type_joints   && a.index == 0) jntAcc = a.data;
        else if (a.type == cgltf_attribute_type_weights  && a.index == 0) wgtAcc = a.data;
    }

    if (!posAcc) return false;
    cgltf_size n = posAcc->count;

    out.positions.resize(n);
    for (cgltf_size i = 0; i < n; ++i) {
        float v[3] = {};
        cgltf_accessor_read_float(posAcc, i, v, 3);
        out.positions[i] = {v[0], v[1], v[2]};
    }

    out.uvs.resize(n, math::Vec2(0.0f, 0.0f));
    if (uvAcc) {
        for (cgltf_size i = 0; i < n; ++i) {
            float v[2] = {};
            cgltf_accessor_read_float(uvAcc, i, v, 2);
            out.uvs[i] = {v[0], v[1]};
        }
    }

    out.normals.resize(n, math::Vec3(0.0f, 1.0f, 0.0f));
    if (nrmAcc) {
        for (cgltf_size i = 0; i < n; ++i) {
            float v[3] = {};
            cgltf_accessor_read_float(nrmAcc, i, v, 3);
            out.normals[i] = {v[0], v[1], v[2]};
        }
    }

    if (jntAcc && wgtAcc) {
        out.boneIndices.resize(n);
        out.boneWeights.resize(n);
        for (cgltf_size i = 0; i < n; ++i) {
            cgltf_uint j[4] = {};
            float      w[4] = {};
            cgltf_accessor_read_uint(jntAcc, i, j, 4);
            cgltf_accessor_read_float(wgtAcc, i, w, 4);
            out.boneIndices[i] = {(uint8_t)j[0], (uint8_t)j[1], (uint8_t)j[2], (uint8_t)j[3]};
            out.boneWeights[i] = {w[0], w[1], w[2], w[3]};
        }
    }

    if (prim->indices) {
        out.indices.resize(prim->indices->count);
        for (cgltf_size i = 0; i < prim->indices->count; ++i) {
            cgltf_uint v = 0;
            cgltf_accessor_read_uint(prim->indices, i, &v, 1);
            out.indices[i] = static_cast<uint32_t>(v);
        }
    }

    return true;
}

// ── skeleton ─────────────────────────────────────────────────────────────

static bool extractSkeleton(cgltf_data* data, anim::Skeleton& out) {
    if (data->skins_count == 0) return false;
    cgltf_skin* skin = &data->skins[0];
    if (skin->joints_count == 0) return false;

    std::vector<int> parentIdx(skin->joints_count, -1);
    for (cgltf_size j = 0; j < skin->joints_count; ++j) {
        cgltf_node* joint = skin->joints[j];
        if (!joint->parent) continue;
        for (cgltf_size k = 0; k < skin->joints_count; ++k) {
            if (skin->joints[k] == joint->parent) {
                parentIdx[j] = static_cast<int>(k);
                break;
            }
        }
    }

    out.bones.resize(skin->joints_count);
    for (cgltf_size j = 0; j < skin->joints_count; ++j) {
        anim::Bone& bone = out.bones[j];
        bone.parent = parentIdx[j];
        if (skin->inverse_bind_matrices) {
            float m[16] = {};
            cgltf_accessor_read_float(skin->inverse_bind_matrices, j, m, 16);
            std::memcpy(bone.invBindPose.m, m, sizeof(float) * 16);
        }
        cgltf_node_transform_local(skin->joints[j], bone.restLocal.m);
        if (skin->joints[j]->name) bone.name = skin->joints[j]->name;
    }
    return true;
}

// ── animations ───────────────────────────────────────────────────────────

static void extractAnimations(cgltf_data* data,
    std::vector<std::pair<std::string, anim::SkeletalClip>>& out)
{
    cgltf_skin* skin = data->skins_count > 0 ? &data->skins[0] : nullptr;

    for (cgltf_size a = 0; a < data->animations_count; ++a) {
        cgltf_animation* anim = &data->animations[a];
        std::string name = anim->name ? anim->name : ("clip_" + std::to_string(a));

        struct TRSChannels {
            std::vector<float>      tTimes, rTimes, sTimes;
            std::vector<math::Vec3> tValues, sValues;
            std::vector<math::Quat> rValues;
        };
        std::map<int, TRSChannels> boneTracks;

        for (cgltf_size c = 0; c < anim->channels_count; ++c) {
            cgltf_animation_channel* ch = &anim->channels[c];
            if (!ch->target_node || !ch->sampler) continue;

            int boneIdx = -1;
            if (skin) {
                for (cgltf_size j = 0; j < skin->joints_count; ++j) {
                    if (skin->joints[j] == ch->target_node) {
                        boneIdx = static_cast<int>(j);
                        break;
                    }
                }
            }
            if (boneIdx < 0) continue;

            cgltf_accessor* inAcc  = ch->sampler->input;
            cgltf_accessor* outAcc = ch->sampler->output;
            std::vector<float> times(inAcc->count);
            for (cgltf_size t = 0; t < inAcc->count; ++t)
                cgltf_accessor_read_float(inAcc, t, &times[t], 1);

            TRSChannels& track = boneTracks[boneIdx];
            if (ch->target_path == cgltf_animation_path_type_translation) {
                track.tTimes = times;
                track.tValues.resize(outAcc->count);
                for (cgltf_size t = 0; t < outAcc->count; ++t) {
                    float v[3] = {};
                    cgltf_accessor_read_float(outAcc, t, v, 3);
                    track.tValues[t] = {v[0], v[1], v[2]};
                }
            } else if (ch->target_path == cgltf_animation_path_type_rotation) {
                track.rTimes = times;
                track.rValues.resize(outAcc->count);
                for (cgltf_size t = 0; t < outAcc->count; ++t) {
                    float v[4] = {};
                    cgltf_accessor_read_float(outAcc, t, v, 4);
                    track.rValues[t] = {v[0], v[1], v[2], v[3]};
                }
            } else if (ch->target_path == cgltf_animation_path_type_scale) {
                track.sTimes = times;
                track.sValues.resize(outAcc->count);
                for (cgltf_size t = 0; t < outAcc->count; ++t) {
                    float v[3] = {};
                    cgltf_accessor_read_float(outAcc, t, v, 3);
                    track.sValues[t] = {v[0], v[1], v[2]};
                }
            }
        }

        anim::SkeletalClip clip;
        clip.loop = anim::isLoopClipName(name);
        for (auto& [boneIdx, trs] : boneTracks) {
            std::set<float> timeSet;
            for (float t : trs.tTimes) timeSet.insert(t);
            for (float t : trs.rTimes) timeSet.insert(t);
            for (float t : trs.sTimes) timeSet.insert(t);
            if (timeSet.empty()) continue;

            const cgltf_node* joint = skin->joints[boneIdx];
            const math::Vec3 restT = joint->has_translation
                ? math::Vec3(joint->translation[0], joint->translation[1], joint->translation[2])
                : math::Vec3(0.0f, 0.0f, 0.0f);
            const math::Quat restR = joint->has_rotation
                ? math::Quat{joint->rotation[0], joint->rotation[1], joint->rotation[2], joint->rotation[3]}
                : math::Quat::identity();
            const math::Vec3 restS = joint->has_scale
                ? math::Vec3(joint->scale[0], joint->scale[1], joint->scale[2])
                : math::Vec3(1.0f, 1.0f, 1.0f);

            anim::BoneTrack btrack;
            btrack.boneIndex = boneIdx;
            for (float t : timeSet) {
                anim::Keyframe kf;
                kf.time        = t;
                kf.translation = sampleVec3Track(trs.tTimes, trs.tValues, t, restT);
                kf.rotation    = sampleQuatTrack(trs.rTimes, trs.rValues, t, restR);
                kf.scale       = sampleVec3Track(trs.sTimes, trs.sValues, t, restS);
                btrack.keyframes.push_back(kf);
            }
            clip.tracks.push_back(std::move(btrack));
        }
        out.push_back({std::move(name), std::move(clip)});
    }
}

// ── public API ───────────────────────────────────────────────────────────

bool sortJointsParentFirst(LoadResult& r) {
    auto& bones = r.skeleton.bones;
    const int n = static_cast<int>(bones.size());
    bool sorted = true;
    for (int i = 0; i < n; ++i)
        if (bones[i].parent >= i) sorted = false;
    if (sorted) return false;

    std::vector<int> order;
    std::vector<bool> placed(n, false);
    order.reserve(n);
    while (static_cast<int>(order.size()) < n) {
        const size_t before = order.size();
        for (int i = 0; i < n; ++i) {
            if (placed[i]) continue;
            const int p = bones[i].parent;
            if (p < 0 || placed[p]) {
                placed[i] = true;
                order.push_back(i);
            }
        }
        if (order.size() == before) return false;
    }

    std::vector<int> newIndex(n);
    for (int k = 0; k < n; ++k) newIndex[order[k]] = k;

    std::vector<anim::Bone> sortedBones(n);
    for (int k = 0; k < n; ++k) {
        sortedBones[k] = bones[order[k]];
        if (sortedBones[k].parent >= 0) sortedBones[k].parent = newIndex[sortedBones[k].parent];
    }
    bones = std::move(sortedBones);
    for (auto& idx : r.mesh.boneIndices)
        for (auto& j : idx)
            if (j < n) j = static_cast<uint8_t>(newIndex[j]);
    for (auto& [name, clip] : r.clips)
        for (auto& trk : clip.tracks)
            if (trk.boneIndex >= 0 && trk.boneIndex < n) trk.boneIndex = newIndex[trk.boneIndex];
    return true;
}

bool load(const uint8_t* bytes, size_t size, LoadResult& out) {
    if (!bytes || size == 0) return false;

    cgltf_options options = {};
    cgltf_data* data = nullptr;
    if (cgltf_parse(&options, bytes, size, &data) != cgltf_result_success)
        return false;
    // for .glb, embedded buffer is already loaded by cgltf_parse; nullptr path skips external URIs
    if (cgltf_load_buffers(&options, data, nullptr) != cgltf_result_success) {
        cgltf_free(data);
        return false;
    }

    out = {};
    bool ok = extractMesh(data, out.mesh);
    if (ok) {
        extractSkeleton(data, out.skeleton);
        extractAnimations(data, out.clips);
        sortJointsParentFirst(out);
    }
    cgltf_free(data);
    return ok;
}

void selfTest() {
    aout << "[gltf] self-test starting" << std::endl;

    LoadResult r;
    assert(!load(nullptr, 0, r));
    assert(!load(nullptr, 128, r));

    uint8_t garbage[64] = {};
    assert(!load(garbage, sizeof(garbage), r));

    // minimal valid GLB (header + JSON chunk, no mesh) → false
    {
        // JSON padded to 4-byte boundary with spaces
        const char* json = "{\"asset\":{\"version\":\"2.0\"}}  ";
        uint8_t glb[48] = {};
        uint32_t magic    = 0x46546C67u;
        uint32_t ver      = 2u;
        uint32_t total    = 48u;
        uint32_t jsonLen  = 28u;
        uint32_t jsonType = 0x4E4F534Au;
        std::memcpy(glb +  0, &magic,    4);
        std::memcpy(glb +  4, &ver,      4);
        std::memcpy(glb +  8, &total,    4);
        std::memcpy(glb + 12, &jsonLen,  4);
        std::memcpy(glb + 16, &jsonType, 4);
        std::memcpy(glb + 20, json,      28);
        assert(!load(glb, 48, r)); // valid parse, no mesh → false
    }

    {
        LoadResult lr;
        lr.skeleton.bones.resize(3);
        lr.skeleton.bones[0].parent = 2; lr.skeleton.bones[0].name = "child";
        lr.skeleton.bones[1].parent = 0; lr.skeleton.bones[1].name = "grandchild";
        lr.skeleton.bones[2].parent = -1; lr.skeleton.bones[2].name = "root";
        lr.mesh.boneIndices = {{0, 1, 2, 0}};
        anim::SkeletalClip clip;
        clip.tracks.push_back({1, {}});
        lr.clips.push_back({"Idle", clip});
        assert(sortJointsParentFirst(lr));
        const auto& b = lr.skeleton.bones;
        assert(b[0].name == "root" && b[1].name == "child" && b[2].name == "grandchild");
        assert(b[0].parent == -1 && b[1].parent == 0 && b[2].parent == 1);
        assert(lr.mesh.boneIndices[0][0] == 1 && lr.mesh.boneIndices[0][1] == 2 &&
               lr.mesh.boneIndices[0][2] == 0);
        assert(lr.clips[0].second.tracks[0].boneIndex == 2);
        assert(lr.skeleton.find("grandchild") == 2 && lr.skeleton.find("nope") == -1);
        assert(!sortJointsParentFirst(lr));
    }

    aout << "[gltf] self-test PASS" << std::endl;
}

} // namespace gltf
