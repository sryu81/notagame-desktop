#include "LevelLab.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

#include "Engine.h"
#include "GameState.h"
#include "HubScene.h"
#include "Prop.h"
#include "Unit.h"
#include "UnitDef.h"
#include "UnitTypes.h"

namespace {

constexpr float kFovY = 30.0f * math::PI / 180.0f;
constexpr float kNear = 0.1f;
constexpr float kFar  = 120.0f;
const math::Vec3 kEye(0.0f, 17.0f, 21.0f);
const math::Vec3 kLook(0.0f, 0.0f, 1.0f);
constexpr float kUnitScale = 0.45f;

const char* kRoundTrip = "V3 ROUND TRIP";
const char* kV2Compat  = "V2 COMPAT";
const char* kByCatalog = "UNITS BY CATALOG";
const char* kPropsRow  = "PROPS";
const char* kTerrain   = "TERRAIN MATCHES";
const char* kWinRow    = "WIN CONDITION";

const char* kFiles[] = {"level_01.lvl", "level_02.lvl", "level_03.lvl", "level_v2.lvl"};
const char* kNames[] = {"FLAT", "HILL", "FORT", "V2"};

bool heightAt(const Mesh& m, float x, float z, float& y) {
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        const math::Vec3& a = m.positions[m.indices[i]];
        const math::Vec3& b = m.positions[m.indices[i + 1]];
        const math::Vec3& c = m.positions[m.indices[i + 2]];
        const float det = (b.z - c.z) * (a.x - c.x) + (c.x - b.x) * (a.z - c.z);
        if (std::abs(det) < 1e-9f) continue;
        const float l0 = ((b.z - c.z) * (x - c.x) + (c.x - b.x) * (z - c.z)) / det;
        const float l1 = ((c.z - a.z) * (x - c.x) + (a.x - c.x) * (z - c.z)) / det;
        const float l2 = 1.0f - l0 - l1;
        if (l0 < -1e-5f || l1 < -1e-5f || l2 < -1e-5f) continue;
        y = l0 * a.y + l1 * b.y + l2 * c.y;
        return true;
    }
    return false;
}

}

LevelLab::LevelLab(game::GameState& gs) : gs_(gs) {}
LevelLab::~LevelLab() = default;

void LevelLab::buildCatalog() {
    catalog_.clear();
    auto def = [this](const char* id, unit::Faction f, unit::Stats st, math::Vec3 color) {
        unit::UnitDef d;
        d.name = id;
        d.faction = f;
        d.baseStats = st;
        d.baseColor = color;
        d.baseScale = kUnitScale;
        catalog_.registerDef(id, d);
    };
    for (const auto& t : game::unitTypes()) def(t.id, t.faction, t.stats, t.color);
}

void LevelLab::loadFiles(Renderer& r) {
    entries_.clear();
    for (const char* f : kFiles) {
        Entry e;
        e.file = f;
        if (r.platform().readAsset(f, e.bytes))
            e.loaded = level::load(e.bytes.data(), e.bytes.size(), e.level);
        if (e.loaded)
            e.terrain = assets_->addMesh(e.file, primitives::makeCellTerrain(
                e.level.width, e.level.height, e.level.cellSize, e.level.cellY));
        aout << "[levellab] " << f << (e.loaded ? " loaded" : " FAILED") << std::endl;
        entries_.push_back(std::move(e));
    }
}

void LevelLab::enter(Renderer& r) {
    r.setPickRoot(nullptr);
    assets_ = std::make_unique<game::LabAssets>(r.platform());
    loadFiles(r);
    assets_->upload();
    buildCatalog();
    root_ = std::make_unique<SceneNode>();

    ui_ = std::make_unique<ui::UIRoot>();
    ui_->init();
    checks_ = std::make_unique<game::LabChecklist>(math::Vec2(16, 16));
    for (const char* row : {kRoundTrip, kV2Compat, kByCatalog, kPropsRow, kTerrain, kWinRow})
        checks_->add(row);
    resetCamera(r);
    buildUI(r);
    restartAutoDrive();

    Renderer* rptr = &r;
    auto alive = alive_;
    LevelLab* self = this;
    subs_.push_back(EventBus::on<PointerDownEvent>([alive, self, rptr](const PointerDownEvent& e) {
        if (!*alive || !self->ui_) return;
        if (ui::UIButton* btn = self->ui_->hitTestButton(math::Vec2(e.x, e.y))) {
            btn->handleClick();
            rptr->cancelPan();
        }
    }));
}

void LevelLab::exit(Renderer& r) {
    *alive_ = false;
    for (auto id : subs_) EventBus::unsubscribe(id);
    subs_.clear();
    builder_.reset();
    units_.clear();
    props_.clear();
    grid_.reset();
    ui_.reset();
    panel_.reset();
    checks_.reset();
    root_.reset();
    catalog_.clear();
    entries_.clear();
    assets_.reset();
}

void LevelLab::showLevel(int i) {
    builder_.reset();
    units_.clear();
    props_.clear();
    grid_.reset();
    root_->clearChildren();
    terrainMesh_ = nullptr;
    current_ = i;
    framesShown_ = 0;
    const Entry& e = entries_[i];
    if (!e.loaded) {
        info_ = e.file + " FAILED TO LOAD";
        if (infoText_) infoText_->setText(info_);
        return;
    }
    const level::Level& lvl = e.level;
    grid_ = game::LevelSpawner::makeGrid(lvl);
    const Material* lit = assets_->material("lit");
    terrainMesh_ = assets_->mesh(e.terrain);
    grid_->attachTerrainMesh(root_.get(), terrainMesh_, assets_->model(e.terrain), lit);
    builder_ = std::make_unique<game::EntityBuilder>(root_.get(), grid_.get());
    const game::LevelSpawner::Visuals vis{assets_->mesh("cube"), assets_->model("cube"), lit};
    props_ = game::LevelSpawner::spawnProps(lvl, *builder_, *grid_, vis);
    units_ = game::LevelSpawner::spawnUnits(lvl, catalog_, *builder_, vis);
    auto tiles = grid_->attachTo(root_.get(), assets_->mesh("plane"), assets_->model("plane"),
                                 math::Vec3(0.66f, 0.70f, 0.52f), math::Vec3(0.25f, 0.22f, 0.22f), lit);
    grid_->reTranslateTiles(tiles, 0.04f);
    for (SceneNode* t : tiles) t->setLocal(t->local() * math::Mat4::scale(math::Vec3(0.6f, 1.0f, 0.6f)));

    char buf[128];
    snprintf(buf, sizeof(buf), "%s V%u %ux%u  UNITS %zu  PROPS %zu  WIN: %s", kNames[i], lvl.version,
             lvl.width, lvl.height, units_.size(), props_.size(), level::describe(lvl.win).c_str());
    info_ = buf;
    if (infoText_) infoText_->setText(info_);
    if (panel_) panel_->refresh();
}

void LevelLab::resetCamera(Renderer& r) {
    Camera& cam = r.camera();
    cam.setPerspective(kFovY, cam.aspect(), kNear, kFar);
    cam.setTerrainHeightFn({});
    cam.setForward(math::Vec3(0, 0, -1));
    cam.setPitchLimits(Camera::kMinPitchDeg, Camera::kMaxPitchDeg);
    cam.setYawLimits(Camera::kMinYawDeg, Camera::kMaxYawDeg);
    cam.setDistanceLimits(8.0f, 40.0f, 26.6f);
    cam.setViewLookAt(kEye, kLook, math::Vec3(0, 1, 0));
    r.setClearColor(math::Vec3(0.42f, 0.60f, 0.80f));
    r.setFrameLight(math::Vec3(0.3f, 0.85f, 0.45f).normalized(), 0.35f);
}

void LevelLab::restartAutoDrive() {
    step_ = 0;
    stepT_ = 0.0f;
    unitsOk_ = propsOk_ = terrainOk_ = true;
    if (checks_) checks_->reset();
    showLevel(0);
}

void LevelLab::buildUI(Renderer& r) {
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
    panel_->addAction("RESTART CHECKS", [this]() { autoOn_ = true; restartAutoDrive(); });
    panel_->addRow("LEVEL", [this]() { return std::string(kNames[current_]); }, [this]() {
        autoOn_ = false;
        showLevel((current_ + 1) % static_cast<int>(entries_.size()));
    });
    panel_->addAction("BACK", [rptr, gs]() {
        rptr->sceneManager().replace(std::make_unique<HubScene>(*gs));
    });
    root->addChild(panel_->build());
    root->addChild(checks_->build());
    auto info = std::make_unique<ui::UIText>(math::Vec2(16.0f, static_cast<float>(h) - 40.0f), info_,
                                             math::Vec4(1.0f, 1.0f, 1.0f, 1.0f), 2);
    infoText_ = info.get();
    root->addChild(std::move(info));
    ui_->setRoot(std::move(root));
}

void LevelLab::onResize(int w, int h, Renderer& r) {
    if (w < 1 || h < 1 || !ui_) return;
    r.camera().setPerspective(kFovY, static_cast<float>(w) / static_cast<float>(h), kNear, kFar);
    buildUI(r);
}

bool LevelLab::checkUnits(const Entry& e) const {
    const auto& recs = e.level.units;
    if (units_.size() != recs.size()) return false;
    for (size_t i = 0; i < recs.size(); ++i) {
        const game::Unit* u = units_[i];
        const auto& rec = recs[i];
        if (static_cast<int>(u->cell().x) != rec.x || static_cast<int>(u->cell().y) != rec.y) return false;
        if (grid_->at(rec.x, rec.y).blockingUnit != u) return false;
        const unit::Stats& s = u->stats();
        if (!rec.catalogId.empty()) {
            const unit::UnitDef* d = catalog_.find(rec.catalogId);
            if (!d || u->faction() != d->faction || s.maxHp != d->baseStats.maxHp ||
                s.atk != d->baseStats.atk || s.def != d->baseStats.def ||
                s.mov != d->baseStats.mov || s.range != d->baseStats.range) {
                aout << "[levellab] unit " << i << " (" << rec.catalogId << ") mismatch" << std::endl;
                return false;
            }
        } else if (s.maxHp != rec.maxHp || s.atk != rec.atk || s.range != rec.range ||
                   static_cast<int>(u->faction()) != rec.faction) {
            return false;
        }
        const math::Vec3 wp = grid_->cellToWorld(rec.x, rec.y);
        const math::Mat4 w = const_cast<game::Unit*>(u)->node()->world();
        if (std::abs(w.m[12] - wp.x) > 1e-3f || std::abs(w.m[13] - wp.y) > 1e-3f ||
            std::abs(w.m[14] - wp.z) > 1e-3f)
            return false;
    }
    return true;
}

int LevelLab::shadowDrawables() const {
    int n = 0;
    std::function<void(const SceneNode*)> walk = [&](const SceneNode* s) {
        for (const auto& d : s->drawables())
            if (d.model && d.material && d.material->shader) ++n;
        for (const auto& c : s->children()) walk(c.get());
    };
    walk(root_.get());
    return n;
}

bool LevelLab::checkProps(const Entry& e, Renderer& r) const {
    const auto& recs = e.level.props;
    if (props_.size() != recs.size()) return false;
    for (size_t i = 0; i < recs.size(); ++i) {
        SceneNode* n = props_[i]->node();
        const math::Vec3 wp = grid_->cellToWorld(recs[i].x, recs[i].y);
        const math::Mat4 w = n->world();
        if (std::abs(w.m[12] - wp.x) > 1e-3f || std::abs(w.m[14] - wp.z) > 1e-3f ||
            std::abs(w.m[13] - wp.y) > 1e-3f)
            return false;
        if (recs[i].blocks == grid_->at(recs[i].x, recs[i].y).passable) return false;
        if (n->children().empty() || n->children().front()->drawables().empty()) return false;
    }
    const int expected = shadowDrawables();
    if (r.shadowStats().draws != expected) {
        aout << "[levellab] shadow draws " << r.shadowStats().draws << " want " << expected << std::endl;
        return false;
    }
    return true;
}

bool LevelLab::checkTerrain() const {
    if (!terrainMesh_ || !grid_) return false;
    float worst = 0.0f;
    for (int y = 0; y < grid_->height(); ++y)
        for (int x = 0; x < grid_->width(); ++x) {
            const math::Vec3 c = grid_->cellToWorld(x, y);
            float h = 0.0f;
            if (!heightAt(*terrainMesh_, c.x, c.z, h)) return false;
            worst = std::max(worst, std::abs(h - c.y));
        }
    if (worst > 0.01f) aout << "[levellab] terrain off by " << worst << std::endl;
    return worst <= 0.01f;
}

void LevelLab::autoDrive(float dt, Renderer& r) {
    stepT_ += dt;
    auto next = [this]() { ++step_; stepT_ = 0.0f; };
    switch (step_) {
        case 0: {
            bool ok = true;
            for (int i = 0; i < 3; ++i) {
                const Entry& e = entries_[i];
                ok = ok && e.loaded && e.level.version == 3 && level::save(e.level) == e.bytes;
            }
            checks_->check(kRoundTrip, ok);
            next();
            break;
        }
        case 1:
        case 2:
        case 3: {
            if (stepT_ < 0.6f || framesShown_ < 3) break;
            const Entry& e = entries_[current_];
            unitsOk_ = unitsOk_ && checkUnits(e);
            propsOk_ = propsOk_ && checkProps(e, r);
            terrainOk_ = terrainOk_ && checkTerrain();
            if (step_ == 3) {
                checks_->check(kByCatalog, unitsOk_);
                checks_->check(kPropsRow, propsOk_);
                checks_->check(kTerrain, terrainOk_);
                const auto& w1 = entries_[0].level.win;
                const auto& w2 = entries_[1].level.win;
                const auto& w3 = entries_[2].level.win;
                checks_->check(kWinRow, w1.type == level::WinType::DefeatAll &&
                                        w2.type == level::WinType::SurviveRounds && w2.a == 8 &&
                                        w3.type == level::WinType::ReachCell && w3.a == 8 && w3.b == 8 &&
                                        info_.find(level::describe(w3)) != std::string::npos);
            }
            showLevel(current_ + 1);
            next();
            break;
        }
        case 4: {
            if (stepT_ < 0.6f || framesShown_ < 3) break;
            const Entry& e = entries_[3];
            bool idsEmpty = true;
            for (const auto& u : e.level.units) idsEmpty = idsEmpty && u.catalogId.empty();
            checks_->check(kV2Compat, e.loaded && e.level.version == 2 && e.level.units.size() == 3 &&
                                      idsEmpty && checkUnits(e) && checkTerrain());
            aout << "[levellab] auto-drive done: " << checks_->passedCount() << "/"
                 << checks_->size() << " checks green" << std::endl;
            next();
            break;
        }
        default:
            break;
    }
}

void LevelLab::update(float dt, Renderer& r) {
    if (root_) root_->update(dt);
    if (builder_) builder_->updateAll(dt);
    ++framesShown_;
    if (autoOn_) autoDrive(dt, r);
    if (panel_) panel_->refresh();
}

void LevelLab::render(Renderer& r) {
    r.renderSceneGraph(root_.get());
    r.renderUI(ui_.get());
}
