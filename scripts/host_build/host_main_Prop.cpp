#include "Prop.h"
#include "PropDef.h"
#include "PropFactory.h"
#include "SceneGraphBuilder.h"
#include "SceneNode.h"

#include "AndroidOut.h"
#include "Material.h"
#include "Mesh.h"
#include "Model.h"

#include <cassert>

int main() {
    aout << "host Prop driver starting" << std::endl;

    game::Prop::selfTest();

    {
        prop::PropDef def;
        def.name  = "Crate";
        def.color = math::Vec3(0.7f, 0.5f, 0.3f);
        def.scale = math::Vec3(1.0f, 1.0f, 1.0f);

        const Mesh m{};
        engine::VisualDef v;
        v.mesh = &m;

        SceneNode root;
        auto p = prop::spawn(&root, def, v);
        assert(p != nullptr);
        assert(p->node() != nullptr);
        assert(p->node()->parent() == &root);
        assert(p->scale().x > 0.0f);
        assert(!p->isSelected());
        aout << "[host] prop::spawn built 1 mesh node under root" << std::endl;
    }

    aout << "host Prop driver PASS" << std::endl;
    return 0;
}
