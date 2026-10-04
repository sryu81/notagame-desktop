#ifndef NOT_A_GAME_LEVEL_SPAWNER_H
#define NOT_A_GAME_LEVEL_SPAWNER_H

#include <memory>
#include <vector>

#include "EntityBuilder.h"
#include "LevelLoader.h"

namespace grid { class BattleGrid; }

namespace game {

class Prop;
class Unit;
class UnitCatalog;

class LevelSpawner {
public:
    struct Visuals {
        const Mesh*     cube     = nullptr;
        const Model*    cubeModel = nullptr;
        const Material* material = nullptr;
        const Mesh*     propMesh[3] = {nullptr, nullptr, nullptr};
        const Model*    propModel[3] = {nullptr, nullptr, nullptr};
        const Material* propMaterial = nullptr;
    };

    static std::unique_ptr<grid::BattleGrid> makeGrid(const level::Level& lvl);
    static std::vector<Unit*> spawnUnits(const level::Level& lvl, const UnitCatalog& catalog,
                                         EntityBuilder& builder, const Visuals& rig);
    static std::vector<Prop*> spawnProps(const level::Level& lvl, EntityBuilder& builder,
                                         grid::BattleGrid& grid, const Visuals& v);

    static void selfTest();
};

} // namespace game

#endif
