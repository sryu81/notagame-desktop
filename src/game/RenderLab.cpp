#include "RenderLab.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>

#include <GLES3/gl3.h>

#include "Engine.h"
#include "GameState.h"
#include "HubScene.h"

namespace {

constexpr float kFovY = 30.0f * math::PI / 180.0f;
constexpr float kNear = 0.1f;
constexpr float kFar  = 100.0f;
const math::Vec3 kEye(0.0f, 7.4f, 11.0f);
const math::Vec3 kTarget(0.0f, 0.5f, 0.0f);
const math::Vec3 kTexCubePos(-4.0f, 0.75f, 0.0f);
const math::Vec3 kProbeQuadColor(1.0f, 0.0f, 0.0f);
const math::Vec3 kProbeCubeColor(0.0f, 0.0f, 1.0f);
constexpr int kSphereRings = 12;
constexpr int kSphereSegments = 24;

const char* kTexture     = "TEXTURE MATERIAL";
const char* kPrimitives  = "PRIMITIVES";
const char* kSkinnedDraw = "SKINNED DRAW";
const char* kCpuSkin     = "CPU SKIN";
const char* kCasters     = "SHADOW CASTERS";
const char* kFollows     = "SHADOW FOLLOWS";
const char* kDepthWrite  = "DEPTHWRITE OFF";
const char* kBounds      = "BOUNDING SPHERE";
const char* kBoneClamp   = "BONE CLAMP";
const char* kHeavy       = "HEAVY MESH";

bool loadGlb(const Platform& p, const char* path, gltf::LoadResult& out) {
    std::vector<uint8_t> bytes;
    if (!p.readAsset(path, bytes)) return false;
    return gltf::load(bytes.data(), bytes.size(), out);
}

math::Vec3 ndcOf(const math::Mat4& vp, const math::Vec3& p) {
    const math::Vec4 c = vp * math::Vec4(p.x, p.y, p.z, 1.0f);
    return math::Vec3(c.x / c.w, c.y / c.w, c.z / c.w);
}

}

void RenderLab::enter(Renderer& r) {
    savedShadowExtent_ = r.shadowExtent();
    r.setShadowExtent(12.0f);
    r.setPickRoot(nullptr);
    loadContent(r);
    buildScene();

    ui_ = std::make_unique<ui::UIRoot>();
    ui_->init();
    checks_ = std::make_unique<game::LabChecklist>(math::Vec2(16, 16));
    for (const char* row : {kTexture, kPrimitives, kSkinnedDraw, kCpuSkin, kCasters, kFollows,
                            kDepthWrite, kBounds, kBoneClamp})
        checks_->add(row);
    resetCamera(r);
    buildUI(r);
    restartAutoDrive();

    Renderer* rptr = &r;
    auto alive = alive_;
    RenderLab* self = this;
    subs_.push_back(EventBus::on<PointerDownEvent>([alive, self, rptr](const PointerDownEvent& e) {
        if (!*alive || !self->ui_) return;
        if (ui::UIButton* btn = self->ui_->hitTestButton(math::Vec2(e.x, e.y))) {
            btn->handleClick();
            rptr->cancelPan();
        }
    }));
}

void RenderLab::exit(Renderer& r) {
    *alive_ = false;
    for (auto id : subs_) EventBus::unsubscribe(id);
    subs_.clear();
    r.setShadowExtent(savedShadowExtent_);
    ui_.reset();
    panel_.reset();
    checks_.reset();
    root_.reset();
    assets_.reset();
    texture_.reset();
}

void RenderLab::loadContent(Renderer& r) {
    assets_ = std::make_unique<game::LabAssets>(r.platform());
    hSphere_ = assets_->addMesh("sphere", primitives::makeUVSphere(1.0f, kSphereRings, kSphereSegments));

    gltf::LoadResult unit;
    if (loadGlb(r.platform(), "unit_v2.glb", unit)) {
        unitMesh_ = unit.mesh;
        unitSkeleton_ = std::move(unit.skeleton);
        for (auto& [name, clip] : unit.clips)
            if (name == "idle") unitIdle_ = clip;
        hUnit_ = assets_->addMesh("unit_v2", std::move(unit.mesh));
    }
    gltf::LoadResult giant;
    if (loadGlb(r.platform(), "giant.glb", giant))
        hGiant_ = assets_->addMesh("giant", std::move(giant.mesh));
    assets_->upload();

    texture_ = TextureAsset::loadAsset(r.platform(), "checker.png");
    const Shader* lit = assets_->registry().shader(assets_->registry().findShader("lit"));
    const asset::Handle hTex = assets_->registry().addMaterial("textured", {lit, texture_.get()});
    texMat_ = assets_->registry().material(hTex);
}

void RenderLab::buildScene() {
    const Material* lit = assets_->material("lit");
    const Material* unlit = assets_->material("unlit");
    const Material* skinned = assets_->material("skinned");
    const Mesh* plane = assets_->mesh("plane");
    const Model* planeModel = assets_->model("plane");
    const Mesh* cube = assets_->mesh("cube");
    const Model* cubeModel = assets_->model("cube");

    root_ = std::make_unique<SceneNode>();
    probes_.clear();
    const DepthProbe probeDefs[] = {{{-2.0f, 0.5f, 4.0f}, true}, {{2.0f, 0.5f, 4.0f}, false}};
    for (const auto& pd : probeDefs) {
        const math::Vec3 towardEye = (kEye - pd.cubePos).normalized();
        SceneNode* occluder = root_->addChild();
        occluder->setLocal(math::Mat4::translation(pd.cubePos + towardEye * 1.4f) *
                           math::Mat4::scale(math::Vec3(0.5f)));
        SceneNode::Drawable od{cube, cubeModel, kProbeQuadColor, unlit};
        od.depthWrite = pd.depthWrite;
        occluder->addDrawable(od);
        SceneNode* c = root_->addChild();
        c->setLocal(math::Mat4::translation(pd.cubePos) * math::Mat4::scale(math::Vec3(0.6f)));
        c->addDrawable({cube, cubeModel, kProbeCubeColor, unlit});
        probes_.push_back(pd);
    }

    SceneNode* ground = root_->addChild();
    ground->setLocal(math::Mat4::scale(math::Vec3(24.0f, 1.0f, 24.0f)));
    ground->addDrawable({plane, planeModel, math::Vec3(0.55f, 0.58f, 0.50f), lit});

    SceneNode* texCube = root_->addChild();
    texCube->setLocal(math::Mat4::translation(kTexCubePos) *
                      math::Mat4::scale(math::Vec3(1.5f)));
    texCube->addDrawable({cube, cubeModel, math::Vec3(1.0f), texMat_});

    SceneNode* sphere = root_->addChild();
    sphere->setLocal(math::Mat4::translation(math::Vec3(-1.3f, 1.0f, 0.0f)));
    sphere->addDrawable({assets_->mesh(hSphere_), assets_->model(hSphere_),
                         math::Vec3(0.85f, 0.55f, 0.25f), lit});

    if (hUnit_ != asset::kNull) {
        skinnedNode_ = root_->addChild();
        skinnedNode_->setLocal(math::Mat4::translation(math::Vec3(1.3f, 0.0f, 0.0f)) *
                               math::Mat4::scale(math::Vec3(1.2f)));
        skinnedNode_->setSkeleton(&unitSkeleton_);
        skinnedNode_->setSkeletalClip(&unitIdle_);
        skinnedNode_->addDrawable({assets_->mesh(hUnit_), assets_->model(hUnit_),
                                   math::Vec3(0.3f, 0.8f, 0.4f), skinned});
    }

    if (hGiant_ != asset::kNull) {
        const Mesh* gm = assets_->mesh(hGiant_);
        SceneNode* giant = root_->addChild();
        giant->setLocal(math::Mat4::translation(math::Vec3(4.0f, 0.0f, -2.0f)) *
                        math::Mat4::scale(math::Vec3(0.25f)));
        giant->addDrawable({gm, assets_->model(hGiant_), math::Vec3(0.6f, 0.6f, 0.7f), lit});
        gizmoNode_ = giant->addChild();
        gizmoNode_->setLocal(math::Mat4::translation(gm->boundingSphereCenter()) *
                             math::Mat4::scale(math::Vec3(gm->boundingSphereRadius())));
        SceneNode::Drawable gd{assets_->mesh(hSphere_), assets_->model(hSphere_),
                               math::Vec3(1.0f, 1.0f, 0.2f), unlit};
        gd.depthWrite = false;
        gizmoNode_->addDrawable(gd);
        gizmoNode_->drawables()[0].material = gizmo_ ? unlit : nullptr;
    }
}

void RenderLab::resetCamera(Renderer& r) {
    Camera& cam = r.camera();
    cam.setPerspective(kFovY, cam.aspect(), kNear, kFar);
    cam.setTerrainHeightFn({});
    cam.setForward(math::Vec3(0, 0, -1));
    cam.setPitchLimits(Camera::kMinPitchDeg, Camera::kMaxPitchDeg);
    cam.setYawLimits(Camera::kMinYawDeg, Camera::kMaxYawDeg);
    cam.setDistanceLimits(6.0f, 25.0f, 13.0f);
    cam.setViewLookAt(kEye, kTarget, math::Vec3(0, 1, 0));
    r.setClearColor(math::Vec3(0.45f, 0.70f, 0.95f));
}

void RenderLab::restartAutoDrive() {
    autoT_ = 0.0f;
    step_ = 0;
    readPixels_ = false;
    firstBones_.clear();
    if (checks_) checks_->reset();
}

void RenderLab::buildUI(Renderer& r) {
    const int w = r.width() > 0 ? r.width() : 1280;
    const int h = r.height() > 0 ? r.height() : 720;
    ui_->setScreenSize(w, h);
    const float rowH = std::clamp(static_cast<float>(h) / 18.0f, 36.0f, 72.0f);
    const float rowW = rowH * 5.5f;
    const int scale = rowH >= 56.0f ? 3 : 2;

    auto root = std::make_unique<ui::UIPanel>(math::Vec2(0, 0),
        math::Vec2(static_cast<float>(w), static_cast<float>(h)), math::Vec4(0, 0, 0, 0));

    Renderer* rptr = &r;
    game::GameState* gs = &gs_;
    panel_ = std::make_unique<game::DebugPanel>(
        math::Vec2(static_cast<float>(w) - 24.0f - rowW, 24.0f),
        math::Vec2(rowW, rowH), rowH * 0.15f, scale);
    panel_->addRow("AUTO", [this]() { return std::string(autoOn_ ? "ON" : "OFF"); },
                   [this]() { autoOn_ = !autoOn_; });
    panel_->addAction("RESTART CHECKS", [this, rptr]() {
        autoOn_ = true;
        resetCamera(*rptr);
        restartAutoDrive();
    });
    panel_->addRow("SUN", [this]() { return std::string(sunMoving_ ? "MOVING" : "FIXED"); },
                   [this]() { sunMoving_ = !sunMoving_; });
    panel_->addRow("BOUNDS GIZMO", [this]() { return std::string(gizmo_ ? "ON" : "OFF"); },
                   [this]() {
                       gizmo_ = !gizmo_;
                       if (gizmoNode_)
                           gizmoNode_->drawables()[0].material = gizmo_ ? assets_->material("unlit") : nullptr;
                   });
    panel_->addRow("SHADOW EXTENT", [rptr]() { return std::to_string(static_cast<int>(rptr->shadowExtent())); },
                   [rptr]() {
                       const float e = rptr->shadowExtent();
                       rptr->setShadowExtent(e >= 24.0f ? 6.0f : e * 2.0f);
                   });
    panel_->addAction("LOAD HEAVY MESH", [this, rptr]() { loadHeavyMesh(*rptr); });
    panel_->addAction("RESET VIEW", [this, rptr]() { resetCamera(*rptr); });
    panel_->addAction("BACK", [rptr, gs]() {
        rptr->sceneManager().replace(std::make_unique<HubScene>(*gs));
    });
    root->addChild(panel_->build());
    root->addChild(checks_->build());
    ui_->setRoot(std::move(root));
}

void RenderLab::onResize(int w, int h, Renderer& r) {
    if (w < 1 || h < 1 || !ui_) return;
    r.camera().setPerspective(kFovY, static_cast<float>(w) / static_cast<float>(h), kNear, kFar);
    buildUI(r);
}

math::Vec2 RenderLab::project(const math::Vec3& p, Renderer& r) const {
    const math::Vec3 n = ndcOf(r.camera().viewProjection(), p);
    return math::Vec2((n.x * 0.5f + 0.5f) * static_cast<float>(r.width()),
                      (0.5f - n.y * 0.5f) * static_cast<float>(r.height()));
}

void RenderLab::checkStatic(Renderer& r) {

    const Mesh* cube = assets_->mesh("cube");
    const Mesh* sphere = assets_->mesh(hSphere_);
    checks_->check(kPrimitives,
        cube && cube->indices.size() == 36 &&
        sphere && sphere->indices.size() == static_cast<size_t>(6 * kSphereRings * kSphereSegments) &&
        sphere->positions.size() == static_cast<size_t>((kSphereRings + 1) * (kSphereSegments + 1)));

    if (hGiant_ != asset::kNull) {
        const Mesh* gm = assets_->mesh(hGiant_);
        const math::Vec3 c = gm->boundingSphereCenter();
        const float radius = gm->boundingSphereRadius();
        float maxD = 0.0f;
        for (const auto& p : gm->positions) maxD = std::max(maxD, (p - c).length());
        checks_->check(kBounds, radius > 0.0f && maxD <= radius * 1.0001f && maxD >= radius * 0.99f);
    }

    const Shader* sk = assets_->registry().shader(assets_->registry().findShader("skinned"));
    std::vector<math::Mat4> many(60, math::Mat4::identity());
    while (glGetError() != GL_NO_ERROR) {}
    sk->activate();
    sk->setBoneMatrices(&many[0].m[0], static_cast<int>(many.size()));
    checks_->check(kBoneClamp, Shader::clampBoneCount(60) == Shader::kMaxBones &&
                               glGetError() == GL_NO_ERROR);
}

void RenderLab::checkSkinning() {
    if (!skinnedNode_) return;
    const auto& bm = skinnedNode_->boneMatrices();
    bool animated = false;
    for (size_t i = 0; i < bm.size() && i < firstBones_.size(); ++i)
        for (int k = 0; k < 16; ++k)
            if (std::abs(bm[i].m[k] - firstBones_[i].m[k]) > 1e-4f) animated = true;
    checks_->check(kSkinnedDraw, !bm.empty() && bm.size() == unitSkeleton_.bones.size() &&
                                 Shader::clampBoneCount(static_cast<int>(bm.size())) ==
                                     static_cast<int>(bm.size()) && animated);

    std::vector<math::Vec3> outPos, outNrm;
    anim::skinMesh(unitMesh_.positions, unitMesh_.normals, unitMesh_.boneIndices,
                   unitMesh_.boneWeights, bm, outPos, outNrm);
    bool match = outPos.size() == unitMesh_.positions.size();
    bool moved = false;
    for (size_t v = 0; match && v < outPos.size(); ++v) {
        math::Vec3 expected(0.0f);
        for (int k = 0; k < 4; ++k) {
            const float wgt = unitMesh_.boneWeights[v][k];
            if (wgt == 0.0f) continue;
            expected += bm[unitMesh_.boneIndices[v][k]].transformPoint(unitMesh_.positions[v]) * wgt;
        }
        if ((expected - outPos[v]).length() > 1e-4f) match = false;
        if ((outPos[v] - unitMesh_.positions[v]).length() > 1e-4f) moved = true;
    }
    checks_->check(kCpuSkin, match && moved);
}

void RenderLab::readTexturePixels(Renderer& r) {
    std::vector<int> blues;
    for (int i = 0; i <= 8; ++i) {
        const float t = -0.6f + 1.2f * static_cast<float>(i) / 8.0f;
        const math::Vec2 s = project(kTexCubePos + math::Vec3(t, 0.75f, 0.0f), r);
        uint8_t px[4] = {};
        glReadPixels(static_cast<GLint>(s.x), r.height() - 1 - static_cast<GLint>(s.y), 1, 1,
                     GL_RGBA, GL_UNSIGNED_BYTE, px);
        blues.push_back(px[2]);
    }
    float mean = 0.0f;
    for (int b : blues) mean += static_cast<float>(b);
    mean /= static_cast<float>(blues.size());
    int flips = 0;
    for (size_t i = 1; i < blues.size(); ++i) {
        const float a = static_cast<float>(blues[i - 1]) - mean;
        const float b = static_cast<float>(blues[i]) - mean;
        if (a * b < 0.0f && std::abs(a) > 10.0f && std::abs(b) > 10.0f) ++flips;
    }
    const bool bound = texture_ && texture_->getTextureID() != 0 && texMat_ && texMat_->texture == texture_.get();
    if (flips < 4) aout << "[renderlab] texture alternations " << flips << std::endl;
    checks_->check(kTexture, bound && flips >= 4);
}

void RenderLab::readDepthProbes(Renderer& r) {
    bool ok = true;
    for (const auto& pd : probes_) {
        const math::Vec2 s = project(pd.cubePos, r);
        uint8_t px[4] = {};
        glReadPixels(static_cast<GLint>(s.x), r.height() - 1 - static_cast<GLint>(s.y), 1, 1,
                     GL_RGBA, GL_UNSIGNED_BYTE, px);
        const bool seesQuad = px[0] > 200 && px[2] < 60;
        const bool seesCube = px[2] > 200 && px[0] < 60;
        const bool expected = pd.depthWrite ? seesQuad : seesCube;
        if (!expected)
            aout << "[renderlab] depth probe depthWrite=" << pd.depthWrite << " pixel=("
                 << int(px[0]) << "," << int(px[1]) << "," << int(px[2]) << ")" << std::endl;
        ok = ok && expected;
    }
    checks_->check(kDepthWrite, ok);
}

void RenderLab::loadHeavyMesh(Renderer& r) {
    checks_->add(kHeavy);
    uiDirty_ = true;
    const auto t0 = std::chrono::steady_clock::now();
    gltf::LoadResult heavy;
    const bool ok = loadGlb(r.platform(), "robot1.glb", heavy);
    const float ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (!ok) {
        aout << "[renderlab] robot1.glb not in assets/ (moved to art/; copy it back to test)" << std::endl;
        return;
    }
    aout << "[renderlab] robot1.glb " << heavy.mesh.indices.size() / 3 << " tris parsed in "
         << ms << " ms" << std::endl;
    checks_->check(kHeavy, heavy.mesh.indices.size() > 0);
}

void RenderLab::autoDrive(float dt, Renderer& r) {
    autoT_ += dt;
    switch (step_) {
        case 0:
            checkStatic(r);
            if (skinnedNode_) firstBones_ = skinnedNode_->boneMatrices();
            step_ = 1;
            break;
        case 1:
            if (autoT_ < 1.0f) break;
            checks_->check(kCasters, r.shadowStats().draws > 0 && r.shadowStats().skinnedDraws >= 1);
            step_ = 2;
            break;
        case 2:
            if (autoT_ < 1.5f) break;
            readPixels_ = true;
            step_ = 3;
            break;
        case 3:
            if (autoT_ < 2.0f) break;
            r.camera().setTarget(followProbe_);
            step_ = 4;
            break;
        case 4: {
            if (autoT_ < 2.2f) break;
            const math::Mat4& lvp = r.lightViewProjection();
            const math::Vec3 probe = ndcOf(lvp, followProbe_);
            const math::Vec3 origin = ndcOf(lvp, math::Vec3(0.0f));
            const bool inside = std::abs(probe.x) < 1.0f && std::abs(probe.y) < 1.0f &&
                                std::abs(probe.z) < 1.0f;
            const bool originOut = std::abs(origin.x) > 1.0f || std::abs(origin.y) > 1.0f;
            checks_->check(kFollows, inside && originOut);
            resetCamera(r);
            aout << "[renderlab] auto-drive done: " << checks_->passedCount() << "/"
                 << checks_->size() << " checks green" << std::endl;
            step_ = 5;
            break;
        }
        default:
            break;
    }
}

void RenderLab::update(float dt, Renderer& r) {
    time_ += dt;
    const float a = sunMoving_ ? time_ * 0.4f : 0.6f;
    r.setFrameLight(math::Vec3(std::cos(a) * 0.5f, 0.8f, std::sin(a) * 0.5f).normalized(), 0.3f);
    if (root_) root_->update(dt);
    if (autoOn_) autoDrive(dt, r);
    if (autoOn_ && step_ >= 1 && step_ < 5) checkSkinning();
    if (uiDirty_) {
        uiDirty_ = false;
        buildUI(r);
    }
    if (panel_) panel_->refresh();
}

void RenderLab::render(Renderer& r) {
    r.renderSceneGraph(root_.get());
    if (readPixels_) {
        readPixels_ = false;
        readDepthProbes(r);
        readTexturePixels(r);
    }
    r.renderUI(ui_.get());
}
