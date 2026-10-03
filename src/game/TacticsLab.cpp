#include "TacticsLab.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>

#include "Engine.h"
#include "GameState.h"
#include "HubScene.h"
#include "Turn.h"
#include "Unit.h"
#include "UnitDef.h"

namespace {

constexpr float kFovY = 30.0f * math::PI / 180.0f;
constexpr float kNear = 0.1f;
constexpr float kFar  = 100.0f;
const math::Vec3 kEye(0.0f, 10.5f, 11.0f);
const math::Vec3 kLook(0.0f, 0.0f, 0.6f);
constexpr float kUnitScale = 0.4f;
constexpr float kHold = 0.8f;

const char* kMovReach   = "MOV REACH";
const char* kOccupied   = "OCCUPIED BLOCKS";
const char* kElevStep   = "ELEVATION STEP";
const char* kLosSym     = "LOS SYMMETRIC";
const char* kLosRidge   = "LOS RIDGE";
const char* kCoverRow   = "COVER";
const char* kDamageRow  = "DAMAGE FORMULA";
const char* kRangedRow  = "RANGED";
const char* kStunRow    = "STUN";

const char* kScenarioNames[] = {"REACH", "OCCUPANCY", "ELEVATION", "SIGHT", "COVER", "RANGED"};

const math::Vec2 kReachExpected[] = {
    {0, 3}, {1, 2}, {1, 3}, {2, 2}, {2, 3}, {3, 0}, {3, 1}, {3, 2}, {4, 1},
    {4, 2}, {4, 3}, {4, 4}, {4, 5}, {5, 2}, {5, 3}, {5, 4}, {6, 3}};

struct DamageCase {
    int atk, def;
    float attackerY, targetY;
    bool cover;
    uint32_t roll;
    float coverPct;
    bool elevation;
    bool variance;
    int expected;
};

const DamageCase kDamageCases[] = {
    {8,  4, 0.0f, 0.0f, false, 0, 0.25f, true,  false, 4},
    {5,  5, 0.0f, 0.0f, false, 0, 0.25f, true,  false, 1},
    {8,  4, 1.0f, 0.0f, false, 0, 0.25f, true,  false, 6},
    {8,  4, 0.4f, 0.0f, false, 0, 0.25f, true,  false, 4},
    {8,  4, 1.0f, 0.0f, false, 0, 0.25f, false, false, 4},
    {8,  4, 0.0f, 0.0f, true,  0, 0.25f, true,  false, 3},
    {12, 4, 0.0f, 0.0f, true,  0, 0.25f, true,  false, 6},
    {12, 4, 0.0f, 0.0f, true,  0, 0.50f, true,  false, 4},
    {8,  4, 0.0f, 0.0f, false, 2, 0.25f, true,  true,  5},
    {8,  4, 1.0f, 0.0f, true,  0, 0.25f, true,  false, 5},
};

bool sameCells(std::vector<math::Vec2> a, std::vector<math::Vec2> b) {
    auto less = [](const math::Vec2& p, const math::Vec2& q) { return p.y < q.y || (p.y == q.y && p.x < q.x); };
    std::sort(a.begin(), a.end(), less);
    std::sort(b.begin(), b.end(), less);
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].x != b[i].x || a[i].y != b[i].y) return false;
    return true;
}

bool contains(const std::vector<math::Vec2>& cells, int x, int y) {
    for (const auto& c : cells) if (static_cast<int>(c.x) == x && static_cast<int>(c.y) == y) return true;
    return false;
}

}

TacticsLab::TacticsLab(game::GameState& gs) : gs_(gs) {}
TacticsLab::~TacticsLab() = default;

void TacticsLab::buildCatalog() {
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
    def("soldier", unit::Faction::Player, {10, 6, 2, 3, 1}, math::Vec3(0.90f, 0.55f, 0.25f));
    def("scout",   unit::Faction::Player, {10, 6, 2, 5, 1}, math::Vec3(0.95f, 0.75f, 0.30f));
    def("ally",    unit::Faction::Ally,   {10, 4, 2, 3, 1}, math::Vec3(0.30f, 0.50f, 0.95f));
    def("enemy",   unit::Faction::Enemy,  {12, 5, 3, 3, 1}, math::Vec3(0.85f, 0.20f, 0.20f));
    def("brute",   unit::Faction::Player, {12, 12, 2, 3, 1}, math::Vec3(0.70f, 0.40f, 0.20f));
    def("target",  unit::Faction::Enemy,  {20, 5, 4, 3, 1}, math::Vec3(0.60f, 0.20f, 0.45f));
    def("archer",  unit::Faction::Player, {10, 9, 2, 3, 4}, math::Vec3(0.30f, 0.80f, 0.40f));
}

void TacticsLab::enter(Renderer& r) {
    r.setPickRoot(nullptr);
    assets_ = std::make_unique<game::LabAssets>(r.platform());
    assets_->upload();
    buildCatalog();
    root_ = std::make_unique<SceneNode>();
    overlay_ = std::make_unique<SceneNode>();
    actions_ = std::make_unique<action::Manager>();

    ui_ = std::make_unique<ui::UIRoot>();
    ui_->init();
    checks_ = std::make_unique<game::LabChecklist>(math::Vec2(16, 16));
    for (const char* row : {kMovReach, kOccupied, kElevStep, kLosSym, kLosRidge, kCoverRow,
                            kDamageRow, kRangedRow, kStunRow})
        checks_->add(row);
    resetCamera(r);
    buildUI(r);
    restartAutoDrive();

    Renderer* rptr = &r;
    auto alive = alive_;
    TacticsLab* self = this;
    subs_.push_back(EventBus::on<PointerDownEvent>([alive, self, rptr](const PointerDownEvent& e) {
        if (!*alive || !self->ui_) return;
        if (ui::UIButton* btn = self->ui_->hitTestButton(math::Vec2(e.x, e.y))) {
            btn->handleClick();
            rptr->cancelPan();
        }
    }));
}

void TacticsLab::exit(Renderer& r) {
    *alive_ = false;
    for (auto id : subs_) EventBus::unsubscribe(id);
    subs_.clear();
    clearScenario();
    actions_.reset();
    ui_.reset();
    panel_.reset();
    checks_.reset();
    overlay_.reset();
    root_.reset();
    catalog_.clear();
    assets_.reset();
}

void TacticsLab::clearScenario() {
    if (actions_) actions_->clear();
    builder_.reset();
    units_.clear();
    grid_.reset();
    if (root_) root_->clearChildren();
    if (overlay_) overlay_->clearChildren();
    worldRoot_ = unitsRoot_ = fxRoot_ = nullptr;
}

game::Unit* TacticsLab::spawn(const char* id, int x, int y) {
    game::SpawnDef s;
    s.cellX = x;
    s.cellY = y;
    s.mesh = assets_->mesh("cube");
    s.model = assets_->model("cube");
    s.material = assets_->material("lit");
    game::Unit* u = builder_->spawnFromCatalog(catalog_, id, s);
    units_.push_back(u);
    return u;
}

void TacticsLab::finishGrid() {
    const Mesh* cube = assets_->mesh("cube");
    const Model* cubeModel = assets_->model("cube");
    const Material* lit = assets_->material("lit");
    grid_->attachTo(worldRoot_, assets_->mesh("plane"), assets_->model("plane"),
                    math::Vec3(0.62f, 0.66f, 0.52f), math::Vec3(0.22f, 0.22f, 0.25f), lit);
    for (int y = 0; y < grid_->height(); ++y)
        for (int x = 0; x < grid_->width(); ++x) {
            const math::Vec3 c = grid_->cellToWorld(x, y);
            if (c.y > 0.01f) {
                SceneNode* n = worldRoot_->addChild();
                n->setLocal(math::Mat4::translation(math::Vec3(c.x, c.y * 0.5f - 0.002f, c.z)) *
                            math::Mat4::scale(math::Vec3(0.94f, c.y, 0.94f)));
                n->addDrawable({cube, cubeModel, math::Vec3(0.55f, 0.50f, 0.42f), lit});
            }
            if (!grid_->at(x, y).passable) {
                SceneNode* n = worldRoot_->addChild();
                n->setLocal(math::Mat4::translation(math::Vec3(c.x, c.y + 0.35f, c.z)) *
                            math::Mat4::scale(math::Vec3(0.9f, 0.7f, 0.9f)));
                n->addDrawable({cube, cubeModel, math::Vec3(0.35f, 0.35f, 0.40f), lit});
            }
            if (grid_->at(x, y).cover) {
                SceneNode* n = worldRoot_->addChild();
                n->setLocal(math::Mat4::translation(math::Vec3(c.x - 0.42f, c.y + 0.2f, c.z)) *
                            math::Mat4::scale(math::Vec3(0.12f, 0.4f, 0.9f)));
                n->addDrawable({cube, cubeModel, math::Vec3(0.45f, 0.32f, 0.20f), lit});
            }
        }
}

void TacticsLab::loadScenario(int s) {
    clearScenario();
    scenario_ = s;
    worldRoot_ = root_->addChild();
    unitsRoot_ = root_->addChild();
    fxRoot_ = root_->addChild();
    switch (s) {
        case kReach:
            grid_ = std::make_unique<grid::BattleGrid>(7, 7, 1.0f);
            for (int x = 0; x < 3; ++x) grid_->setPassable(x, 1, false);
            for (int x = 1; x < 4; ++x) grid_->setPassable(x, 4, false);
            break;
        case kOccupancy:
            grid_ = std::make_unique<grid::BattleGrid>(7, 3, 1.0f);
            for (int x = 0; x < 7; ++x) {
                grid_->setPassable(x, 0, false);
                grid_->setPassable(x, 2, false);
            }
            break;
        case kElevation: {
            grid_ = std::make_unique<grid::BattleGrid>(5, 3, 1.0f);
            const float h[5] = {0.0f, 0.5f, 1.5f, 1.5f, 1.5f};
            for (int x = 0; x < 5; ++x) {
                grid_->setCellY(x, 1, h[x]);
                grid_->setPassable(x, 0, false);
                grid_->setPassable(x, 2, false);
            }
            break;
        }
        case kSight:
            grid_ = std::make_unique<grid::BattleGrid>(7, 7, 1.0f);
            grid_->setCellY(3, 3, 1.0f);
            grid_->setCellY(0, 3, 2.0f);
            grid_->setPassable(2, 5, false);
            grid_->setPassable(4, 1, false);
            break;
        case kCover:
            grid_ = std::make_unique<grid::BattleGrid>(6, 4, 1.0f);
            grid_->setCover(3, 2, true);
            break;
        case kRanged:
            grid_ = std::make_unique<grid::BattleGrid>(8, 3, 1.0f);
            break;
        default:
            return;
    }
    finishGrid();
    builder_ = std::make_unique<game::EntityBuilder>(unitsRoot_, grid_.get());
    switch (s) {
        case kReach:     spawn("soldier", 3, 3); break;
        case kOccupancy: spawn("scout", 0, 1); spawn("ally", 1, 1); spawn("enemy", 4, 1); break;
        case kElevation: spawn("scout", 0, 1); break;
        case kSight:     spawn("archer", 1, 3); spawn("enemy", 5, 3); spawn("archer", 0, 3); break;
        case kCover:     spawn("brute", 2, 1); spawn("target", 3, 1);
                         spawn("brute", 2, 2); spawn("target", 3, 2); break;
        case kRanged:    spawn("archer", 0, 1); spawn("enemy", 4, 1); spawn("enemy", 7, 1); break;
    }
    showOverlay();
    if (panel_) panel_->refresh();
}

void TacticsLab::showCells(const std::vector<math::Vec2>& cells, const math::Vec3& color) {
    for (const auto& c : cells) {
        const math::Vec3 p = grid_->cellToWorld(static_cast<int>(c.x), static_cast<int>(c.y));
        SceneNode* n = overlay_->addChild();
        n->setLocal(math::Mat4::translation(math::Vec3(p.x, p.y + 0.004f, p.z)) *
                    math::Mat4::scale(math::Vec3(0.6f, 1.0f, 0.6f)));
        n->addDrawable({assets_->mesh("plane"), assets_->model("plane"), color, assets_->material("unlit")});
    }
}

void TacticsLab::showOverlay() {
    overlay_->clearChildren();
    char buf[96];
    switch (scenario_) {
        case kReach:
        case kOccupancy:
        case kElevation: {
            const auto cells = rules::reachable(*grid_, *units_[0], tun_);
            showCells(cells, math::Vec3(0.25f, 0.55f, 1.0f));
            snprintf(buf, sizeof(buf), "%s: MOV %d REACHES %zu CELLS", kScenarioNames[scenario_],
                     units_[0]->stats().mov, cells.size());
            break;
        }
        case kSight: {
            std::vector<math::Vec2> seen, hidden;
            for (int y = 0; y < grid_->height(); ++y)
                for (int x = 0; x < grid_->width(); ++x) {
                    if (!grid_->at(x, y).passable) continue;
                    const math::Vec2 c(static_cast<float>(x), static_cast<float>(y));
                    (rules::lineOfSight(*grid_, units_[0]->cell(), c, tun_) ? seen : hidden).push_back(c);
                }
            showCells(seen, math::Vec3(0.3f, 0.9f, 0.3f));
            showCells(hidden, math::Vec3(0.9f, 0.25f, 0.2f));
            snprintf(buf, sizeof(buf), "SIGHT: LOW ARCHER SEES %zu, HIDDEN %zu", seen.size(), hidden.size());
            break;
        }
        case kCover: {
            const int open = rules::damage(units_[0]->stats(), units_[1]->stats(),
                                           rules::contextFor(*grid_, *units_[0], *units_[1]), tun_);
            const int covered = rules::damage(units_[2]->stats(), units_[3]->stats(),
                                              rules::contextFor(*grid_, *units_[2], *units_[3]), tun_);
            snprintf(buf, sizeof(buf), "COVER %d%%: OPEN %d, IN COVER %d",
                     static_cast<int>(tun_.coverPct * 100.0f + 0.5f), open, covered);
            break;
        }
        case kRanged:
            snprintf(buf, sizeof(buf), "RANGED: NEAR %s, FAR %s",
                     rules::toString(rules::canAttack(*grid_, *units_[0], *units_[1], tun_)),
                     rules::toString(rules::canAttack(*grid_, *units_[0], *units_[2], tun_)));
            break;
        default:
            buf[0] = 0;
    }
    info_ = buf;
    if (infoText_) infoText_->setText(info_);
}

void TacticsLab::resetCamera(Renderer& r) {
    Camera& cam = r.camera();
    cam.setPerspective(kFovY, cam.aspect(), kNear, kFar);
    cam.setTerrainHeightFn({});
    cam.setForward(math::Vec3(0, 0, -1));
    cam.setPitchLimits(Camera::kMinPitchDeg, Camera::kMaxPitchDeg);
    cam.setYawLimits(Camera::kMinYawDeg, Camera::kMaxYawDeg);
    cam.setDistanceLimits(5.0f, 25.0f, 15.2f);
    cam.setViewLookAt(kEye, kLook, math::Vec3(0, 1, 0));
    r.setClearColor(math::Vec3(0.36f, 0.52f, 0.70f));
    r.setFrameLight(math::Vec3(0.3f, 0.85f, 0.45f).normalized(), 0.4f);
}

void TacticsLab::restartAutoDrive() {
    step_ = 0;
    stepT_ = 0.0f;
    waiting_ = false;
    hpChanges_ = 0;
    earlyHit_ = false;
    sawProjectile_ = false;
    if (checks_) checks_->reset();
    loadScenario(kReach);
}

void TacticsLab::buildUI(Renderer& r) {
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
    auto onOff = [](bool b) { return std::string(b ? "ON" : "OFF"); };
    panel_ = std::make_unique<game::DebugPanel>(
        math::Vec2(static_cast<float>(w) - 24.0f - rowW, 24.0f),
        math::Vec2(rowW, rowH), rowH * 0.15f, scale);
    panel_->addRow("AUTO", [this, onOff]() { return onOff(autoOn_); }, [this]() { autoOn_ = !autoOn_; });
    panel_->addAction("RESTART CHECKS", [this]() { autoOn_ = true; restartAutoDrive(); });
    panel_->addRow("SCENE", [this]() { return std::string(kScenarioNames[scenario_]); }, [this]() {
        autoOn_ = false;
        loadScenario((scenario_ + 1) % kScenarioCount);
    });
    panel_->addRow("VARIANCE", [this, onOff]() { return onOff(tun_.variance); },
                   [this]() { tun_.variance = !tun_.variance; showOverlay(); });
    panel_->addRow("COVER", [this]() { return std::string(tun_.coverPct > 0.3f ? "50%" : "25%"); },
                   [this]() { tun_.coverPct = tun_.coverPct > 0.3f ? 0.25f : 0.5f; showOverlay(); });
    panel_->addRow("ELEV BONUS", [this, onOff]() { return onOff(tun_.elevationBonus); },
                   [this]() { tun_.elevationBonus = !tun_.elevationBonus; showOverlay(); });
    panel_->addRow("DIAGONAL", [this, onOff]() { return onOff(tun_.diagonal); },
                   [this]() { tun_.diagonal = !tun_.diagonal; showOverlay(); });
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

void TacticsLab::onResize(int w, int h, Renderer& r) {
    if (w < 1 || h < 1 || !ui_) return;
    r.camera().setPerspective(kFovY, static_cast<float>(w) / static_cast<float>(h), kNear, kFar);
    buildUI(r);
}

void TacticsLab::checkDamageTable() {
    int ok = 0;
    for (const auto& c : kDamageCases) {
        rules::Tunables t;
        t.coverPct = c.coverPct;
        t.elevationBonus = c.elevation;
        t.variance = c.variance;
        const int got = rules::damage(unit::Stats{10, c.atk, 0, 3, 1}, unit::Stats{10, 5, c.def, 3, 1},
                                      {c.attackerY, c.targetY, c.cover, c.roll}, t);
        if (got == c.expected) ++ok;
        else aout << "[tacticslab] damage case atk=" << c.atk << " def=" << c.def << " got " << got
                  << " want " << c.expected << std::endl;
    }
    checks_->check(kDamageRow, ok == static_cast<int>(std::size(kDamageCases)));
}

void TacticsLab::checkStun() {
    game::Unit* archer = units_[0];
    game::Unit* nearU = units_[1];
    game::Unit* farU = units_[2];
    turn::Manager tm;
    tm.setUnits({archer, nearU, farU});
    const bool stunned = nearU->isStunned();
    tm.endTurn();
    const bool skippedOnce = tm.currentUnit() == farU && tm.skippedTurns() == 1;
    tm.endTurn();
    tm.endTurn();
    const bool backInTurn = tm.currentUnit() == nearU && tm.skippedTurns() == 1 && !nearU->isStunned();
    checks_->check(kStunRow, stunned && skippedOnce && backInTurn);
}

void TacticsLab::observe() {
    if (scenario_ != kRanged || units_.size() < 2 || !fxRoot_) return;
    const bool flying = !fxRoot_->children().empty();
    if (flying) {
        sawProjectile_ = true;
        const math::Mat4 m = fxRoot_->children().front()->world();
        lastProjectile_ = math::Vec3(m.m[12], m.m[13], m.m[14]);
    }
    if (units_[1]->hp() != lastHp_) {
        ++hpChanges_;
        if (flying) earlyHit_ = true;
        lastHp_ = units_[1]->hp();
    }
}

void TacticsLab::autoDrive(float dt) {
    stepT_ += dt;
    auto next = [this]() { ++step_; stepT_ = 0.0f; };
    rules::Tunables base;
    switch (step_) {
        case 0: {
            const auto cells = rules::reachable(*grid_, *units_[0], base);
            checks_->check(kMovReach, cells.size() == 17 &&
                           sameCells(cells, {std::begin(kReachExpected), std::end(kReachExpected)}));
            next();
            break;
        }
        case 1:
            if (stepT_ < kHold) break;
            loadScenario(kOccupancy);
            {
                const auto cells = rules::reachable(*grid_, *units_[0], base);
                const auto path = rules::pathTo(*grid_, *units_[0], math::Vec2(3, 1), base);
                checks_->check(kOccupied, sameCells(cells, {{2, 1}, {3, 1}}) && path.size() == 4 &&
                                          path[1].x == 1.0f && rules::pathTo(*grid_, *units_[0],
                                          math::Vec2(1, 1), base).empty());
            }
            next();
            break;
        case 2:
            if (stepT_ < kHold) break;
            loadScenario(kElevation);
            {
                const auto cells = rules::reachable(*grid_, *units_[0], base);
                rules::Tunables strict;
                strict.stepLimit = 0.4f;
                checks_->check(kElevStep, contains(cells, 1, 1) && !contains(cells, 2, 1) &&
                                          rules::reachable(*grid_, *units_[0], strict).empty());
            }
            next();
            break;
        case 3:
            if (stepT_ < kHold) break;
            loadScenario(kSight);
            {
                int pairs = 0, asym = 0;
                for (int i = 0; i < 49; ++i)
                    for (int j = i + 1; j < 49; ++j) {
                        const math::Vec2 a(static_cast<float>(i % 7), static_cast<float>(i / 7));
                        const math::Vec2 b(static_cast<float>(j % 7), static_cast<float>(j / 7));
                        ++pairs;
                        if (rules::lineOfSight(*grid_, a, b, base) != rules::lineOfSight(*grid_, b, a, base))
                            ++asym;
                    }
                checks_->check(kLosSym, pairs == 49 * 48 / 2 && asym == 0);
                const bool lowBlocked = !rules::lineOfSight(*grid_, units_[0]->cell(), units_[1]->cell(), base);
                const bool highSees = rules::lineOfSight(*grid_, units_[2]->cell(), units_[1]->cell(), base);
                const bool lowOpen = rules::lineOfSight(*grid_, units_[0]->cell(), math::Vec2(5, 5), base);
                checks_->check(kLosRidge, lowBlocked && highSees && lowOpen);
            }
            next();
            break;
        case 4:
            if (stepT_ < kHold) break;
            loadScenario(kCover);
            checkDamageTable();
            openHp0_ = units_[1]->hp();
            coverHp0_ = units_[3]->hp();
            for (int k = 0; k < 2; ++k) {
                game::Unit* a = units_[k * 2];
                game::Unit* t = units_[k * 2 + 1];
                actions_->enqueue(std::make_unique<action::AttackAction>(
                    a, t, rules::damage(a->stats(), t->stats(), rules::contextFor(*grid_, *a, *t), base)));
            }
            next();
            break;
        case 5: {
            if (stepT_ < 0.2f || !actions_->isIdle()) break;
            const int dropOpen = openHp0_ - units_[1]->hp();
            const int dropCover = coverHp0_ - units_[3]->hp();
            rules::Tunables half;
            half.coverPct = 0.5f;
            const int half50 = rules::damage(units_[2]->stats(), units_[3]->stats(),
                                             rules::contextFor(*grid_, *units_[2], *units_[3]), half);
            if (dropOpen != 8 || dropCover != 6 || half50 != 4)
                aout << "[tacticslab] cover open=" << dropOpen << " cover=" << dropCover
                     << " half=" << half50 << std::endl;
            checks_->check(kCoverRow, dropOpen == 8 && dropCover == 6 && half50 == 4);
            next();
            break;
        }
        case 6:
            if (stepT_ < kHold) break;
            loadScenario(kRanged);
            {
                game::Unit* archer = units_[0];
                game::Unit* nearU = units_[1];
                nearHp0_ = nearU->hp();
                lastHp_ = nearHp0_;
                hpChanges_ = 0;
                earlyHit_ = false;
                sawProjectile_ = false;
                auto ranged = std::make_unique<action::RangedAttackAction>(
                    archer, nearU, rules::damage(archer->stats(), nearU->stats(),
                                                 rules::contextFor(*grid_, *archer, *nearU), base),
                    grid_.get(), fxRoot_,
                    SceneNode::Drawable{assets_->mesh("cube"), assets_->model("cube"),
                                        math::Vec3(1.0f, 0.85f, 0.2f), assets_->material("lit")},
                    true, 5.0f);
                ranged->setStatus(unit::StatusEffect{unit::StatusType::Stunned, 1, 0});
                actions_->enqueue(std::move(ranged));
            }
            next();
            break;
        case 7: {
            if (stepT_ < 0.2f || !actions_->isIdle()) break;
            game::Unit* archer = units_[0];
            game::Unit* nearU = units_[1];
            const math::Vec3 tgt = grid_->cellToWorld(4, 1) + math::Vec3(0, 0.9f, 0);
            const float miss = (lastProjectile_ - tgt).length();
            const bool refused = rules::canAttack(*grid_, *archer, *units_[2], base) ==
                                 rules::AttackCheck::OutOfRange;
            const int dmg = 9 - 3;
            if (!(sawProjectile_ && miss < 0.6f && hpChanges_ == 1 && !earlyHit_))
                aout << "[tacticslab] ranged seen=" << sawProjectile_ << " miss=" << miss
                     << " changes=" << hpChanges_ << " early=" << earlyHit_ << std::endl;
            checks_->check(kRangedRow, sawProjectile_ && miss < 0.6f && hpChanges_ == 1 && !earlyHit_ &&
                                       nearU->hp() == nearHp0_ - dmg && fxRoot_->children().empty() &&
                                       refused);
            checkStun();
            aout << "[tacticslab] auto-drive done: " << checks_->passedCount() << "/"
                 << checks_->size() << " checks green" << std::endl;
            next();
            break;
        }
        default:
            break;
    }
}

void TacticsLab::update(float dt, Renderer& r) {
    if (root_) root_->update(dt);
    if (actions_) actions_->update(dt);
    if (builder_) builder_->updateAll(dt);
    observe();
    if (autoOn_) autoDrive(dt);
    if (panel_) panel_->refresh();
}

void TacticsLab::render(Renderer& r) {
    r.renderSceneGraph(root_.get());
    r.renderSceneGraph(overlay_.get());
    r.renderUI(ui_.get());
}
