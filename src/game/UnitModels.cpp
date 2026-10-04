#include "UnitModels.h"

#include <cassert>
#include <cstring>

#include "ClipEvents.h"
#include "LabAssets.h"
#include "UnitCatalog.h"
#include "UnitDef.h"
#include "UnitTypes.h"

namespace game {

namespace {

const anim::SkeletalClip* clip(const UnitModels::Rig& rig, const char* name) {
    auto it = rig.clips.find(name);
    return it == rig.clips.end() ? nullptr : &it->second;
}

}

const char* UnitModels::propFile(int type) {
    static const char* kFiles[kPropTypes] = {"prop_crate.glb", "prop_rock.glb", "prop_tree.glb"};
    return type >= 0 && type < kPropTypes ? kFiles[type] : nullptr;
}

const std::array<const char*, 5>& UnitModels::clipNames() {
    static const std::array<const char*, 5> kNames = {"Idle", "Walk", "Attack", "Hurt", "Death"};
    return kNames;
}

bool UnitModels::loadRig(const Platform& p, LabAssets& assets, const std::string& file) {
    if (rigs_.count(file)) return true;
    std::vector<uint8_t> bytes;
    if (!p.readAsset(file, bytes)) return false;
    gltf::LoadResult glb;
    if (!gltf::load(bytes.data(), bytes.size(), glb)) return false;
    Rig& rig = rigs_[file];
    rig.skeleton = std::move(glb.skeleton);
    for (auto& [name, c] : glb.clips) {
        applyClipEvents(name, c);
        rig.clips[name] = std::move(c);
    }
    rig.mesh = assets.addMesh(file, std::move(glb.mesh));
    return true;
}

void UnitModels::load(const Platform& p, LabAssets& assets) {
    if (!loadRig(p, assets, kDefaultRig)) aout << "[models] " << kDefaultRig << " missing; cube rigs" << std::endl;
    for (const auto& t : unitTypes())
        if (t.model && !loadRig(p, assets, t.model)) aout << "[models] " << t.model << " missing; test_rig" << std::endl;
    for (int i = 0; i < kPropTypes; ++i) {
        std::vector<uint8_t> bytes;
        gltf::LoadResult glb;
        if (p.readAsset(propFile(i), bytes) && gltf::load(bytes.data(), bytes.size(), glb))
            props_[i] = assets.addMesh(propFile(i), std::move(glb.mesh));
        else
            aout << "[models] " << propFile(i) << " missing; box prop" << std::endl;
    }
    palette_ = TextureAsset::loadAsset(p, kPalette);
    skinnedMat_ = {assets.material("skinned")->shader, palette_.get()};
    litMat_ = {assets.material("lit")->shader, palette_.get()};
}

const UnitModels::Rig* UnitModels::rig(const std::string& file) const {
    auto it = rigs_.find(file);
    return it == rigs_.end() ? nullptr : &it->second;
}

bool UnitModels::ownModel(const std::string& typeId) const {
    const UnitType* t = findUnitType(typeId);
    return t && t->model && rig(t->model);
}

const UnitModels::Rig* UnitModels::rigFor(const std::string& typeId) const {
    const UnitType* t = findUnitType(typeId);
    if (t && t->model && rig(t->model)) return rig(t->model);
    return rig(kDefaultRig);
}

asset::Handle UnitModels::propMesh(int type) const {
    return type >= 0 && type < kPropTypes ? props_[type] : asset::kNull;
}

void UnitModels::buildCatalog(UnitCatalog& catalog, const LabAssets& assets) const {
    catalog.clear();
    for (const auto& t : unitTypes()) {
        unit::UnitDef u;
        u.name = t.name;
        u.faction = t.faction;
        u.baseStats = t.stats;
        u.baseColor = t.color;
        u.baseScale = kUnitScale;
        u.modelYawDeg = 180.0f;
        if (const Rig* r = rigFor(t.id)) {
            u.skeleton = &r->skeleton;
            u.mesh = assets.mesh(r->mesh);
            u.model = assets.model(r->mesh);
            u.material = assets.material("skinned");
            const auto& n = clipNames();
            u.clips = {clip(*r, n[0]), clip(*r, n[1]), clip(*r, n[2]), clip(*r, n[3]), clip(*r, n[4])};
            if (ownModel(t.id) && palette_) {
                u.material = &skinnedMat_;
                u.baseColor = math::Vec3(1.0f, 1.0f, 1.0f);
            }
        }
        catalog.registerDef(t.id, u);
    }
}

void UnitModels::selfTest() {
    aout << "[models] self-test starting" << std::endl;
    for (int i = 0; i < kPropTypes; ++i) assert(propFile(i) && std::strstr(propFile(i), ".glb"));
    assert(propFile(-1) == nullptr && propFile(kPropTypes) == nullptr);
    assert(clipNames().size() == 5 && std::strcmp(clipNames()[2], "Attack") == 0);
    for (const auto& t : unitTypes())
        if (t.model) assert(std::strstr(t.model, ".glb") && std::strcmp(t.model, kDefaultRig) != 0);

    UnitModels m;
    assert(!m.textured() && m.rig(kDefaultRig) == nullptr && m.rigFor("knight") == nullptr);
    assert(!m.ownModel("knight") && !m.ownModel("nope") && m.propMesh(0) == asset::kNull);
    m.rigs_[kDefaultRig].skeleton.bones.resize(2);
    assert(m.rigFor("knight") == m.rig(kDefaultRig) && !m.ownModel("knight"));
    m.rigs_["knight.glb"];
    assert(m.ownModel("knight") && m.rigFor("knight") == m.rig("knight.glb"));
    assert(m.rigFor("slinger") == m.rig(kDefaultRig));
    aout << "[models] self-test PASS" << std::endl;
}

} // namespace game
