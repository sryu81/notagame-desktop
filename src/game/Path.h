#ifndef NOT_A_GAME_PATH_H
#define NOT_A_GAME_PATH_H

#include <vector>

#include "Math.h"

namespace grid {
class BattleGrid;
}

namespace path {

std::vector<math::Vec2> findPath(const grid::BattleGrid &grid, math::Vec2 start, math::Vec2 goal);

void selfTest();

} // namespace path

#endif
