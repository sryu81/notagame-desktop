#ifndef NOT_A_GAME_TILEGRID_H
#define NOT_A_GAME_TILEGRID_H

#include <vector>

#include "Math.h"

class Mesh;
class Model;
class SceneNode;
struct Material;

namespace engine {

class TileGrid {
public:
    TileGrid(int width, int height, float cellSize);

    int width() const { return width_; }
    int height() const { return height_; }
    float cellSize() const { return cellSize_; }

    void setCellY(int x, int y, float yElev);
    void setCellY(const std::vector<float>& heightmap);
    void applyElevationFromMesh(const Mesh& mesh);
    float cellY(int x, int y) const;

    bool inBounds(int x, int y) const;

    math::Vec2 worldToCell(const math::Vec3& world) const;
    math::Vec3 cellToWorld(int x, int y) const;

    static void selfTest();

private:
    int width_;
    int height_;
    float cellSize_;
    std::vector<float> cellY_;
};

} // namespace engine

#endif
