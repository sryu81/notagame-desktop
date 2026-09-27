#include "EntityBuilder.h"
#include "SceneGraphBuilder.h"

#include "AndroidOut.h"
#include "Unit.h"
#include "Material.h"
#include "Mesh.h"
#include "Model.h"
#include "Prop.h"
#include "SceneNode.h"

#include <cassert>

int main() {
    aout << "host EntityBuilder driver starting" << std::endl;
    game::EntityBuilder::selfTest();

    {
        const Mesh m{};
        SceneNode root;
        game::EntityBuilder eb(&root);

        game::SpawnDef def;
        def.kind       = game::ObjectKind::Unit;
        def.name       = "HostHero";
        def.faction    = unit::Faction::Player;
        def.baseStats  = {10, 5, 2, 3, 1};
        def.baseColor  = {1.0f, 0.5f, 0.0f};
        def.mesh       = &m;

        auto* ch = eb.spawnUnit(def);
        assert(ch != nullptr);
        assert(ch->faction() == unit::Faction::Player);
        aout << "[host] EntityBuilder produced 1 Unit" << std::endl;
    }

    aout << "host EntityBuilder driver PASS" << std::endl;
    return 0;
}
