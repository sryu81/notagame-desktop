#include "BattleGrid.h"

#include "Engine.h"

#include <cassert>
#include <cmath>

namespace grid {

BattleGrid::BattleGrid(int width, int height, float cellSize)
        : tiles_(width, height, cellSize) {
    cells_.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
}

const Cell& BattleGrid::at(int x, int y) const {
    assert(tiles_.inBounds(x, y));
    return cells_[static_cast<size_t>(y) * static_cast<size_t>(tiles_.width())
                  + static_cast<size_t>(x)];
}

void BattleGrid::setPassable(int x, int y, bool passable) {
    assert(tiles_.inBounds(x, y));
    cells_[static_cast<size_t>(y) * static_cast<size_t>(tiles_.width())
           + static_cast<size_t>(x)].passable = passable;
}

void BattleGrid::setCover(int x, int y, bool cover) {
    assert(tiles_.inBounds(x, y));
    cells_[static_cast<size_t>(y) * static_cast<size_t>(tiles_.width())
           + static_cast<size_t>(x)].cover = cover;
}

void BattleGrid::setCellUnit(int x, int y, game::Unit* u) {
    assert(tiles_.inBounds(x, y));
    cells_[static_cast<size_t>(y) * static_cast<size_t>(tiles_.width())
           + static_cast<size_t>(x)].blockingUnit = u;
}

static constexpr float kGridTileGap = 0.03f;

std::vector<SceneNode*> BattleGrid::attachTo(SceneNode* parent, const Mesh* planeMesh, const Model* planeModel,
                    const math::Vec3& passableColor, const math::Vec3& impassableColor,
                    const Material* mat) const {
    std::vector<SceneNode*> nodes;
    nodes.reserve(static_cast<size_t>(tiles_.width()) * static_cast<size_t>(tiles_.height()));
    for (int y = 0; y < tiles_.height(); y++) {
        for (int x = 0; x < tiles_.width(); x++) {
            SceneNode* cellNode = parent->addChild();
            const math::Vec3 center = tiles_.cellToWorld(x, y);
            const float tileScale = tiles_.cellSize() * (1.0f - kGridTileGap);
            cellNode->setLocal(math::Mat4::translation(center)
                               * math::Mat4::scale(math::Vec3(tileScale, 1.0f, tileScale)));
            const Cell& c = at(x, y);
            const math::Vec3& color = c.passable ? passableColor : impassableColor;
            SceneNode::Drawable tile{planeMesh, planeModel, color, mat};
            tile.castShadow = false;
            cellNode->addDrawable(tile);
            nodes.push_back(cellNode);
        }
    }
    return nodes;
}

void BattleGrid::reTranslateTiles(const std::vector<SceneNode*>& nodes, float yOffset) const {
    const int w = tiles_.width();
    const int h = tiles_.height();
    assert(static_cast<int>(nodes.size()) == w * h);
    const float tileScale = tiles_.cellSize() * (1.0f - kGridTileGap);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const math::Vec3 center = tiles_.cellToWorld(x, y);
            nodes[static_cast<size_t>(y) * w + x]->setLocal(
                    math::Mat4::translation(math::Vec3(center.x, center.y + yOffset, center.z))
                    * math::Mat4::scale(math::Vec3(tileScale, 1.0f, tileScale)));
        }
    }
}

SceneNode* BattleGrid::attachTerrainMesh(SceneNode* parent, const Mesh* terrainMesh,
                                 const Model* terrainModel, const Material* mat) const {
    (void)tiles_;
    SceneNode* node = parent->addChild();
    node->setLocal(math::Mat4::identity());
    SceneNode::Drawable ground{terrainMesh, terrainModel, math::Vec3(0.55f, 0.55f, 0.55f), mat};
    ground.castShadow = false;
    node->addDrawable(ground);
    return node;
}

void BattleGrid::selfTest() {
    aout << "[battle-grid] self-test starting" << std::endl;

    {
        BattleGrid g(6, 6, 1.0f);
        assert(g.width() == 6);
        assert(g.height() == 6);
        assert(g.inBounds(0, 0));
        assert(g.inBounds(5, 5));
        assert(!g.inBounds(6, 0));
        assert(!g.inBounds(0, 6));
        assert(!g.inBounds(-1, 0));
        for (int y = 0; y < 6; y++) {
            for (int x = 0; x < 6; x++) {
                assert(g.at(x, y).passable);
                assert(!g.at(x, y).cover);
            }
        }
    }

    {
        BattleGrid g(6, 6, 1.0f);
        g.setPassable(0, 0, false);
        g.setPassable(5, 5, false);
        g.setCover(2, 3, true);
        assert(!g.at(0, 0).passable);
        assert(!g.at(5, 5).passable);
        assert(g.at(2, 3).cover);
        assert(g.at(0, 1).passable);
        assert(!g.at(0, 1).cover);
    }

    {
        BattleGrid g(6, 6, 1.0f);
        const math::Vec3 w00 = g.cellToWorld(0, 0);
        assert(math::approxEqual(w00.x, -2.5f, 1e-4f));
        assert(math::approxEqual(w00.z, -2.5f, 1e-4f));
        const math::Vec2 c00 = g.worldToCell(math::Vec3(0.0f, 0.0f, 0.0f));
        assert(math::approxEqual(c00.x, 3.0f, 1e-4f));
        assert(math::approxEqual(c00.y, 3.0f, 1e-4f));
    }

    {
        BattleGrid g(6, 6, 1.0f);
        g.setPassable(2, 2, false);
        g.setPassable(3, 2, false);
        SceneNode parent;
        g.attachTo(&parent, nullptr, nullptr,
                   math::Vec3(0.76f, 0.70f, 0.50f), math::Vec3(0.30f, 0.30f, 0.30f));
        assert(parent.children().size() == 36);
        assert(!parent.children()[0]->drawables()[0].castShadow);
    }

    {
        BattleGrid g(4, 4, 1.0f);
        assert(math::approxEqual(g.cellY(0, 0), 0.0f));
        g.setCellY(0, 0, 0.5f);
        g.setCellY(2, 1, -0.25f);
        assert(math::approxEqual(g.cellY(0, 0), 0.5f));
        assert(math::approxEqual(g.cellY(2, 1), -0.25f));
        assert(math::approxEqual(g.cellToWorld(0, 0).y, 0.5f));
        assert(math::approxEqual(g.cellToWorld(2, 1).y, -0.25f));
    }

    aout << "[battle-grid] self-test PASS" << std::endl;
}

} // namespace grid
