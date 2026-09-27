#include "EntityBuilder.h"

#include <cassert>
#include <functional>

#include "BattleGrid.h"
#include "Engine.h"
#include "Prop.h"
#include "PropDef.h"
#include "PropFactory.h"
#include "Unit.h"
#include "UnitCatalog.h"
#include "UnitDef.h"
#include "UnitFactory.h"

namespace game {

EntityBuilder::EntityBuilder(SceneNode* root, grid::BattleGrid* grid,
                             SceneNode* shadowRoot,
                             std::deque<anim::Clip>* clipStorage)
    : root_(root), grid_(grid), shadowRoot_(shadowRoot), clipStorage_(clipStorage) {}

EntityBuilder::~EntityBuilder() = default;

Object* EntityBuilder::spawn(const SpawnDef& def) {
    switch (def.kind) {
        case ObjectKind::Unit: return spawnUnit(def);
        case ObjectKind::Prop: return spawnProp(def);
    }
    return nullptr;
}

void EntityBuilder::placeUnitAtCell(Unit* u) {
    if (!u || !grid_ || !u->node()) return;
    const math::Vec2 cell = u->cell();
    const math::Vec3 wp = grid_->cellToWorld(static_cast<int>(cell.x),
                                              static_cast<int>(cell.y));
    const float unitY = wp.y;
    anim::Clip* clip = const_cast<anim::Clip*>(u->node()->clip());
    if (clip != nullptr && !clip->keyframes.empty()) {
        const float baseY = clip->keyframes.front().translation.y;
        for (auto& kf : clip->keyframes) {
            const float dy = kf.translation.y - baseY;
            kf.translation.x = wp.x;
            kf.translation.y = unitY + dy;
            kf.translation.z = wp.z;
        }
        u->node()->setClip(clip);
    }
    u->node()->setLocal(math::Mat4::translation(math::Vec3(wp.x, unitY, wp.z))
                       * math::Mat4::scale(math::Vec3(0.56f)));
}

void EntityBuilder::createShadowMirror(Unit* u) {
    if (!u || !u->node() || !shadowRoot_) return;
    SceneNode* sn = shadowRoot_->addChild();
    sn->setLocal(u->node()->local());
    std::vector<std::pair<math::Mat4, SceneNode::Drawable>> flatDrawables;
    std::function<void(const SceneNode*, const math::Mat4&)> collect =
        [&](const SceneNode* n, const math::Mat4& parentXf) {
            math::Mat4 localXf = parentXf * n->local();
            for (const auto& d : n->drawables())
                flatDrawables.push_back({localXf, d});
            for (const auto& child : n->children())
                collect(child.get(), localXf);
        };
    for (const auto& child : u->node()->children())
        collect(child.get(), math::Mat4::identity());
    for (auto& [xf, dw] : flatDrawables) {
        SceneNode* dn = sn->addChild();
        dn->setLocal(xf);
        dn->addDrawable(dw);
    }
    shadowNodes_.push_back(sn);
}

Unit* EntityBuilder::spawnUnit(const SpawnDef& def) {
    if (!root_) return nullptr;

    unit::UnitDef cd;
    cd.name          = def.name;
    cd.faction       = def.faction;
    cd.baseStats     = def.baseStats;
    cd.baseColor     = def.baseColor;
    cd.selectedColor = def.selectedColor;
    cd.useRig        = def.useRig;
    cd.rootDropY     = def.rootDropY;
    cd.skeleton      = def.skeleton;
    if (def.clips) cd.clips = *def.clips;

    engine::RigResources rigRes{def.mesh, def.model, def.material};

    auto c = unit::spawn(root_, cd, rigRes);
    if (!c) return nullptr;

    if (def.cellX >= 0 && def.cellY >= 0) {
        c->setCell(math::Vec2(static_cast<float>(def.cellX),
                              static_cast<float>(def.cellY)));
    }

    if (grid_) c->setGrid(grid_);

    // Per-unit clip from template (stored in external storage that outlives EB)
    if (def.clipTemplate != nullptr && c->node() && clipStorage_) {
        clipStorage_->push_back(*def.clipTemplate);
        c->node()->setClip(&clipStorage_->back());
    }

    // Place at grid cell (positions clip keyframes + node transform)
    if (grid_ && def.cellX >= 0 && def.cellY >= 0) {
        placeUnitAtCell(c.get());
    }

    // Shadow mirror
    if (shadowRoot_) {
        createShadowMirror(c.get());
    }

    Unit* ptr = c.get();
    units_.push_back(std::move(c));
    return ptr;
}

Prop* EntityBuilder::spawnProp(const SpawnDef& def) {
    if (!root_) return nullptr;

    prop::PropDef pd;
    pd.name  = def.name;
    pd.color = def.propColor;
    pd.scale = def.propScale;

    engine::VisualDef v;
    v.mesh     = def.mesh;
    v.model    = def.model;
    v.material = def.material;

    auto p = prop::spawn(root_, pd, v);
    if (!p) return nullptr;

    if (def.cellX >= 0 && def.cellY >= 0 && grid_) {
        const math::Vec3 wp = grid_->cellToWorld(def.cellX, def.cellY);
        p->node()->setLocal(math::Mat4::translation(wp));
    }

    Prop* ptr = p.get();
    props_.push_back(std::move(p));
    return ptr;
}

Unit* EntityBuilder::spawnFromCatalog(
    const UnitCatalog& catalog,
    const std::string& catalogId,
    const SpawnDef& overrides) {
    const unit::UnitDef* cd = catalog.find(catalogId);
    if (!cd) return nullptr;

    SpawnDef s = overrides;
    s.kind       = ObjectKind::Unit;
    s.faction    = cd->faction;
    s.baseStats  = cd->baseStats;
    s.baseColor  = cd->baseColor;
    s.selectedColor = cd->selectedColor;
    s.skeleton   = cd->skeleton;
    s.clips      = &cd->clips;

    if (overrides.name.empty())       s.name       = cd->name;
    if (overrides.mesh == nullptr)    s.mesh       = nullptr;
    if (overrides.model == nullptr)   s.model      = nullptr;
    if (overrides.material == nullptr) s.material  = nullptr;

    return spawnUnit(s);
}

std::vector<Unit*> EntityBuilder::unitsAll() const {
    std::vector<Unit*> v;
    v.reserve(units_.size());
    for (const auto& c : units_) {
        if (c) v.push_back(c.get());
    }
    return v;
}

std::vector<Prop*> EntityBuilder::propsAll() const {
    std::vector<Prop*> v;
    v.reserve(props_.size());
    for (const auto& p : props_) {
        if (p) v.push_back(p.get());
    }
    return v;
}

std::vector<std::unique_ptr<Unit>> EntityBuilder::releaseUnits() {
    auto result = std::move(units_);
    units_.clear();
    return result;
}

std::vector<std::unique_ptr<Prop>> EntityBuilder::releaseProps() {
    auto result = std::move(props_);
    props_.clear();
    return result;
}

void EntityBuilder::populate(EntityRegistry& reg) {
    // Units into registry as Object*
    for (auto& u : units_) {
        if (u) reg.add(std::move(u));
    }
    units_.clear();

    // Props into registry as Object*
    for (auto& p : props_) {
        if (p) reg.add(std::move(p));
    }
    props_.clear();

    // Shadow nodes (not owned by builder — children of shadowRoot)
    for (size_t i = 0; i < shadowNodes_.size(); ++i) {
        reg.setShadow(i, shadowNodes_[i]);
    }
    shadowNodes_.clear();
}

void EntityBuilder::eachUnit(std::function<void(Unit&)> fn) {
    for (const auto& c : units_) {
        if (c) fn(*c);
    }
}

void EntityBuilder::eachProp(std::function<void(Prop&)> fn) {
    for (const auto& p : props_) {
        if (p) fn(*p);
    }
}

void EntityBuilder::updateAll(float dt) {
    for (auto& c : units_) { if (c) c->update(dt); }
    for (auto& p : props_) { if (p) p->update(dt); }
}

void EntityBuilder::saveAll(engine::SaveWriter& w) const {
    w.writeU32(static_cast<uint32_t>(units_.size()));
    for (const auto& c : units_) {
        if (c) c->save(w);
    }
    w.writeU32(static_cast<uint32_t>(props_.size()));
    for (const auto& p : props_) {
        if (p) p->save(w);
    }
}

void EntityBuilder::loadAll(engine::SaveReader& r) {
    uint32_t n = 0;
    r.readU32(n);
    units_.resize(n);
    for (auto& c : units_) {
        c = std::make_unique<Unit>(nullptr, unit::Faction::Neutral, unit::Stats{});
        c->load(r);
        if (grid_) c->setGrid(grid_);
    }
    r.readU32(n);
    props_.resize(n);
    for (auto& p : props_) {
        p = std::make_unique<Prop>(nullptr);
        p->load(r);
    }
}

void EntityBuilder::selfTest() {
    aout << "[entity-builder] self-test starting" << std::endl;

    const Mesh m{};
    SceneNode root;

    {
        EntityBuilder eb(&root);

        SpawnDef def;
        def.kind       = ObjectKind::Unit;
        def.name       = "Hero";
        def.faction    = unit::Faction::Player;
        def.baseStats  = {15, 6, 4, 3, 1};
        def.baseColor  = {0.9f, 0.3f, 0.2f};
        def.mesh       = &m;

        auto* obj = eb.spawn(def);
        assert(obj != nullptr);
        assert(obj->node() != nullptr);
        assert(obj->node()->parent() == &root);
        auto* ch = dynamic_cast<Unit*>(obj);
        assert(ch != nullptr);
        assert(ch->faction() == unit::Faction::Player);
        assert(ch->hp() == 15);
        assert(eb.units().size() == 1);
    }

    {
        EntityBuilder eb(&root);

        SpawnDef def;
        def.kind      = ObjectKind::Prop;
        def.name      = "Crate";
        def.propColor = {0.4f, 0.3f, 0.2f};
        def.propScale = {0.8f, 0.8f, 0.8f};
        def.mesh      = &m;

        auto* obj = eb.spawn(def);
        assert(obj != nullptr);
        auto* pr = dynamic_cast<Prop*>(obj);
        assert(pr != nullptr);
        assert(eb.props().size() == 1);
    }

    {
        EntityBuilder eb(&root);
        assert(eb.spawnUnit({})->node() != nullptr);
        assert(eb.units().size() == 1);
        assert(eb.spawnProp({})->node() != nullptr);
        assert(eb.props().size() == 1);
    }

    {
        EntityBuilder eb(&root);
        assert(eb.unitsAll().empty());
        assert(eb.propsAll().empty());
        eb.spawnUnit({.mesh = &m});
        eb.spawnProp({.mesh = &m});
        assert(eb.unitsAll().size() == 1);
        assert(eb.propsAll().size() == 1);
    }

    {
        EntityBuilder eb(&root);
        int charCount = 0;
        eb.eachUnit([&](Unit&) { charCount++; });
        assert(charCount == 0);

        eb.spawnUnit({.mesh = &m});
        eb.eachUnit([&](Unit& c) { charCount++; });
        assert(charCount == 1);
    }

    aout << "[entity-builder] self-test PASS" << std::endl;
}

} // namespace game
