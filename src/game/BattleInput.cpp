#include "BattleData.h"

#include <algorithm>
#include <cmath>

#include "Path.h"
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
    const auto ray = r.pickingRay(x, y);
    const auto hit = r.pickAt(x, y);
    aout << "[pick] tap (" << x << ", " << y << ") ray origin=("
         << ray.origin.x << ", " << ray.origin.y << ", " << ray.origin.z
         << ") dir=(" << ray.direction.x << ", " << ray.direction.y
         << ", " << ray.direction.z << ")";
    if (hit.valid) {
        aout << " HIT index=" << hit.shapeIndex << " dist=" << hit.distance;
    } else {
        aout << " miss";
    }
    aout << std::endl;

    aout << "[pick] units=" << d().registry->size()
         << " alive=" << charactersAliveCount() << std::endl;
    game::Unit* picked = pickUnit(x, y, r);
    game::Unit* sel = selectedUnit();

    if (picked != nullptr && picked != sel &&
        sel != nullptr &&
        sel->faction() == unit::Faction::Player &&
        picked->faction() == unit::Faction::Enemy &&
        isPlayerTurn()) {
        if (sel->isStunned()) {
            aout << "[action] stunned, cannot attack" << std::endl;
            return;
        }
        const math::Vec2 a = sel->cell();
        const math::Vec2 b = picked->cell();
        const int cheb = std::max(static_cast<int>(std::abs(a.x - b.x)),
                                  static_cast<int>(std::abs(a.y - b.y)));
        if (cheb > sel->stats().range) {
            aout << "[action] out of range (cheb=" << cheb
                 << " > range=" << sel->stats().range << ")" << std::endl;
            return;
        }
        if (!d().grid->hasLineOfSight(a, b)) {
            aout << "[action] LoS blocked from (" << a.x << "," << a.y
                 << ") to (" << b.x << "," << b.y << ")" << std::endl;
            return;
        }
        enqueueAttack(sel, picked);
        setPathOverlay({});
        aout << "[action] attack from (" << a.x << "," << a.y << ") to ("
             << b.x << "," << b.y << ")" << std::endl;
        return;
    }

    if (picked != nullptr) {
        if (picked->isSelected()) {
            picked->setSelected(false);
        } else {
            for (size_t i = 0; i < d().registry->size(); ++i) {
                auto* o = d().registry->unitAt(i);
                if (o && o != picked) o->setSelected(false);
            }
            picked->setSelected(true);
            math::Vec3 worldPos = d().grid->cellToWorld(
                static_cast<int>(picked->cell().x),
                static_cast<int>(picked->cell().y));
            r.camera().setTarget(worldPos);
        }
        setPathOverlay({});
        aout << "[select] unit cell=(" << picked->cell().x << ", " << picked->cell().y
             << ") faction=" << static_cast<int>(picked->faction())
             << " selected=" << (picked->isSelected() ? "true" : "false") << std::endl;
        return;
    }

    if (sel != nullptr) {
        if (!isPlayerTurn()) {
            aout << "[path] not your turn" << std::endl;
            return;
        }
        if (sel->isStunned()) {
            aout << "[path] stunned, cannot move" << std::endl;
            return;
        }
        if (!d().grid) return;
        const math::Vec2 start = sel->cell();
        const math::Vec2 goal  = cellAtScreen(x, y, r);
        aout << "[path] from (" << start.x << ", " << start.y << ") to ("
             << goal.x << ", " << goal.y << ")" << std::endl;
        if (goal.x < 0.0f || goal.y < 0.0f) {
            aout << "[path] miss: tap is off the grid" << std::endl;
            return;
        }
        if (goal.x == start.x && goal.y == start.y) {
            aout << "[path] same cell, no path" << std::endl;
            setPathOverlay({});
            return;
        }
        if (d().actionManager && !d().actionManager->isIdle()) {
            aout << "[path] action in progress, ignored" << std::endl;
            return;
        }
        const auto path = path::findPath(*d().grid, start, goal);
        if (path.empty()) {
            aout << "[path] no path" << std::endl;
            setPathOverlay({});
            return;
        }
        aout << "[path] " << path.size() << " cells:";
        for (const auto& c : path) aout << " (" << c.x << "," << c.y << ")";
        aout << std::endl;
        setPathOverlay(path);
        enqueueMove(sel, path);
        sel->setSelected(false);
        return;
    }

    for (size_t i = 0; i < d().registry->size(); ++i) {
        auto* c = d().registry->unitAt(i);
        if (c) c->setSelected(false);
    }
    setPathOverlay({});
    aout << "[select] deselect all" << std::endl;
}

} // namespace battle
