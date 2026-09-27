#include "SceneGraphBuilder.h"

#include "AndroidOut.h"
#include "Material.h"
#include "Mesh.h"
#include "Model.h"
#include "SceneNode.h"

#include <cassert>
#include <functional>

namespace engine {
namespace scenegraph {

SceneNode* buildMeshNode(
    SceneNode& parent,
    const VisualDef& v,
    const math::Mat4& xform)
{
    SceneNode* node = parent.addChild();
    node->setLocal(xform);
    if (v.mesh) {
        node->addDrawable({
            v.mesh,
            v.model,
            v.baseColor,
            v.material
        });
    }
    return node;
}

SceneNode* buildSkinnedNode(
    SceneNode& parent,
    const VisualDef& v,
    const math::Mat4& xform)
{
    SceneNode* node = buildMeshNode(parent, v, xform);
    if (v.skeleton) node->setSkeleton(v.skeleton);
    if (v.idleClip) node->setSkeletalClip(v.idleClip);
    return node;
}

SceneNode* buildRigNode(
    SceneNode& parent,
    const RigResources& res,
    const RigDef& r,
    const math::Mat4& xform)
{
    constexpr float kHipY        = 1.0f;
    constexpr float kBodyH       = 0.8f;
    constexpr float kBodyCenter  = kHipY + kBodyH * 0.5f;
    constexpr float kShoulderY   = kHipY + kBodyH;
    constexpr float kNeckY       = kShoulderY + 0.05f;
    constexpr float kUpperLeg    = 0.5f;
    constexpr float kLowerLeg    = 0.5f;
    constexpr float kUpperArm    = 0.4f;
    constexpr float kLowerArm    = 0.4f;
    constexpr float kHipHalfW    = 0.18f;
    constexpr float kShldrHalfW  = 0.34f;
    constexpr float kBodyW       = 0.46f;
    constexpr float kBodyD       = 0.32f;
    constexpr float kLimbThick   = 0.14f;
    constexpr float kHandSize    = 0.13f;
    constexpr float kFootW       = 0.22f;
    constexpr float kFootH       = 0.08f;
    constexpr float kFootD       = 0.30f;

    const math::Vec3 limbColor = r.bodyColor * 0.6f;
    const math::Vec3 headColor(
        r.bodyColor.x + 0.3f,
        r.bodyColor.y + 0.3f,
        r.bodyColor.z + 0.3f);
    const math::Vec3 footColor = r.bodyColor * 0.4f;

    SceneNode* root = parent.addChild();
    root->setLocal(xform * math::Mat4::translation(math::Vec3(0.0f, -r.rootDropY, 0.0f)));

    auto addBox = [&](SceneNode* p, const math::Vec3& localPos,
                      const math::Vec3& scale, const math::Vec3& color) {
        SceneNode* n = p->addChild();
        n->setLocal(math::Mat4::translation(localPos) * math::Mat4::scale(scale));
        n->addDrawable({res.cubeMesh, res.cubeModel, color, res.material});
    };

    addBox(root, math::Vec3(0.0f, kBodyCenter, 0.0f),
           math::Vec3(kBodyW, kBodyH, kBodyD), r.bodyColor);

    auto buildLeg = [&](float xSide) {
        SceneNode* hip = root->addChild();
        hip->setLocal(math::Mat4::translation(math::Vec3(xSide, kHipY, 0.0f)));
        addBox(hip, math::Vec3(0.0f, -kUpperLeg * 0.5f, 0.0f),
               math::Vec3(kLimbThick, kUpperLeg, kLimbThick), limbColor);
        SceneNode* knee = hip->addChild();
        knee->setLocal(math::Mat4::translation(math::Vec3(0.0f, -kUpperLeg, 0.0f)));
        addBox(knee, math::Vec3(0.0f, -kLowerLeg * 0.5f, 0.0f),
               math::Vec3(kLimbThick * 0.9f, kLowerLeg, kLimbThick * 0.9f), limbColor);
        addBox(knee, math::Vec3(0.0f, -kLowerLeg, 0.05f),
               math::Vec3(kFootW, kFootH, kFootD), footColor);
    };
    buildLeg(-kHipHalfW);
    buildLeg( kHipHalfW);

    auto buildArm = [&](float xSide) {
        SceneNode* sh = root->addChild();
        sh->setLocal(math::Mat4::translation(math::Vec3(xSide, kShoulderY, 0.0f)));
        addBox(sh, math::Vec3(0.0f, -kUpperArm * 0.5f, 0.0f),
               math::Vec3(kLimbThick * 0.8f, kUpperArm, kLimbThick * 0.8f), limbColor);
        SceneNode* el = sh->addChild();
        el->setLocal(math::Mat4::translation(math::Vec3(0.0f, -kUpperArm, 0.0f)));
        addBox(el, math::Vec3(0.0f, -kLowerArm * 0.5f, 0.0f),
               math::Vec3(kLimbThick * 0.7f, kLowerArm, kLimbThick * 0.7f), limbColor);
        addBox(el, math::Vec3(0.0f, -kLowerArm, 0.0f),
               math::Vec3(kHandSize, kHandSize * 0.7f, kHandSize), headColor);
    };
    buildArm(-kShldrHalfW);
    buildArm( kShldrHalfW);

    SceneNode* neck = root->addChild();
    neck->setLocal(math::Mat4::translation(math::Vec3(0.0f, kNeckY, 0.0f)));
    addBox(neck, math::Vec3(0.0f, 0.0f, 0.0f),
           math::Vec3(0.12f, 0.12f, 0.12f), limbColor);
    addBox(neck, math::Vec3(0.0f, 0.25f, 0.0f),
           math::Vec3(0.30f, 0.30f, 0.30f), headColor);

    return root;
}

void selfTest() {
    aout << "[scenegraph] self-test starting" << std::endl;

    {
        SceneNode parent;
        const Mesh m{};
        VisualDef v;
        v.mesh  = &m;

        const math::Mat4 xf = math::Mat4::translation(math::Vec3(1, 2, 3));
        SceneNode* n = buildMeshNode(parent, v, xf);

        assert(n != nullptr);
        assert(n->parent() == &parent);
        assert(math::approxEqual(n->local().m[12], 1.0f, 1e-4f));
        assert(math::approxEqual(n->local().m[13], 2.0f, 1e-4f));
        assert(math::approxEqual(n->local().m[14], 3.0f, 1e-4f));
        assert(n->drawables().size() == 1);
        assert(n->drawables()[0].mesh == &m);
        assert(n->drawables()[0].model == nullptr);
        assert(n->skeleton() == nullptr);
        assert(n->skeletalClip() == nullptr);
    }

    {
        SceneNode parent;
        const Mesh m{};
        anim::Skeleton skel;
        skel.bones.push_back({-1, math::Mat4::identity()});

        anim::SkeletalClip clip;
        clip.tracks.push_back({0, {}});

        VisualDef v;
        v.mesh = &m;
        v.skeleton = &skel;
        v.idleClip = &clip;

        SceneNode* n = buildSkinnedNode(parent, v, math::Mat4::identity());
        assert(n != nullptr);
        assert(n->skeleton() == &skel);
        assert(n->skeletalClip() == &clip);
        assert(n->drawables().size() == 1);
    }

    {
        SceneNode parent;
        VisualDef v;
        SceneNode* n = buildMeshNode(parent, v, math::Mat4::identity());
        assert(n != nullptr);
        assert(n->drawables().empty());
    }

    {
        SceneNode parent;
        const Mesh m{};
        const Material mat{};
        VisualDef v;
        v.mesh = &m;
        v.material = &mat;

        SceneNode* n = buildMeshNode(parent, v, math::Mat4::identity());
        assert(n != nullptr);
        assert(n->drawables()[0].material == &mat);
    }

    {
        SceneNode parent;
        const Mesh m{};
        VisualDef v;
        v.mesh  = &m;

        SceneNode* n1 = buildMeshNode(parent, v, math::Mat4::identity());
        SceneNode* n2 = buildMeshNode(parent, v, math::Mat4::identity());
        assert(n1 != nullptr);
        assert(n2 != nullptr);
        assert(n1 != n2);
        assert(parent.children().size() == 2);
    }

    {
        SceneNode parent;
        const Mesh m{};
        const Model* mod = nullptr;
        const Material mat{};
        RigResources res{&m, mod, &mat};
        RigDef r;
        r.bodyColor = {0.4f, 0.8f, 0.2f};
        r.rootDropY = 0.25f;

        SceneNode* root = buildRigNode(parent, res, r, math::Mat4::identity());
        assert(root != nullptr);
        assert(root->parent() == &parent);
        assert(math::approxEqual(root->local().m[13], -0.25f, 1e-4f));
        const size_t expected = 1 + 2 + 2 + 1;
        assert(root->children().size() == expected);
        size_t totalDrawables = 0;
        std::function<void(SceneNode*)> countDraw = [&](SceneNode* n) {
            for (const auto& d : n->drawables()) {
                assert(d.material == &mat);
                ++totalDrawables;
            }
            for (const auto& c : n->children()) countDraw(c.get());
        };
        countDraw(root);
        const size_t expectedDrawables = 1 + 2 * 3 + 2 * 3 + 2;
        assert(totalDrawables == expectedDrawables);
        SceneNode* hipL = root->children()[1].get();
        assert(hipL->children().size() == 2);
        SceneNode* kneeL = hipL->children()[1].get();
        assert(kneeL->children().size() == 2);
    }

    aout << "[scenegraph] self-test PASS" << std::endl;
}

} // namespace scenegraph
} // namespace engine
