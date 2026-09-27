#include "Unit.h"
#include "UnitCatalog.h"
#include "UnitDef.h"
#include "UnitFactory.h"
#include "SceneGraphBuilder.h"
#include "SceneNode.h"

#include "AndroidOut.h"
#include "Material.h"
#include "Mesh.h"
#include "Model.h"

int main() {
    aout << "host Unit driver starting" << std::endl;

    game::Unit::selfTest();
    engine::scenegraph::selfTest();

    {
        unit::UnitDef def;
        def.name = "TestHero";
        def.baseStats = unit::Stats{20, 7, 3, 5, 1};
        def.faction = unit::Faction::Player;
        def.baseColor = math::Vec3(0.2f, 0.6f, 0.3f);
        def.rootDropY = 0.5f;

        const Mesh m{};
        engine::RigResources res{&m, nullptr, nullptr};
        SceneNode root;
        auto c = unit::spawn(&root, def, res);
        assert(c != nullptr);
        assert(c->node() != nullptr);
        assert(c->node()->parent() == &root);
        assert(c->faction() == unit::Faction::Player);
        assert(c->hp() == 20);
        aout << "[host] unit::spawn built rig with "
             << c->node()->children().size() << " top-level pivots" << std::endl;
    }

    {
        SceneNode parent;
        const Mesh m{};
        engine::RigResources res{&m, nullptr, nullptr};
        engine::RigDef r;
        r.bodyColor = {0.5f, 0.5f, 0.9f};
        r.rootDropY = 0.0f;
        SceneNode* root = engine::scenegraph::buildRigNode(parent, res, r, math::Mat4::identity());
        assert(root != nullptr);
        assert(math::approxEqual(root->local().m[13], 0.0f, 1e-4f));
        size_t total = 0;
        for (const auto& c : root->children()) {
            total += 1 + c->children().size();
            for (const auto& gc : c->children()) {
                total += gc->children().size();
            }
        }
        aout << "[host] buildRigNode produced " << total << " total nodes" << std::endl;
    }

    aout << "host Unit driver PASS" << std::endl;
    return 0;
}
