#ifndef NOT_A_GAME_UNIT_MODELS_H
#define NOT_A_GAME_UNIT_MODELS_H

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Engine.h"

class Platform;

namespace game {

class LabAssets;
class UnitCatalog;

class UnitModels {
public:
    static constexpr float kUnitScale = 0.5f;
    static constexpr int   kPropTypes = 3;
    static constexpr const char* kDefaultRig = "test_rig.glb";
    static constexpr const char* kPalette = "knight_palette.png";

    struct Rig {
        anim::Skeleton skeleton;
        std::map<std::string, anim::SkeletalClip> clips;
        asset::Handle mesh = asset::kNull;
    };

    void load(const Platform& p, LabAssets& assets);
    void buildCatalog(UnitCatalog& catalog, const LabAssets& assets) const;

    const Rig* rig(const std::string& file) const;
    const Rig* rigFor(const std::string& typeId) const;
    bool ownModel(const std::string& typeId) const;
    asset::Handle propMesh(int type) const;
    const Material* propMaterial() const { return palette_ ? &litMat_ : nullptr; }
    const Material* skinnedMaterial() const { return palette_ ? &skinnedMat_ : nullptr; }
    bool textured() const { return palette_ != nullptr; }

    static const char* propFile(int type);
    static const std::array<const char*, 5>& clipNames();
    static void selfTest();

private:
    bool loadRig(const Platform& p, LabAssets& assets, const std::string& file);

    std::map<std::string, Rig> rigs_;
    std::array<asset::Handle, kPropTypes> props_{asset::kNull, asset::kNull, asset::kNull};
    std::shared_ptr<TextureAsset> palette_;
    Material skinnedMat_;
    Material litMat_;
};

} // namespace game

#endif
