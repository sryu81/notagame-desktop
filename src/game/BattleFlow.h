#ifndef NOT_A_GAME_BATTLE_FLOW_H
#define NOT_A_GAME_BATTLE_FLOW_H

#include <cstdint>
#include <vector>

#include "LevelLoader.h"
#include "Saveable.h"

namespace action { class Manager; }
namespace game { class Unit; }
namespace turn { class Manager; }

namespace flow {

enum class Outcome { Ongoing, Victory, Defeat };

Outcome evaluate(const level::WinCondition& win, const std::vector<game::Unit*>& units, int round);
const char* toString(Outcome o);

class BattleFlow {
public:
    BattleFlow(turn::Manager& turns, action::Manager& actions);

    void acted() { acted_ = true; }
    bool waiting() const { return acted_; }
    bool update();

private:
    turn::Manager& turns_;
    action::Manager& actions_;
    bool acted_ = false;
};

class BattleSnapshot : public engine::Saveable {
public:
    BattleSnapshot(std::vector<game::Unit*> units, turn::Manager* turns, uint32_t* rngState = nullptr);

    void save(engine::SaveWriter& w) const override;
    void load(engine::SaveReader& r) override;
    bool loaded() const { return loaded_; }

private:
    std::vector<game::Unit*> units_;
    turn::Manager* turns_;
    uint32_t* rng_;
    bool loaded_ = false;
};

void selfTest();

} // namespace flow

#endif
