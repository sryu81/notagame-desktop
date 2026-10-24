#ifndef NOT_A_GAME_UNIT_SPRITES_H
#define NOT_A_GAME_UNIT_SPRITES_H

#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "AssetRegistry.h"
#include "Material.h"
#include "SceneNode.h"
#include "Sprite.h"

class Camera;
class Platform;
class TextureAsset;

namespace game {

class LabAssets;
class Unit;
class UnitModels;

class UnitSprites {
public:
    struct SheetAsset {
        std::string model;
        sprite::Sheet sheet;
        std::shared_ptr<TextureAsset> texture;
        asset::Handle quad = asset::kNull;
        Material material;
    };

    void load(const Platform& p, LabAssets& assets);
    void createMaterials(LabAssets& assets);
    void attach(SceneNode* root, const UnitModels& models, const std::vector<Unit*>& units,
                const LabAssets& assets);
    void setEnabled(bool on);
    bool enabled() const { return enabled_; }
    void update(const Camera& cam);

    size_t sheetCount() const { return sheets_.size(); }
    size_t boundCount() const { return bound_.size(); }
    bool allBound(const std::vector<Unit*>& units) const;
    bool inSync() const;
    int seenClips() const { return seenClips_; }

    static std::string sheetName(const std::string& modelFile);
    static int clipIndex(const Unit& u);
    static int frameAt(const sprite::Clip& c, float time, float clipDuration);
    static void selfTest();

private:
    struct Binding {
        Unit* unit = nullptr;
        SceneNode* card = nullptr;
        const SheetAsset* sheet = nullptr;
        SceneNode::Drawable cardDrawable;
        std::vector<std::pair<SceneNode*, std::deque<SceneNode::Drawable>>> hidden;
        int clip = -1;
        int frame = 0;
        int facing = 0;
    };

    void apply(Binding& b) const;

    std::vector<SheetAsset> sheets_;
    std::vector<Binding> bound_;
    bool enabled_ = true;
    int seenClips_ = 0;
};

} // namespace game

#endif
