#include "GameUI.h"

#include <algorithm>
#include <cassert>

#include "AI.h"
#include "BattleGrid.h"
#include "Engine.h"
#include "Turn.h"
#include "Unit.h"

namespace gui {

namespace {

const math::Vec4 kCardColor(0.08f, 0.09f, 0.13f, 0.85f);
const math::Vec4 kSlotColor(0.15f, 0.16f, 0.22f, 0.85f);
const math::Vec4 kCurrentColor(0.55f, 0.45f, 0.12f, 0.95f);

}

bool Rect::overlaps(const Rect& o) const {
    return x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h;
}

bool Rect::inside(float sw, float sh) const {
    return x >= 0.0f && y >= 0.0f && x + w <= sw && y + h <= sh;
}

Layout Layout::compute(int w, int h, int orderSlots) {
    Layout l;
    const float fw = static_cast<float>(w), fh = static_cast<float>(h);
    const bool portrait = h > w;
    const float u = std::clamp(std::min(fw, fh) / 14.0f, 48.0f, 120.0f);
    l.unit = u;
    l.textScale = u >= 64.0f ? 3 : 2;
    const float m = 0.4f * u, gap = 0.2f * u;
    if (portrait) {
        const float bh = 1.2f * u;
        const float bw = (fw - 2.0f * m - 2.0f * gap) / 3.0f;
        for (int i = 0; i < 3; ++i)
            l.menu.push_back({m + static_cast<float>(i) * (bw + gap), fh - m - bh, bw, bh});
        l.card = {m, fh - m - bh - gap - 2.8f * u, 0.62f * fw, 2.8f * u};
        const float sw = (fw - 2.0f * m - static_cast<float>(orderSlots - 1) * gap) / static_cast<float>(orderSlots);
        for (int i = 0; i < orderSlots; ++i)
            l.order.push_back({m + static_cast<float>(i) * (sw + gap), m, sw, 1.2f * u});
    } else {
        const float bw = 4.5f * u, bh = u;
        for (int i = 0; i < 3; ++i)
            l.menu.push_back({fw - m - bw, fh - m - static_cast<float>(3 - i) * bh - static_cast<float>(2 - i) * gap, bw, bh});
        l.card = {m, fh - m - 2.8f * u, 6.5f * u, 2.8f * u};
        const float sw = 1.8f * u;
        const float total = static_cast<float>(orderSlots) * sw + static_cast<float>(orderSlots - 1) * gap;
        for (int i = 0; i < orderSlots; ++i)
            l.order.push_back({(fw - total) * 0.5f + static_cast<float>(i) * (sw + gap), 0.3f * u, sw, u});
    }
    return l;
}

std::vector<Rect> Layout::all() const {
    std::vector<Rect> r = menu;
    r.push_back(card);
    r.insert(r.end(), order.begin(), order.end());
    return r;
}

const char* label(Action a) {
    switch (a) {
        case Action::Move:   return "MOVE";
        case Action::Attack: return "ATTACK";
        case Action::Wait:   return "WAIT";
    }
    return "?";
}

std::vector<Action> legalActions(const grid::BattleGrid& g, const game::Unit& u,
                                 const std::vector<game::Unit*>& units, const TurnState& s,
                                 const rules::Tunables& t) {
    std::vector<Action> out;
    if (u.isDead() || u.isStunned()) return {Action::Wait};
    if (!s.moved && !s.acted && !rules::reachable(g, u, t).empty()) out.push_back(Action::Move);
    if (!s.acted) {
        for (const game::Unit* e : units)
            if (e && rules::canAttack(g, u, *e, t) == rules::AttackCheck::Ok) {
                out.push_back(Action::Attack);
                break;
            }
    }
    out.push_back(Action::Wait);
    return out;
}

std::vector<const game::Unit*> upcoming(const turn::Manager& tm, const std::vector<game::Unit*>& units, int n) {
    std::vector<const game::Unit*> out;
    if (units.empty()) return out;
    const size_t start = tm.turnIndex();
    for (size_t k = 0; out.size() < static_cast<size_t>(n) && k < units.size() * static_cast<size_t>(n + 1); ++k) {
        const game::Unit* u = units[(start + k) % units.size()];
        if (u && !u->isDead()) out.push_back(u);
    }
    return out;
}

void ActionMenu::build(ui::UIElement* layer, const Layout& l, Handler onPick) {
    buttons_.clear();
    slots_ = l.menu;
    for (int i = 0; i < 3; ++i) {
        const Action a = static_cast<Action>(i);
        auto b = std::make_unique<ui::UIButton>(math::Vec2(l.menu[i].x, l.menu[i].y),
                                                math::Vec2(l.menu[i].w, l.menu[i].h), label(a),
                                                [onPick, a]() { if (onPick) onPick(a); });
        b->setTextScale(l.textScale);
        b->setVisible(false);
        buttons_.push_back(b.get());
        layer->addChild(std::move(b));
    }
}

void ActionMenu::show(const std::vector<Action>& legal) {
    visible_ = legal;
    for (auto* b : buttons_) b->setVisible(false);
    const size_t first = slots_.size() - std::min(slots_.size(), legal.size());
    for (size_t i = 0; i < legal.size() && i < slots_.size(); ++i) {
        ui::UIButton* b = buttons_[static_cast<size_t>(legal[i])];
        const Rect& r = slots_[first + i];
        b->setPosition(math::Vec2(r.x, r.y));
        b->setVisible(true);
    }
}

std::vector<Rect> ActionMenu::rects() const {
    std::vector<Rect> out;
    for (auto* b : buttons_)
        if (b->visible()) out.push_back({b->position().x, b->position().y, b->size().x, b->size().y});
    return out;
}

void UnitCard::build(ui::UIElement* layer, const Layout& l) {
    auto panel = std::make_unique<ui::UIPanel>(math::Vec2(l.card.x, l.card.y), math::Vec2(l.card.w, l.card.h), kCardColor);
    const float lineH = l.card.h / 3.0f;
    for (int i = 0; i < 3; ++i) {
        auto t = std::make_unique<ui::UIText>(math::Vec2(l.card.x + 0.3f * l.unit, l.card.y + lineH * i + lineH * 0.3f),
                                              "", math::Vec4(1, 1, 1, 1), l.textScale);
        lines_[i] = t.get();
        panel->addChild(std::move(t));
    }
    panel_ = panel.get();
    panel_->setVisible(false);
    layer->addChild(std::move(panel));
}

void UnitCard::show(const game::Unit* u, std::string name) {
    unit_ = u;
    name_ = std::move(name);
    refresh();
}

void UnitCard::refresh() {
    if (!unit_) {
        hp_.clear();
        status_.clear();
        if (panel_) panel_->setVisible(false);
        return;
    }
    hp_ = "HP " + std::to_string(unit_->hp()) + "/" + std::to_string(unit_->maxHp());
    status_.clear();
    if (unit_->hasStatus(unit::StatusType::Stunned)) status_ += "STN";
    if (unit_->hasStatus(unit::StatusType::Poisoned)) status_ += status_.empty() ? "PSN" : " PSN";
    if (unit_->isDead()) status_ = "DOWN";
    if (panel_) {
        panel_->setVisible(true);
        lines_[0]->setText(name_);
        lines_[1]->setText(hp_);
        lines_[2]->setText(status_.empty() ? "OK" : status_);
    }
}

void TurnOrderBar::build(ui::UIElement* layer, const Layout& l) {
    slots_.clear();
    texts_.clear();
    for (const Rect& r : l.order) {
        auto p = std::make_unique<ui::UIPanel>(math::Vec2(r.x, r.y), math::Vec2(r.w, r.h), kSlotColor);
        auto t = std::make_unique<ui::UIText>(math::Vec2(r.x + 0.15f * l.unit, r.y + r.h * 0.3f), "",
                                              math::Vec4(1, 1, 1, 1), l.textScale);
        texts_.push_back(t.get());
        p->addChild(std::move(t));
        slots_.push_back(p.get());
        layer->addChild(std::move(p));
    }
}

void TurnOrderBar::update(const turn::Manager& tm, const std::vector<game::Unit*>& units,
                          const std::vector<std::string>& names) {
    order_ = upcoming(tm, units, static_cast<int>(slots_.empty() ? 6 : slots_.size()));
    labels_.clear();
    for (const game::Unit* u : order_) {
        std::string n = "?";
        for (size_t i = 0; i < units.size(); ++i) if (units[i] == u && i < names.size()) n = names[i];
        labels_.push_back(n);
    }
    for (size_t i = 0; i < slots_.size(); ++i) {
        texts_[i]->setText(i < labels_.size() ? labels_[i] : "");
        slots_[i]->setColor(i == 0 ? kCurrentColor : kSlotColor);
    }
}

void selfTest() {
    aout << "[gui] self-test starting" << std::endl;

    for (auto [w, h] : {std::pair{1280, 720}, std::pair{1080, 1920}, std::pair{720, 1280}, std::pair{1920, 1080}}) {
        const Layout l = Layout::compute(w, h);
        const auto rects = l.all();
        for (size_t i = 0; i < rects.size(); ++i) {
            assert(rects[i].w >= 48.0f && rects[i].h >= 48.0f);
            assert(rects[i].inside(static_cast<float>(w), static_cast<float>(h)));
            for (size_t j = i + 1; j < rects.size(); ++j) assert(!rects[i].overlaps(rects[j]));
        }
    }

    grid::BattleGrid g(6, 1, 1.0f);
    game::Unit k(nullptr, unit::Faction::Player, unit::Stats{10, 5, 2, 3, 1});
    game::Unit e(nullptr, unit::Faction::Enemy, unit::Stats{10, 5, 2, 3, 1});
    k.setCell(math::Vec2(0, 0));
    e.setCell(math::Vec2(5, 0));
    k.setGrid(&g);
    e.setGrid(&g);
    const std::vector<game::Unit*> all{&k, &e};
    auto has = [](const std::vector<Action>& v, Action a) { return std::find(v.begin(), v.end(), a) != v.end(); };
    auto acts = legalActions(g, k, all, {});
    assert(has(acts, Action::Move) && !has(acts, Action::Attack) && has(acts, Action::Wait));
    k.setCell(math::Vec2(4, 0));
    acts = legalActions(g, k, all, {true, false});
    assert(!has(acts, Action::Move) && has(acts, Action::Attack));
    acts = legalActions(g, k, all, {true, true});
    assert(acts.size() == 1 && acts[0] == Action::Wait);

    turn::Manager tm;
    game::Unit c(nullptr, unit::Faction::Ally, unit::Stats{});
    std::vector<game::Unit*> order{&k, &e, &c};
    tm.setUnits(order);
    auto up = upcoming(tm, order, 4);
    assert(up.size() == 4 && up[0] == &k && up[1] == &e && up[2] == &c && up[3] == &k);
    e.takeDamage(99);
    up = upcoming(tm, order, 3);
    assert(up[0] == &k && up[1] == &c && up[2] == &k);

    aout << "[gui] self-test PASS" << std::endl;
}

} // namespace gui
