#include "LevelSpawner.h"

#include <cassert>

#include "BattleGrid.h"
#include "Engine.h"
#include "Prop.h"
#include "Unit.h"
#include "UnitCatalog.h"
#include "UnitDef.h"

namespace game {

namespace {

struct Box { math::Vec3 pos; math::Vec3 size; float shade; };

std::vector<Box> propBoxes(uint8_t type) {
    switch (type) {
        case 0:  return {{{0.0f, 0.35f, 0.0f}, {0.7f, 0.7f, 0.7f}, 1.0f}};
        case 1:  return {{{0.0f, 0.22f, 0.0f}, {0.8f, 0.45f, 0.7f}, 1.0f}};
        default: return {{{0.0f, 0.40f, 0.0f}, {0.18f, 0.8f, 0.18f}, 0.0f},
                         {{0.0f, 1.15f, 0.0f}, {0.7f, 0.8f, 0.7f}, 1.0f}};
    }
}

}

std::unique_ptr<grid::BattleGrid> LevelSpawner::makeGrid(const level::Level& lvl) {
    auto g = std::make_unique<grid::BattleGrid>(lvl.width, lvl.height, lvl.cellSize);
    for (uint16_t y = 0; y < lvl.height; ++y)
        for (uint16_t x = 0; x < lvl.width; ++x) {
            const size_t i = static_cast<size_t>(y) * lvl.width + x;
            if (lvl.terrain[i] == 0) g->setPassable(x, y, false);
            g->setCellY(x, y, lvl.cellY[i]);
        }
    return g;
}

std::vector<Unit*> LevelSpawner::spawnUnits(const level::Level& lvl, const UnitCatalog& catalog,
                                            EntityBuilder& builder, const Visuals& rig) {
    std::vector<Unit*> out;
    for (const auto& rec : lvl.units) {
        SpawnDef s;
        s.cellX = rec.x;
        s.cellY = rec.y;
        s.mesh = rig.cube;
        s.model = rig.cubeModel;
        s.material = rig.material;
        Unit* u = nullptr;
        if (!rec.catalogId.empty() && catalog.has(rec.catalogId)) {
            u = builder.spawnFromCatalog(catalog, rec.catalogId, s);
        } else {
            s.faction = static_cast<unit::Faction>(rec.faction);
            s.baseStats = {rec.maxHp, rec.atk, rec.def, rec.mov, rec.range};
            s.baseColor = rec.baseColor;
            s.selectedColor = rec.selectedColor;
            s.baseScale = 0.45f;
            u = builder.spawnUnit(s);
        }
        if (u) {
            u->setUnitId(rec.unitId);
            out.push_back(u);
        }
    }
    return out;
}

std::vector<Prop*> LevelSpawner::spawnProps(const level::Level& lvl, EntityBuilder& builder,
                                            grid::BattleGrid& grid, const Visuals& v) {
    std::vector<Prop*> out;
    for (const auto& rec : lvl.props) {
        SpawnDef s;
        s.kind = ObjectKind::Prop;
        s.propColor = rec.color;
        s.mesh = v.cube;
        s.model = v.cubeModel;
        s.material = v.material;
        Prop* p = builder.spawnProp(s);
        if (!p) continue;
        math::Vec3 at = rec.pos;
        if (lvl.version >= 3) {
            at = grid.cellToWorld(rec.x, rec.y);
            if (rec.blocks) grid.setPassable(rec.x, rec.y, false);
        }
        SceneNode* n = p->node();
        n->drawables().clear();
        n->setLocal(math::Mat4::translation(at) *
                    math::Mat4::rotation(math::Quat::fromAxisAngle(math::Vec3(0, 1, 0), rec.yaw)) *
                    math::Mat4::scale(math::Vec3(rec.scale)));
        p->setScale(math::Vec3(rec.scale));
        for (const Box& b : propBoxes(rec.type)) {
            SceneNode* c = n->addChild();
            c->setLocal(math::Mat4::translation(b.pos) * math::Mat4::scale(b.size));
            const math::Vec3 trunk(0.40f, 0.27f, 0.15f);
            c->addDrawable({v.cube, v.cubeModel, b.shade > 0.5f ? rec.color : trunk, v.material});
        }
        out.push_back(p);
    }
    return out;
}

void LevelSpawner::selfTest() {
    aout << "[level-spawner] self-test starting" << std::endl;

    level::Level lvl;
    lvl.width = 4;
    lvl.height = 3;
    lvl.terrain.assign(12, 1);
    lvl.terrain[5] = 0;
    lvl.cellY.assign(12, 0.0f);
    lvl.cellY[2] = 0.5f;
    level::UnitSpawn a;
    a.x = 0; a.y = 0; a.catalogId = "knight";
    level::UnitSpawn b;
    b.x = 3; b.y = 2; b.faction = 1; b.maxHp = 7; b.atk = 3; b.def = 1; b.mov = 2; b.range = 1;
    lvl.units = {a, b};
    level::Prop crate;
    crate.type = 0; crate.x = 2; crate.y = 2; crate.scale = 0.5f;
    level::Prop tree;
    tree.type = 2; tree.x = 3; tree.y = 0; tree.blocks = false;
    lvl.props = {crate, tree};

    auto g = makeGrid(lvl);
    assert(!g->at(1, 1).passable && g->at(0, 0).passable);
    assert(math::approxEqual(g->cellY(2, 0), 0.5f));

    UnitCatalog cat;
    unit::UnitDef kd;
    kd.name = "Knight";
    kd.faction = unit::Faction::Player;
    kd.baseStats = {15, 6, 4, 4, 1};
    cat.registerDef("knight", kd);

    const Mesh m{};
    SceneNode root;
    EntityBuilder eb(&root, g.get());
    const Visuals vis{&m, nullptr, nullptr};
    const auto units = spawnUnits(lvl, cat, eb, vis);
    assert(units.size() == 2);
    assert(units[0]->stats().maxHp == 15 && units[0]->faction() == unit::Faction::Player);
    assert(units[1]->stats().maxHp == 7 && units[1]->faction() == unit::Faction::Enemy);
    assert(g->at(3, 2).blockingUnit == units[1]);

    const auto props = spawnProps(lvl, eb, *g, vis);
    assert(props.size() == 2);
    assert(!g->at(2, 2).passable && g->at(3, 0).passable);
    const math::Vec3 wp = g->cellToWorld(2, 2);
    assert(math::approxEqual(props[0]->node()->local().m[12], wp.x) &&
           math::approxEqual(props[0]->node()->local().m[14], wp.z));
    assert(props[0]->node()->children().size() == 1 && props[1]->node()->children().size() == 2);

    aout << "[level-spawner] self-test PASS" << std::endl;
}

} // namespace game
