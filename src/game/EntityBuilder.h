#ifndef NOT_A_GAME_ENTITY_BUILDER_GAME_H
#define NOT_A_GAME_ENTITY_BUILDER_GAME_H

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Animation.h"
#include "Bone.h"
#include "EntityRegistry.h"
#include "Math.h"
#include "Object.h"
#include "Unit.h"

class SceneNode;
struct Mesh;
struct Model;
struct Material;

namespace unit {
struct ClipSet;
}
namespace grid {
class BattleGrid;
}

namespace game {

class Unit;
class UnitCatalog;
class Prop;

enum class ObjectKind : uint8_t {
    Unit = 0,
    Prop = 1,
};

struct SpawnDef {
    ObjectKind kind = ObjectKind::Unit;
    std::string name;

    // Unit
    unit::Faction faction         = unit::Faction::Neutral;
    unit::Stats   baseStats       = {10, 5, 3, 4, 1};
    math::Vec3    baseColor       {1.0f, 1.0f, 1.0f};
    math::Vec3    selectedColor   {1.5f, 1.5f, 0.5f};
    bool          useRig          = true;
    float         rootDropY       = 0.5f;
    const anim::Skeleton*     skeleton   = nullptr;
    const unit::ClipSet* clips      = nullptr;
    const anim::Clip*  clipTemplate = nullptr;  // per-unit clip template (rig units)

    // Prop
    math::Vec3 propColor{0.5f, 0.5f, 0.5f};
    math::Vec3 propScale{1.0f, 1.0f, 1.0f};

    // Visual (rig uses mesh/model/material for pivot boxes; prop uses them directly)
    const Mesh*     mesh     = nullptr;
    const Model*    model    = nullptr;
    const Material* material = nullptr;

    // Placement
    int cellX = -1;
    int cellY = -1;
};

class EntityBuilder {
public:
    EntityBuilder(SceneNode* root, grid::BattleGrid* grid = nullptr,
                  SceneNode* shadowRoot = nullptr,
                  std::deque<anim::Clip>* clipStorage = nullptr);
    ~EntityBuilder();

    Object* spawn(const SpawnDef& def);

    Unit* spawnUnit(const SpawnDef& def);
    Prop* spawnProp(const SpawnDef& def);

    Unit* spawnFromCatalog(const UnitCatalog& catalog,
                           const std::string& catalogId,
                           const SpawnDef& overrides);

    const std::vector<std::unique_ptr<Unit>>& units() const { return units_; }
    const std::vector<std::unique_ptr<Prop>>& props()      const { return props_; }

    std::vector<Unit*> unitsAll() const;
    std::vector<Prop*> propsAll()      const;

    std::vector<std::unique_ptr<Unit>> releaseUnits();
    std::vector<std::unique_ptr<Prop>> releaseProps();

    const std::vector<SceneNode*>& shadowNodes() const { return shadowNodes_; }

    void eachUnit(std::function<void(Unit&)> fn);
    void eachProp(std::function<void(Prop&)> fn);

    void updateAll(float dt);
    void saveAll(engine::SaveWriter& w) const;
    void loadAll(engine::SaveReader& r);

    // Transfer ownership into registry (replaces releaseUnits/releaseProps)
    void populate(EntityRegistry& reg);

    static void syncShadowMirror(const SceneNode* src, SceneNode* dst);

    static void selfTest();

private:
    void placeUnitAtCell(Unit* u);
    void createShadowMirror(Unit* u);

    SceneNode* root_ = nullptr;
    grid::BattleGrid* grid_ = nullptr;
    SceneNode* shadowRoot_ = nullptr;
    std::deque<anim::Clip>* clipStorage_ = nullptr;
    std::vector<std::unique_ptr<Unit>> units_;
    std::vector<std::unique_ptr<Prop>> props_;
    std::vector<SceneNode*> shadowNodes_;
};

} // namespace game

#endif
