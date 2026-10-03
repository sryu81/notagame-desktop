#ifndef NOT_A_GAME_GAME_UI_H
#define NOT_A_GAME_GAME_UI_H

#include <functional>
#include <string>
#include <vector>

#include "Math.h"
#include "Rules.h"

namespace grid { class BattleGrid; }
namespace game { class Unit; }
namespace turn { class Manager; }
namespace ui { class UIElement; class UIButton; class UIPanel; class UIText; }

namespace gui {

struct Rect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    bool overlaps(const Rect& o) const;
    bool inside(float sw, float sh) const;
};

struct Layout {
    std::vector<Rect> menu;
    Rect card;
    std::vector<Rect> order;
    int textScale = 2;
    float unit = 48.0f;

    static Layout compute(int w, int h, int orderSlots = 6);
    std::vector<Rect> all() const;
};

enum class Action { Move, Attack, Wait };
const char* label(Action a);

struct TurnState {
    bool moved = false;
    bool acted = false;
};

std::vector<Action> legalActions(const grid::BattleGrid& g, const game::Unit& u,
                                 const std::vector<game::Unit*>& units, const TurnState& s,
                                 const rules::Tunables& t = {});
std::vector<const game::Unit*> upcoming(const turn::Manager& tm, const std::vector<game::Unit*>& units, int n);

class ActionMenu {
public:
    using Handler = std::function<void(Action)>;
    void build(ui::UIElement* layer, const Layout& l, Handler onPick);
    void show(const std::vector<Action>& legal);
    const std::vector<Action>& visible() const { return visible_; }
    std::vector<Rect> rects() const;

private:
    std::vector<ui::UIButton*> buttons_;
    std::vector<Rect> slots_;
    std::vector<Action> visible_;
};

class UnitCard {
public:
    void build(ui::UIElement* layer, const Layout& l);
    void show(const game::Unit* u, std::string name);
    void refresh();
    const std::string& nameText() const { return name_; }
    const std::string& hpText() const { return hp_; }
    const std::string& statusText() const { return status_; }

private:
    ui::UIPanel* panel_ = nullptr;
    ui::UIText* lines_[3] = {nullptr, nullptr, nullptr};
    const game::Unit* unit_ = nullptr;
    std::string name_, hp_, status_;
};

class TurnOrderBar {
public:
    void build(ui::UIElement* layer, const Layout& l);
    void update(const turn::Manager& tm, const std::vector<game::Unit*>& units,
                const std::vector<std::string>& names);
    const std::vector<const game::Unit*>& order() const { return order_; }
    const std::vector<std::string>& labels() const { return labels_; }

private:
    std::vector<ui::UIPanel*> slots_;
    std::vector<ui::UIText*> texts_;
    std::vector<const game::Unit*> order_;
    std::vector<std::string> labels_;
};

void selfTest();

} // namespace gui

#endif
