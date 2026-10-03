#ifndef NOT_A_GAME_RULES_H
#define NOT_A_GAME_RULES_H

#include <cstdint>
#include <vector>

#include "Math.h"
#include "Unit.h"

namespace grid { class BattleGrid; }

namespace rules {

struct Tunables {
    float stepLimit      = 0.5f;
    bool  diagonal       = false;
    float coverPct       = 0.25f;
    bool  elevationBonus = true;
    int   elevationAtk   = 2;
    float highGround     = 0.5f;
    bool  variance       = false;
    float eyeHeight      = 0.6f;
};

struct DamageContext {
    float    attackerY = 0.0f;
    float    targetY   = 0.0f;
    bool     cover     = false;
    uint32_t roll      = 0;
};

enum class AttackCheck { Ok, Dead, Allied, Stunned, OutOfRange, NoLineOfSight };

bool allied(unit::Faction a, unit::Faction b);
int  distance(const math::Vec2& a, const math::Vec2& b);

std::vector<math::Vec2> reachable(const grid::BattleGrid& g, const game::Unit& u,
                                  const Tunables& t = {});
std::vector<math::Vec2> pathTo(const grid::BattleGrid& g, const game::Unit& u,
                               const math::Vec2& goal, const Tunables& t = {});

bool lineOfSight(const grid::BattleGrid& g, const math::Vec2& a, const math::Vec2& b,
                 const Tunables& t = {});

int damage(const unit::Stats& attacker, const unit::Stats& target,
           const DamageContext& ctx = {}, const Tunables& t = {});
DamageContext contextFor(const grid::BattleGrid& g, const game::Unit& attacker,
                         const game::Unit& target, uint32_t roll = 0);

AttackCheck canAttack(const grid::BattleGrid& g, const game::Unit& attacker,
                      const game::Unit& target, const Tunables& t = {});
const char* toString(AttackCheck c);

void selfTest();

} // namespace rules

#endif
