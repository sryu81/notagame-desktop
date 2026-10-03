#ifndef NOT_A_GAME_AI_H
#define NOT_A_GAME_AI_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Math.h"
#include "Rules.h"
#include "Unit.h"

namespace grid { class BattleGrid; }

namespace ai {

class Rng {
public:
    explicit Rng(uint32_t seed) : state_(seed ? seed : 0x9E3779B9u) {}
    uint32_t next();
    int below(int n) { return n > 0 ? static_cast<int>(next() % static_cast<uint32_t>(n)) : 0; }
    uint32_t state() const { return state_; }
    void setState(uint32_t s) { state_ = s; }

private:
    uint32_t state_;
};

struct Plan {
    std::vector<math::Vec2> path;
    game::Unit* target = nullptr;
};

Plan planTurn(const game::Unit& u, const grid::BattleGrid& g, const std::vector<game::Unit*>& units,
              const rules::Tunables& t, Rng& rng);

bool attackableFrom(const grid::BattleGrid& g, const game::Unit& u, const math::Vec2& cell,
                    const game::Unit& enemy, const rules::Tunables& t);
int nearestEnemyDistance(const game::Unit& u, const math::Vec2& cell,
                         const std::vector<game::Unit*>& units);

struct SimUnit {
    std::string   type;
    unit::Faction faction = unit::Faction::Player;
    unit::Stats   stats;
};

struct SimConfig {
    uint32_t        seed      = 1;
    int             size      = 8;
    int             walls     = 6;
    int             hills     = 4;
    int             maxRounds = 30;
    rules::Tunables tun;
};

struct TurnRecord {
    int  unit = -1;
    int  round = 0;
    bool legalMove = true;
    bool legalAttack = true;
    bool couldAttack = false;
    bool attacked = false;
    bool moved = false;
    bool couldApproach = false;
    bool approached = false;
};

struct Battle {
    std::unique_ptr<grid::BattleGrid>        grid;
    std::vector<std::unique_ptr<game::Unit>> units;
    std::vector<std::string>                 types;
};

struct SimResult {
    int rounds = 0;
    int winner = -1;
    std::vector<TurnRecord> turns;
    std::string log;
    std::map<std::string, int> damageByType;
};

Battle setupBattle(const SimConfig& cfg, const std::vector<SimUnit>& roster);
int winnerOf(const std::vector<game::Unit*>& units);
SimResult simulate(const SimConfig& cfg, const std::vector<SimUnit>& roster);
std::string turnLog(int round, int unit, const game::Unit& u, int target, int damage);
std::vector<SimUnit> defaultRoster();

void selfTest();

} // namespace ai

#endif
