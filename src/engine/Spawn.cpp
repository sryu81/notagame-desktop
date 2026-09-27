#include "Spawn.h"

#include "AndroidOut.h"
#include "Mesh.h"
#include "SceneNode.h"

#include <cassert>
#include <string>
#include <utility>

namespace engine {
namespace scenegraph {
namespace {

struct FakeUnit {
    FakeUnit(SceneNode* n, int hp, std::string name)
        : node_(n), hp_(hp), name_(std::move(name)) {}
    SceneNode* node_;
    int hp_;
    std::string name_;
};

} // namespace

void spawnSelfTest() {
    aout << "[scenegraph-spawn] self-test starting" << std::endl;

    {
        SceneNode parent;
        const Mesh m{};
        VisualDef v;
        v.mesh = &m;

        const math::Mat4 xf = math::Mat4::translation(math::Vec3(2, 0, 5));
        auto u = spawn<FakeUnit>(parent, v, xf, 100, "knight");

        assert(u != nullptr);
        assert(u->node_ != nullptr);
        assert(u->node_->parent() == &parent);
        assert(math::approxEqual(u->node_->local().m[14], 5.0f, 1e-4f));
        assert(u->hp_ == 100);
        assert(u->name_ == "knight");
        assert(u->node_->drawables().size() == 1);
    }

    {
        SceneNode parent;
        const Mesh m{};
        anim::Skeleton skel;
        anim::SkeletalClip clip;
        VisualDef v;
        v.mesh = &m;
        v.skeleton = &skel;
        v.idleClip = &clip;

        auto u = spawn<FakeUnit>(parent, v, math::Mat4::identity(), 50, "mage");
        assert(u != nullptr);
        assert(u->hp_ == 50);
        assert(u->name_ == "mage");
        assert(u->node_->skeleton() == &skel);
        assert(u->node_->skeletalClip() == &clip);
    }

    {
        SceneNode parent;
        VisualDef v;
        auto u = spawn<FakeUnit>(parent, v, math::Mat4::identity(), 0, "ghost");
        assert(u != nullptr);
        assert(u->hp_ == 0);
        assert(u->node_->drawables().empty());
    }

    aout << "[scenegraph-spawn] self-test PASS" << std::endl;
}

} // namespace scenegraph
} // namespace engine
