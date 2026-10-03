#ifndef NOT_A_GAME_PICKER_H
#define NOT_A_GAME_PICKER_H

#include <functional>
#include <vector>

#include "Math.h"
#include "Picking.h"

class Camera;
namespace grid { class BattleGrid; }
namespace game { class Unit; }

namespace pick {

struct Options {
    float bodyHeight   = 1.9f;
    float bodyRadius   = 0.3f;
    float tolerancePx  = 28.0f;
    float cycleRadiusPx = 10.0f;
};

enum class How { None, Body, Cell, Near };
const char* toString(How h);

struct Result {
    game::Unit* unit = nullptr;
    How how = How::None;
    bool onGrid = false;
    math::Vec2 cell{-1.0f, -1.0f};
    int candidates = 0;
};

picking::Ray rayAt(const Camera& cam, float x, float y, int w, int h);
bool cellAt(const grid::BattleGrid& g, const picking::Ray& ray, math::Vec2& out);

struct Body {
    math::Vec2 feet;
    math::Vec2 head;
    float radiusPx = 0.0f;
    float depth = 0.0f;
    bool visible = false;
};
Body project(const Camera& cam, int w, int h, const game::Unit& u, const Options& o = {});
float distanceToBody(const Body& b, float x, float y);

class Picker {
public:
    using Prefer = std::function<bool(const game::Unit*)>;

    explicit Picker(Options o = {}) : opt_(o) {}
    Result pick(const Camera& cam, int w, int h, float x, float y, const grid::BattleGrid& g,
                const std::vector<game::Unit*>& units, const Prefer& prefer = {});
    void resetCycle() { lastStack_.clear(); }
    const Options& options() const { return opt_; }

private:
    Options opt_;
    float lastX_ = -1e9f;
    float lastY_ = -1e9f;
    std::vector<const game::Unit*> lastStack_;
    size_t lastIndex_ = 0;
};

void selfTest();

} // namespace pick

#endif
