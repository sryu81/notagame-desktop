#include "BattleData.h"

#include <algorithm>

#include "Unit.h"

namespace battle {

namespace {

bool highlighted(const math::Vec2& c) {
    return std::any_of(d().highlight.begin(), d().highlight.end(),
                       [&](const math::Vec2& h) { return h.x == c.x && h.y == c.y; });
}

pick::Picker::Prefer intent() {
    if (d().mode == Mode::ChooseTarget) return [](const game::Unit* u) { return highlighted(u->cell()); };
    return {};
}

}

void handleTap(float x, float y, Renderer& r) {
    if (d().ui) {
        if (ui::UIButton* btn = d().ui->hitTestButton(math::Vec2(x, y))) {
            btn->handleClick();
            return;
        }
    }
    if (!d().grid || d().outcome != flow::Outcome::Ongoing) return;
    const pick::Result res = d().picker.pick(r.camera(), r.width(), r.height(), x, y, *d().grid, d().units, intent());
    const bool myTurn = playerControls(d().turnUnit) && d().actions->isIdle();
    if (myTurn && d().mode == Mode::ChooseMove) {
        if (res.onGrid && highlighted(res.cell)) {
            playerMove(res.cell);
            return;
        }
    } else if (myTurn && d().mode == Mode::ChooseTarget) {
        if (res.unit && highlighted(res.unit->cell())) {
            playerAttack(res.unit, r);
            return;
        }
    }
    if (myTurn && (d().mode == Mode::ChooseMove || d().mode == Mode::ChooseTarget)) {
        showHighlight({}, math::Vec3(0));
        d().mode = Mode::Menu;
    }
    if (res.unit) d().selected = res.unit;
}

void handleHover(float x, float y, Renderer& r) {
    if (!d().grid) return;
    pick::Picker probe(d().picker.options());
    d().hovered = probe.pick(r.camera(), r.width(), r.height(), x, y, *d().grid, d().units, intent()).unit;
}

} // namespace battle
