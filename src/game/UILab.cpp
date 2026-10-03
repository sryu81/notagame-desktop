#include "UILab.h"

#include <algorithm>

#include "Engine.h"
#include "GameState.h"
#include "HubScene.h"
#include "Rules.h"
#include "Unit.h"

namespace {

constexpr float kFovY = 30.0f * math::PI / 180.0f;
constexpr float kNear = 0.1f;
constexpr float kFar  = 100.0f;
const math::Vec3 kEye(0.0f, 9.0f, 10.5f);
const math::Vec3 kLook(0.0f, 0.0f, 0.3f);

const char* kLegalRow    = "LEGAL ACTIONS ONLY";
const char* kCardRow     = "UNIT CARD";
const char* kOrderRow    = "TURN ORDER";
const char* kPortraitRow = "PORTRAIT";

bool same(const std::vector<gui::Action>& a, std::initializer_list<gui::Action> b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end());
}

}

UILab::UILab(game::GameState& gs) : gs_(gs) {}
UILab::~UILab() = default;

void UILab::enter(Renderer& r) {
    r.setPickRoot(nullptr);
    assets_ = std::make_unique<game::LabAssets>(r.platform());
    assets_->upload();
    root_ = std::make_unique<SceneNode>();
    actions_ = std::make_unique<action::Manager>();

    ui_ = std::make_unique<ui::UIRoot>();
    ui_->init();
    checks_ = std::make_unique<game::LabChecklist>(math::Vec2(16, 16));
    for (const char* row : {kLegalRow, kCardRow, kOrderRow, kPortraitRow}) checks_->add(row);
    resetCamera(r);
    restart();
    buildUI(r);

    Renderer* rptr = &r;
    auto alive = alive_;
    UILab* self = this;
    subs_.push_back(EventBus::on<PointerDownEvent>([alive, self, rptr](const PointerDownEvent& e) {
        if (!*alive || !self->ui_) return;
        if (ui::UIButton* btn = self->ui_->hitTestButton(math::Vec2(e.x, e.y))) {
            btn->handleClick();
            rptr->cancelPan();
        }
    }));
}

void UILab::exit(Renderer& r) {
    *alive_ = false;
    for (auto id : subs_) EventBus::unsubscribe(id);
    subs_.clear();
    if (actions_) actions_->clear();
    actions_.reset();
    turns_.reset();
    builder_.reset();
    units_.clear();
    grid_.reset();
    ui_.reset();
    panel_.reset();
    checks_.reset();
    root_.reset();
    assets_.reset();
}

void UILab::spawn() {
    if (actions_) actions_->clear();
    turns_.reset();
    builder_.reset();
    units_.clear();
    grid_.reset();
    root_->clearChildren();
    grid_ = std::make_unique<grid::BattleGrid>(7, 5, 1.0f);
    const Material* lit = assets_->material("lit");
    grid_->attachTo(root_.get(), assets_->mesh("plane"), assets_->model("plane"),
                    math::Vec3(0.62f, 0.66f, 0.52f), math::Vec3(0.25f, 0.25f, 0.28f), lit);
    unitsRoot_ = root_->addChild();
    builder_ = std::make_unique<game::EntityBuilder>(unitsRoot_, grid_.get());
    struct Def { const char* name; unit::Faction f; unit::Stats st; math::Vec3 color; int x, y; };
    const Def defs[] = {
        {"KNIGHT", unit::Faction::Player, {14, 6, 3, 3, 1}, {0.90f, 0.55f, 0.25f}, 0, 2},
        {"ARCHER", unit::Faction::Player, {9, 5, 2, 3, 3}, {0.30f, 0.75f, 0.45f}, 0, 4},
        {"GOBLIN", unit::Faction::Enemy, {10, 5, 2, 3, 1}, {0.85f, 0.25f, 0.20f}, 4, 2},
    };
    names_.clear();
    for (const Def& d : defs) {
        game::SpawnDef s;
        s.faction = d.f;
        s.baseStats = d.st;
        s.baseColor = d.color;
        s.baseScale = 0.4f;
        s.cellX = d.x;
        s.cellY = d.y;
        s.mesh = assets_->mesh("cube");
        s.model = assets_->model("cube");
        s.material = lit;
        units_.push_back(builder_->spawnUnit(s));
        names_.push_back(d.name);
    }
    turns_ = std::make_unique<turn::Manager>();
    turns_->setUnits(units_);
    turns_->setTimed(false);
    state_ = {};
    selected_ = units_[0];
}

void UILab::restart() {
    step_ = 0;
    stepT_ = 0.0f;
    orderSteps_ = 0;
    orderOk_ = true;
    if (checks_) checks_->reset();
    spawn();
    refreshWidgets();
}

void UILab::refreshWidgets() {
    if (!turns_ || !grid_) return;
    const game::Unit* cur = turns_->currentUnit();
    if (cur) menu_.show(gui::legalActions(*grid_, *cur, units_, state_));
    std::string name;
    for (size_t i = 0; i < units_.size(); ++i) if (units_[i] == selected_) name = names_[i];
    card_.show(selected_, name);
    bar_.update(*turns_, units_, names_);
}

void UILab::onAction(gui::Action a) {
    game::Unit* cur = turns_ ? turns_->currentUnit() : nullptr;
    if (!cur || !actions_->isIdle()) return;
    if (a == gui::Action::Wait) {
        turns_->endTurn();
        state_ = {};
        selected_ = turns_->currentUnit();
    } else if (a == gui::Action::Move) {
        const auto cells = rules::reachable(*grid_, *cur);
        if (cells.empty()) return;
        const math::Vec2 goal = *std::max_element(cells.begin(), cells.end(),
            [](const math::Vec2& p, const math::Vec2& q) { return p.x < q.x; });
        actions_->enqueue(std::make_unique<action::MoveAction>(cur, grid_.get(), rules::pathTo(*grid_, *cur, goal), 0.15f));
        state_.moved = true;
    } else {
        for (game::Unit* e : units_)
            if (rules::canAttack(*grid_, *cur, *e) == rules::AttackCheck::Ok) {
                actions_->enqueue(std::make_unique<action::AttackAction>(
                    cur, e, rules::damage(cur->stats(), e->stats(), rules::contextFor(*grid_, *cur, *e)),
                    nullptr, nullptr, nullptr, nullptr, 0.25f));
                selected_ = e;
                state_.acted = true;
                break;
            }
    }
}

void UILab::resetCamera(Renderer& r) {
    Camera& cam = r.camera();
    cam.setPerspective(kFovY, cam.aspect(), kNear, kFar);
    cam.setTerrainHeightFn({});
    cam.setForward(math::Vec3(0, 0, -1));
    cam.setPitchLimits(Camera::kMinPitchDeg, Camera::kMaxPitchDeg);
    cam.setYawLimits(Camera::kMinYawDeg, Camera::kMaxYawDeg);
    cam.setDistanceLimits(5.0f, 25.0f, 13.8f);
    cam.setViewLookAt(kEye, kLook, math::Vec3(0, 1, 0));
    r.setClearColor(math::Vec3(0.36f, 0.52f, 0.70f));
    r.setFrameLight(math::Vec3(0.3f, 0.85f, 0.45f).normalized(), 0.4f);
}

void UILab::buildUI(Renderer& r) {
    const int w = r.width() > 0 ? r.width() : 1280;
    const int h = r.height() > 0 ? r.height() : 720;
    ui_->setScreenSize(w, h);
    const float rowH = std::clamp(static_cast<float>(h) / 18.0f, 36.0f, 72.0f);
    const float rowW = rowH * 5.5f;
    const int scale = rowH >= 56.0f ? 3 : 2;

    auto root = std::make_unique<ui::UIPanel>(math::Vec2(0, 0),
        math::Vec2(static_cast<float>(w), static_cast<float>(h)), math::Vec4(0, 0, 0, 0));
    const gui::Layout layout = gui::Layout::compute(w, h);
    menu_.build(root.get(), layout, [this](gui::Action a) { onAction(a); });
    card_.build(root.get(), layout);
    bar_.build(root.get(), layout);

    Renderer* rptr = &r;
    game::GameState* gs = &gs_;
    panel_ = std::make_unique<game::DebugPanel>(
        math::Vec2(static_cast<float>(w) - 24.0f - rowW, 24.0f + layout.unit * 1.5f),
        math::Vec2(rowW, rowH), rowH * 0.15f, scale);
    panel_->addRow("AUTO", [this]() { return std::string(autoOn_ ? "ON" : "OFF"); },
                   [this]() { autoOn_ = !autoOn_; });
    panel_->addAction("RESTART CHECKS", [this]() { autoOn_ = true; restart(); });
    panel_->addAction("BACK", [rptr, gs]() {
        rptr->sceneManager().replace(std::make_unique<HubScene>(*gs));
    });
    root->addChild(panel_->build());
    root->addChild(checks_->build());
    ui_->setRoot(std::move(root));
    refreshWidgets();
}

void UILab::onResize(int w, int h, Renderer& r) {
    if (w < 1 || h < 1 || !ui_) return;
    r.camera().setPerspective(kFovY, static_cast<float>(w) / static_cast<float>(h), kNear, kFar);
    buildUI(r);
}

void UILab::autoDrive(float dt) {
    stepT_ += dt;
    auto next = [this]() { ++step_; stepT_ = 0.0f; };
    using A = gui::Action;
    switch (step_) {
        case 0:
            if (stepT_ < 0.5f) break;
            legalBefore_ = same(menu_.visible(), {A::Move, A::Wait});
            onAction(A::Move);
            next();
            break;
        case 1: {
            if (stepT_ < 0.3f || !actions_->isIdle()) break;
            const bool afterMove = same(menu_.visible(), {A::Attack, A::Wait});
            if (!legalBefore_ || !afterMove) aout << "[uilab] menu before/after move wrong" << std::endl;
            checks_->check(kLegalRow, legalBefore_ && afterMove && units_[0]->cell().x == 3.0f);
            selected_ = units_[2];
            refreshWidgets();
            hp0_ = units_[2]->hp();
            const bool before = card_.nameText() == "GOBLIN" &&
                                card_.hpText() == "HP " + std::to_string(hp0_) + "/10" && card_.statusText().empty();
            onAction(A::Attack);
            legalBefore_ = before;
            next();
            break;
        }
        case 2: {
            if (stepT_ < 0.3f || !actions_->isIdle()) break;
            const int dmg = rules::damage(units_[0]->stats(), units_[2]->stats());
            checks_->check(kCardRow, legalBefore_ && card_.nameText() == "GOBLIN" &&
                                     card_.hpText() == "HP " + std::to_string(hp0_ - dmg) + "/10" &&
                                     card_.statusText() == "PSN");
            next();
            break;
        }
        case 3: {
            if (stepT_ < 0.4f) break;
            const auto& order = bar_.order();
            const game::Unit* cur = turns_->currentUnit();
            size_t idx = 0;
            while (idx < units_.size() && units_[idx] != cur) ++idx;
            bool ok = !order.empty() && order[0] == cur && bar_.labels()[0] == names_[idx];
            for (size_t k = 0; k < order.size(); ++k) ok = ok && order[k] == units_[(idx + k) % units_.size()];
            orderOk_ = orderOk_ && ok;
            if (orderSteps_ == 2 * static_cast<int>(units_.size())) {
                checks_->check(kOrderRow, orderOk_);
                next();
                break;
            }
            onAction(A::Wait);
            ++orderSteps_;
            stepT_ = 0.0f;
            break;
        }
        case 4: {
            bool ok = true;
            for (auto [w, h] : {std::pair{1080, 1920}, std::pair{720, 1280}}) {
                const gui::Layout l = gui::Layout::compute(w, h);
                ui::UIPanel layer(math::Vec2(0, 0), math::Vec2(static_cast<float>(w), static_cast<float>(h)),
                                  math::Vec4(0, 0, 0, 0));
                gui::ActionMenu m;
                m.build(&layer, l, nullptr);
                m.show({A::Move, A::Attack, A::Wait});
                std::vector<gui::Rect> rects = m.rects();
                rects.push_back(l.card);
                rects.insert(rects.end(), l.order.begin(), l.order.end());
                for (size_t i = 0; i < rects.size(); ++i) {
                    ok = ok && rects[i].w >= 48.0f && rects[i].h >= 48.0f &&
                         rects[i].inside(static_cast<float>(w), static_cast<float>(h));
                    for (size_t j = i + 1; j < rects.size(); ++j) ok = ok && !rects[i].overlaps(rects[j]);
                }
                ok = ok && m.rects().size() == 3;
            }
            checks_->check(kPortraitRow, ok);
            aout << "[uilab] auto-drive done: " << checks_->passedCount() << "/" << checks_->size()
                 << " checks green" << std::endl;
            next();
            break;
        }
        default:
            break;
    }
}

void UILab::update(float dt, Renderer& r) {
    if (root_) root_->update(dt);
    if (actions_) actions_->update(dt);
    if (builder_) builder_->updateAll(dt);
    refreshWidgets();
    if (autoOn_) autoDrive(dt);
    if (panel_) panel_->refresh();
}

void UILab::render(Renderer& r) {
    r.renderSceneGraph(root_.get());
    r.renderUI(ui_.get());
}
