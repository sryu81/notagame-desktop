#ifndef NOT_A_GAME_TURN_H
#define NOT_A_GAME_TURN_H

#include <cstddef>
#include <map>
#include <vector>

namespace game { class Unit; }
namespace unit {
    enum class Faction;
}

namespace turn {

class Manager {
public:
    Manager();

    void addUnit(game::Unit *u);
    void setUnits(const std::vector<game::Unit *> &units);
    size_t unitCount() const { return units_.size(); }
    bool empty() const { return units_.empty(); }
    bool hasAliveUnit() const;

    void update(float dt);
    void endTurn();

    game::Unit *currentUnit() const;
    unit::Faction currentFaction() const;
    size_t turnIndex() const { return currentIndex_; }
    bool isPlayerTurn() const;
    float elapsed() const { return elapsed_; }
    float currentDuration() const;

    size_t skippedTurns() const { return skipped_; }
    void setTimed(bool timed) { timed_ = timed; }
    bool timed() const { return timed_; }
    int round() const { return round_; }
    void setRound(int r) { round_ = r; }

    void reset();
    void restore(int index, float elapsed);

    static void selfTest();

private:
    void advance();

    std::vector<game::Unit *> units_;
    std::map<unit::Faction, float> durations_;
    size_t currentIndex_ = 0;
    float elapsed_ = 0.0f;
    size_t skipped_ = 0;
    bool timed_ = true;
    int round_ = 1;
};

} // namespace turn

#endif
