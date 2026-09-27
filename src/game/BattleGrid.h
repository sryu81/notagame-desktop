#ifndef NOT_A_GAME_BATTLEGRID_H
#define NOT_A_GAME_BATTLEGRID_H

#include <vector>

#include "Math.h"
#include "TileGrid.h"

class Mesh;
class Model;
class SceneNode;
struct Material;

namespace game { class Unit; }

namespace grid {

struct Cell {
    bool passable = true;
    bool cover = false;
    game::Unit* blockingUnit = nullptr;
};

class BattleGrid {
public:
    BattleGrid(int width, int height, float cellSize);

    int width() const { return tiles_.width(); }
    int height() const { return tiles_.height(); }
    float cellSize() const { return tiles_.cellSize(); }

    bool inBounds(int x, int y) const { return tiles_.inBounds(x, y); }
    const Cell& at(int x, int y) const;
    void setPassable(int x, int y, bool passable);
    void setCover(int x, int y, bool cover);
    void setCellUnit(int x, int y, game::Unit* u);
    void setCellY(int x, int y, float yElev) { tiles_.setCellY(x, y, yElev); }
    void applyElevationFromMesh(const Mesh& mesh) { tiles_.applyElevationFromMesh(mesh); }
    float cellY(int x, int y) const { return tiles_.cellY(x, y); }

    math::Vec2 worldToCell(const math::Vec3& world) const { return tiles_.worldToCell(world); }
    math::Vec3 cellToWorld(int x, int y) const { return tiles_.cellToWorld(x, y); }

    std::vector<SceneNode*> attachTo(SceneNode* parent, const Mesh* planeMesh, const Model* planeModel,
                  const math::Vec3& passableColor, const math::Vec3& impassableColor,
                  const Material* mat = nullptr) const;

    void reTranslateTiles(const std::vector<SceneNode*>& nodes, float yOffset = 0.001f) const;

    SceneNode* attachTerrainMesh(SceneNode* parent, const Mesh* terrainMesh,
                                 const Model* terrainModel, const Material* mat) const;

    bool hasLineOfSight(const math::Vec2& a, const math::Vec2& b) const;

    static void selfTest();

private:
    engine::TileGrid tiles_;
    std::vector<Cell> cells_;
};

} // namespace grid

#endif
