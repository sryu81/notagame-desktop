#include "UnitSprites.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "Engine.h"
#include "LabAssets.h"
#include "Unit.h"
#include "UnitModels.h"
#include "UnitTypes.h"

namespace game {

namespace {

void collectDrawables(SceneNode* n, std::vector<std::pair<SceneNode*, std::deque<SceneNode::Drawable>>>& out) {
    if (!n->drawables().empty()) {
        out.emplace_back(n, std::move(n->drawables()));
        n->drawables().clear();
    }
    for (auto& c : n->children()) collectDrawables(c.get(), out);
}

}

std::string UnitSprites::sheetName(const std::string& modelFile) {
    const size_t dot = modelFile.rfind('.');
    return (dot == std::string::npos ? modelFile : modelFile.substr(0, dot)) + "_sprite";
}

void UnitSprites::load(const Platform& p, LabAssets& assets) {
    sheets_.clear();
    for (const UnitType& t : unitTypes()) {
        if (!t.model) continue;
        if (std::any_of(sheets_.begin(), sheets_.end(), [&t](const SheetAsset& a) { return a.model == t.model; }))
            continue;
        const std::string name = sheetName(t.model);
        SheetAsset a;
        a.model = t.model;
        std::vector<uint8_t> bytes;
        std::string err;
        if (!p.readAsset(name + ".sheet", bytes) ||
            !sprite::parse(std::string(bytes.begin(), bytes.end()), a.sheet, &err)) {
            aout << "[sprites] " << name << ".sheet: " << (err.empty() ? "missing" : err) << std::endl;
            continue;
        }
        a.texture = TextureAsset::loadAsset(p, name + ".png", true);
        if (!a.texture) continue;
        a.sheet.atlasW = a.texture->width();
        a.sheet.atlasH = a.texture->height();
        a.quad = assets.addMesh("quad_" + name, sprite::makeQuad(a.sheet.anchorV()));
        sheets_.push_back(std::move(a));
    }
}

void UnitSprites::createMaterials(LabAssets& assets) {
    asset::Registry& reg = assets.registry();
    const Shader* sh = reg.shader(reg.findShader("sprite"));
    for (SheetAsset& a : sheets_) a.material = {sh, a.texture.get()};
}

void UnitSprites::attach(SceneNode* root, const UnitModels& models, const std::vector<Unit*>& units,
                         const LabAssets& assets) {
    bound_.clear();
    seenClips_ = 0;
    for (Unit* u : units) {
        if (!u || !u->node() || !u->visualNode()->skeleton()) continue;
        const SheetAsset* sheet = nullptr;
        for (const SheetAsset& a : sheets_) {
            const UnitModels::Rig* rig = models.rig(a.model);
            if (rig && &rig->skeleton == u->visualNode()->skeleton()) { sheet = &a; break; }
        }
        if (!sheet || !sheet->material.shader) continue;
        Binding b;
        b.unit = u;
        b.sheet = sheet;
        b.card = root->addChild();
        b.cardDrawable = {assets.mesh(sheet->quad), assets.model(sheet->quad), math::Vec3(1.0f), &sheet->material};
        b.cardDrawable.alphaCutout = true;
        bound_.push_back(std::move(b));
    }
    for (Binding& b : bound_) apply(b);
}

void UnitSprites::apply(Binding& b) const {
    if (enabled_) {
        if (b.hidden.empty()) collectDrawables(b.unit->node(), b.hidden);
        if (b.card->drawables().empty()) b.card->addDrawable(b.cardDrawable);
    } else {
        for (auto& [node, ds] : b.hidden) node->drawables() = std::move(ds);
        b.hidden.clear();
        b.card->drawables().clear();
    }
}

void UnitSprites::setEnabled(bool on) {
    enabled_ = on;
    for (Binding& b : bound_) apply(b);
}

int UnitSprites::clipIndex(const Unit& u) {
    const SceneNode* v = u.visualNode();
    const anim::SkeletalClip* c = v ? v->skeletalClip() : nullptr;
    const unit::ClipSet& s = u.clips();
    const anim::SkeletalClip* order[] = {s.idle, s.walk, s.attack, s.hurt, s.death};
    for (int i = 0; i < 5; ++i)
        if (c && c == order[i]) return i;
    return u.isDead() ? 4 : 0;
}

int UnitSprites::frameAt(const sprite::Clip& c, float time, float clipDuration) {
    if (c.frames <= 1) return 0;
    const float t = std::max(time, 0.0f);
    if (c.loop) return static_cast<int>(std::floor(t * c.fps)) % c.frames;
    if (clipDuration <= 0.0f) return c.frames - 1;
    const float s = std::min(t / clipDuration, 1.0f);
    return std::min(static_cast<int>(std::lround(s * static_cast<float>(c.frames - 1))), c.frames - 1);
}

void UnitSprites::update(const Camera& cam) {
    if (!enabled_) return;
    const math::Mat4& view = cam.view();
    const float camYaw = sprite::cameraYawDeg(view);
    const float pitch = sprite::cameraPitchDeg(view);
    for (Binding& b : bound_) {
        const sprite::Sheet& s = b.sheet->sheet;
        const math::Mat4 m = b.unit->node()->world();
        const math::Vec3 feet(m.m[12], m.m[13], m.m[14]);
        const math::Vec3 fwd = m.transformDirection(math::Vec3(0, 0, -1));
        b.facing = sprite::facingIndex(math::degrees(std::atan2(fwd.x, fwd.z)), camYaw);
        b.clip = clipIndex(*b.unit);
        seenClips_ |= 1 << b.clip;
        const sprite::Clip* c = s.find(UnitModels::clipNames()[b.clip]);
        if (!c) c = &s.clips.front();
        const SceneNode* v = b.unit->visualNode();
        const anim::SkeletalClip* sk = v->skeletalClip();
        b.frame = frameAt(*c, v->time(), sk ? anim::duration(*sk) : c->duration());
        auto& d = b.card->drawables().front();
        d.uvRect = sprite::frameRect(s, *c, b.frame, sprite::viewFor(s, b.facing));
        b.card->setLocal(sprite::billboard(s, sprite::snapToTexels(feet, s.pixelsPerUnit), camYaw, pitch, pitch));
    }
}

bool UnitSprites::allBound(const std::vector<Unit*>& units) const {
    for (const Unit* u : units) {
        if (!u || !u->visualNode() || !u->visualNode()->skeleton()) continue;
        if (std::none_of(bound_.begin(), bound_.end(), [u](const Binding& b) { return b.unit == u; })) return false;
    }
    return !bound_.empty();
}

bool UnitSprites::inSync() const {
    if (!enabled_ || bound_.empty()) return false;
    for (const Binding& b : bound_) {
        if (b.card->drawables().size() != 1 || b.hidden.empty() || !b.unit->visualNode()->drawables().empty())
            return false;
        if (b.clip != clipIndex(*b.unit)) return false;
        const sprite::Sheet& s = b.sheet->sheet;
        const sprite::Clip* c = s.find(UnitModels::clipNames()[b.clip]);
        if (!c || b.frame < 0 || b.frame >= c->frames) return false;
        const math::Vec4 want = sprite::frameRect(s, *c, b.frame, sprite::viewFor(s, b.facing));
        if (!math::approxEqual(b.card->drawables().front().uvRect, want, 1e-6f)) return false;
    }
    return true;
}

void UnitSprites::selfTest() {
    assert(sheetName("knight.glb") == "knight_sprite");
    assert(sheetName("knight_mounted.glb") == "knight_mounted_sprite");
    assert(sheetName("rig") == "rig_sprite");

    sprite::Clip walk;
    walk.frames = 8;
    walk.fps = 8.0f;
    walk.loop = true;
    assert(frameAt(walk, 0.0f, 1.0f) == 0);
    assert(frameAt(walk, 0.26f, 1.0f) == 2);
    assert(frameAt(walk, 1.13f, 1.0f) == 1);
    assert(frameAt(walk, -1.0f, 1.0f) == 0);

    sprite::Clip death;
    death.frames = 8;
    death.fps = 10.0f;
    death.loop = false;
    assert(frameAt(death, 0.0f, 1.4f) == 0);
    assert(frameAt(death, 0.7f, 1.4f) == 4);
    assert(frameAt(death, 1.4f, 1.4f) == 7);
    assert(frameAt(death, 9.0f, 1.4f) == 7);
    assert(frameAt(death, 0.5f, 0.0f) == 7);

    sprite::Clip still;
    assert(frameAt(still, 3.0f, 1.0f) == 0);

    SceneNode root;
    SceneNode* body = root.addChild();
    SceneNode* skin = body->addChild();
    skin->addDrawable({});
    body->addDrawable({});
    std::vector<std::pair<SceneNode*, std::deque<SceneNode::Drawable>>> hidden;
    collectDrawables(&root, hidden);
    assert(hidden.size() == 2 && body->drawables().empty() && skin->drawables().empty());

    aout << "UnitSprites selfTest: PASS" << std::endl;
}

} // namespace game
