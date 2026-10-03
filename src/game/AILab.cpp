#include "AILab.h"

#include <algorithm>
#include <cstdio>

#include "Engine.h"
#include "GameState.h"
#include "HubScene.h"
#include "Unit.h"

namespace {

constexpr float kFovY = 30.0f * math::PI / 180.0f;
constexpr float kNear = 0.1f;
constexpr float kFar  = 100.0f;
const math::Vec3 kEye(0.0f, 11.0f, 12.5f);
const math::Vec3 kLook(0.0f, 0.0f, 0.8f);
constexpr int kBattles = 20;
constexpr uint32_t kLiveSeed = 1;

const char* kLegal     = "LEGAL MOVES";
const char* kApproach  = "APPROACH";
const char* kInRange   = "ATTACK IN RANGE";
const char* kTerminate = "TERMINATES";
const char* kDetermin  = "DETERMINISTIC";

math::Vec3 factionColor(unit::Faction f, int i) {
    if (f == unit::Faction::Enemy) return i % 2 ? math::Vec3(0.25f, 0.65f, 0.25f) : math::Vec3(0.45f, 0.75f, 0.20f);
    return i % 2 ? math::Vec3(0.30f, 0.50f, 0.95f) : math::Vec3(0.95f, 0.55f, 0.25f);
}

}

AILab::AILab(game::GameState& gs) : gs_(gs) {}
AILab::~AILab() = default;

void AILab::enter(Renderer& r) {
    r.setPickRoot(nullptr);
    assets_ = std::make_unique<game::LabAssets>(r.platform());
    assets_->upload();
    roster_ = ai::defaultRoster();
    root_ = std::make_unique<SceneNode>();
    ring_ = std::make_unique<SceneNode>();
    ring_->addDrawable({assets_->mesh("plane"), assets_->model("plane"),
                        math::Vec3(1.0f, 0.95f, 0.15f), assets_->material("unlit")});
    actions_ = std::make_unique<action::Manager>();
    info_.assign(3, "");

    ui_ = std::make_unique<ui::UIRoot>();
    ui_->init();
    checks_ = std::make_unique<game::LabChecklist>(math::Vec2(16, 16));
    for (const char* row : {kLegal, kApproach, kInRange, kTerminate, kDetermin}) checks_->add(row);
    resetCamera(r);
    buildUI(r);
    restart();

    Renderer* rptr = &r;
    auto alive = alive_;
    AILab* self = this;
    subs_.push_back(EventBus::on<PointerDownEvent>([alive, self, rptr](const PointerDownEvent& e) {
        if (!*alive || !self->ui_) return;
        if (ui::UIButton* btn = self->ui_->hitTestButton(math::Vec2(e.x, e.y))) {
            btn->handleClick();
            rptr->cancelPan();
        }
    }));
}

void AILab::exit(Renderer& r) {
    *alive_ = false;
    for (auto id : subs_) EventBus::unsubscribe(id);
    subs_.clear();
    if (actions_) actions_->clear();
    actions_.reset();
    builder_.reset();
    units_.clear();
    grid_.reset();
    ui_.reset();
    panel_.reset();
    checks_.reset();
    infoText_.clear();
    ring_.reset();
    root_.reset();
    assets_.reset();
}

void AILab::runBatch() {
    batch_.clear();
    int turns = 0, illegal = 0, approachCases = 0, approachMiss = 0, rangeCases = 0, rangeMiss = 0;
    int unfinished = 0, playerWins = 0, enemyWins = 0, roundSum = 0;
    bool repeatable = true;
    std::map<std::string, int> damage;
    for (int i = 0; i < kBattles; ++i) {
        ai::SimConfig cfg;
        cfg.seed = static_cast<uint32_t>(i + 1);
        batch_.push_back(ai::simulate(cfg, roster_));
        const ai::SimResult& res = batch_.back();
        repeatable = repeatable && ai::simulate(cfg, roster_).log == res.log;
        for (const auto& t : res.turns) {
            ++turns;
            if (!t.legalMove || !t.legalAttack) ++illegal;
            if (!t.couldAttack && t.couldApproach && !t.attacked) {
                ++approachCases;
                if (!t.approached) ++approachMiss;
            }
            if (t.couldAttack) {
                ++rangeCases;
                if (!t.attacked || t.moved) ++rangeMiss;
            }
        }
        if (res.winner < 0) ++unfinished;
        if (res.winner == 0) ++playerWins;
        if (res.winner == 1) ++enemyWins;
        roundSum += res.rounds;
        for (const auto& [type, d] : res.damageByType) damage[type] += d;
    }
    aout << "[ailab] batch: turns=" << turns << " illegal=" << illegal << " approach " << approachCases
         << "/" << approachMiss << " miss, in-range " << rangeCases << "/" << rangeMiss << " miss" << std::endl;
    checks_->check(kLegal, turns >= 200 && illegal == 0);
    checks_->check(kApproach, approachCases > 0 && approachMiss == 0);
    checks_->check(kInRange, rangeCases > 0 && rangeMiss == 0);
    checks_->check(kTerminate, unfinished == 0);
    checked_ = repeatable;

    char buf[128];
    snprintf(buf, sizeof(buf), "%d BATTLES 3V3 8X8: MEAN ROUNDS %.1f  PLAYER WINS %d  ENEMY WINS %d",
             kBattles, static_cast<float>(roundSum) / kBattles, playerWins, enemyWins);
    info_[0] = buf;
    std::string dmg = "DAMAGE:";
    for (const auto& [type, d] : damage) dmg += " " + type + " " + std::to_string(d);
    info_[1] = dmg;
    aout << "[ailab] " << info_[0] << std::endl << "[ailab] " << info_[1] << std::endl;
}

void AILab::startLive(uint32_t seed) {
    if (actions_) actions_->clear();
    builder_.reset();
    units_.clear();
    grid_.reset();
    root_->clearChildren();

    liveCfg_ = ai::SimConfig{};
    liveCfg_.seed = seed;
    expectedLog_ = ai::simulate(liveCfg_, roster_).log;
    ai::Battle b = ai::setupBattle(liveCfg_, roster_);
    std::vector<math::Vec2> cells;
    for (const auto& u : b.units) cells.push_back(u->cell());
    b.units.clear();
    grid_ = std::move(b.grid);

    const Mesh* cube = assets_->mesh("cube");
    const Model* cubeModel = assets_->model("cube");
    const Material* lit = assets_->material("lit");
    grid_->attachTo(root_.get(), assets_->mesh("plane"), assets_->model("plane"),
                    math::Vec3(0.62f, 0.66f, 0.52f), math::Vec3(0.22f, 0.22f, 0.25f), lit);
    for (int y = 0; y < grid_->height(); ++y)
        for (int x = 0; x < grid_->width(); ++x) {
            const math::Vec3 c = grid_->cellToWorld(x, y);
            if (c.y > 0.01f || !grid_->at(x, y).passable) {
                const float h = grid_->at(x, y).passable ? c.y : 0.7f;
                SceneNode* n = root_->addChild();
                n->setLocal(math::Mat4::translation(math::Vec3(c.x, h * 0.5f - 0.002f, c.z)) *
                            math::Mat4::scale(math::Vec3(0.94f, h, 0.94f)));
                n->addDrawable({cube, cubeModel, grid_->at(x, y).passable ? math::Vec3(0.55f, 0.50f, 0.42f)
                                                                          : math::Vec3(0.35f, 0.35f, 0.40f), lit});
            }
        }

    builder_ = std::make_unique<game::EntityBuilder>(root_.get(), grid_.get());
    for (size_t i = 0; i < roster_.size(); ++i) {
        game::SpawnDef s;
        s.faction = roster_[i].faction;
        s.baseStats = roster_[i].stats;
        s.baseColor = factionColor(roster_[i].faction, static_cast<int>(i));
        s.baseScale = 0.4f;
        s.cellX = static_cast<int>(cells[i].x);
        s.cellY = static_cast<int>(cells[i].y);
        s.mesh = cube;
        s.model = cubeModel;
        s.material = lit;
        units_.push_back(builder_->spawnUnit(s));
    }
    liveRng_ = std::make_unique<ai::Rng>(seed);
    liveLog_.clear();
    liveRound_ = 1;
    liveUnit_ = -1;
    livePhase_ = 0;
    liveTarget_ = nullptr;
    liveDone_ = false;
}

void AILab::stepLive() {
    if (liveDone_ || !actions_->isIdle()) return;
    if (livePhase_ == 0) {
        if (ai::winnerOf(units_) >= 0 || liveRound_ > liveCfg_.maxRounds) {
            liveDone_ = true;
            return;
        }
        do {
            if (++liveUnit_ >= static_cast<int>(units_.size())) {
                liveUnit_ = 0;
                if (++liveRound_ > liveCfg_.maxRounds) {
                    liveDone_ = true;
                    return;
                }
            }
        } while (units_[liveUnit_]->isDead());
        game::Unit* u = units_[liveUnit_];
        const ai::Plan plan = ai::planTurn(*u, *grid_, units_, liveCfg_.tun, *liveRng_);
        liveTarget_ = plan.target;
        liveDamage_ = 0;
        if (!plan.path.empty())
            actions_->enqueue(std::make_unique<action::MoveAction>(u, grid_.get(), plan.path, 0.12f));
        livePhase_ = 1;
        return;
    }
    game::Unit* u = units_[liveUnit_];
    if (livePhase_ == 1 && liveTarget_) {
        liveDamage_ = rules::damage(u->stats(), liveTarget_->stats(),
                                    rules::contextFor(*grid_, *u, *liveTarget_, liveRng_->next()), liveCfg_.tun);
        auto atk = std::make_unique<action::AttackAction>(u, liveTarget_, liveDamage_, nullptr, nullptr,
                                                          nullptr, nullptr, 0.25f);
        atk->setStatus(std::nullopt);
        actions_->enqueue(std::move(atk));
        livePhase_ = 2;
        return;
    }
    int targetIdx = -1;
    for (size_t k = 0; k < units_.size(); ++k) if (units_[k] == liveTarget_) targetIdx = static_cast<int>(k);
    liveLog_ += ai::turnLog(liveRound_, liveUnit_, *u, targetIdx, liveDamage_);
    for (game::Unit* d : units_)
        if (d->isDead() && d->node()) d->node()->setLocal(math::Mat4::scale(math::Vec3(0.0f)));
    livePhase_ = 0;
}

void AILab::finishChecks() {
    const bool same = liveLog_ == expectedLog_;
    if (!same) aout << "[ailab] live log differs:\n  live " << liveLog_ << "\n  sim  " << expectedLog_ << std::endl;
    checks_->check(kDetermin, checked_ && same);
    aout << "[ailab] auto-drive done: " << checks_->passedCount() << "/" << checks_->size()
         << " checks green" << std::endl;
}

void AILab::restart() {
    if (checks_) checks_->reset();
    runBatch();
    startLive(kLiveSeed);
}

void AILab::resetCamera(Renderer& r) {
    Camera& cam = r.camera();
    cam.setPerspective(kFovY, cam.aspect(), kNear, kFar);
    cam.setTerrainHeightFn({});
    cam.setForward(math::Vec3(0, 0, -1));
    cam.setPitchLimits(Camera::kMinPitchDeg, Camera::kMaxPitchDeg);
    cam.setYawLimits(Camera::kMinYawDeg, Camera::kMaxYawDeg);
    cam.setDistanceLimits(5.0f, 25.0f, 16.4f);
    cam.setViewLookAt(kEye, kLook, math::Vec3(0, 1, 0));
    r.setClearColor(math::Vec3(0.38f, 0.55f, 0.75f));
    r.setFrameLight(math::Vec3(0.3f, 0.85f, 0.45f).normalized(), 0.4f);
}

void AILab::buildUI(Renderer& r) {
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
    panel_->addAction("RESTART CHECKS", [this]() { restart(); });
    panel_->addRow("SPEED", [this]() { return std::to_string(static_cast<int>(speed_)) + "X"; },
                   [this]() { speed_ = speed_ >= 8.0f ? 1.0f : speed_ * 2.0f; });
    panel_->addRow("SEED", [this]() { return std::to_string(liveCfg_.seed); }, [this]() {
        startLive(liveCfg_.seed % kBattles + 1);
    });
    panel_->addAction("BACK", [rptr, gs]() {
        rptr->sceneManager().replace(std::make_unique<HubScene>(*gs));
    });
    root->addChild(panel_->build());
    root->addChild(checks_->build());
    infoText_.clear();
    for (int i = 0; i < 3; ++i) {
        auto t = std::make_unique<ui::UIText>(math::Vec2(16.0f, static_cast<float>(h) - 88.0f + 24.0f * i),
                                              info_[i], math::Vec4(1.0f, 1.0f, 1.0f, 1.0f), 2);
        infoText_.push_back(t.get());
        root->addChild(std::move(t));
    }
    ui_->setRoot(std::move(root));
}

void AILab::onResize(int w, int h, Renderer& r) {
    if (w < 1 || h < 1 || !ui_) return;
    r.camera().setPerspective(kFovY, static_cast<float>(w) / static_cast<float>(h), kNear, kFar);
    buildUI(r);
}

void AILab::updateRing() {
    if (liveDone_ || liveUnit_ < 0 || units_[liveUnit_]->isDead()) {
        ring_->setLocal(math::Mat4::scale(math::Vec3(0.0f, 1.0f, 0.0f)));
        return;
    }
    const game::Unit* u = units_[liveUnit_];
    const math::Vec3 p = grid_->cellToWorld(static_cast<int>(u->cell().x), static_cast<int>(u->cell().y));
    ring_->setLocal(math::Mat4::translation(math::Vec3(p.x, p.y + 0.004f, p.z)) *
                    math::Mat4::scale(math::Vec3(0.9f, 1.0f, 0.9f)));
}

void AILab::update(float dt, Renderer& r) {
    const float sdt = dt * speed_;
    if (root_) root_->update(sdt);
    if (actions_) actions_->update(sdt);
    const bool wasDone = liveDone_;
    stepLive();
    if (liveDone_ && !wasDone && !checks_->passed(kDetermin)) finishChecks();
    updateRing();
    char buf[96];
    const int winner = ai::winnerOf(units_);
    snprintf(buf, sizeof(buf), "LIVE SEED %u  ROUND %d%s", liveCfg_.seed, std::min(liveRound_, liveCfg_.maxRounds),
             liveDone_ ? (winner == 0 ? "  PLAYER WINS" : winner == 1 ? "  ENEMY WINS" : "  DRAW") : "");
    info_[2] = buf;
    for (size_t i = 0; i < infoText_.size(); ++i) infoText_[i]->setText(info_[i]);
    if (panel_) panel_->refresh();
}

void AILab::render(Renderer& r) {
    r.renderSceneGraph(root_.get());
    r.renderSceneGraph(ring_.get());
    r.renderUI(ui_.get());
}
