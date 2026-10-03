#include "BattleData.h"

#include <algorithm>

#include "Unit.h"

namespace battle {

void handlePointerDown(float x, float y, Renderer& r) {
    if (d().ui) {
        if (ui::UIButton* btn = d().ui->hitTestButton(math::Vec2(x, y))) {
            btn->handleClick();
            r.cancelPan();
            return;
        }
    }
    if (!d().grid || d().outcome != flow::Outcome::Ongoing) return;
    game::Unit* picked = pickUnit(x, y, r);
    math::Vec2 cell;
    const bool onGrid = cellAtScreen(x, y, r, cell);
    if (!picked && onGrid) picked = unitAtCell(cell);
    auto highlighted = [](const math::Vec2& c) {
        return std::any_of(d().highlight.begin(), d().highlight.end(),
                           [&](const math::Vec2& h) { return h.x == c.x && h.y == c.y; });
    };
    const bool myTurn = playerControls(d().turnUnit) && d().actions->isIdle();
    if (myTurn && d().mode == Mode::ChooseMove) {
        if (onGrid && !picked && highlighted(cell)) {
            playerMove(cell);
            r.cancelPan();
            return;
        }
    } else if (myTurn && d().mode == Mode::ChooseTarget) {
        if (picked && highlighted(picked->cell())) {
            playerAttack(picked, r);
            r.cancelPan();
            return;
        }
    }
    if (myTurn && (d().mode == Mode::ChooseMove || d().mode == Mode::ChooseTarget)) {
        showHighlight({}, math::Vec3(0));
        d().mode = Mode::Menu;
    }
    if (picked) d().selected = picked;
}

} // namespace battle
