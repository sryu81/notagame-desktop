#ifndef NOT_A_GAME_BATTLE_FX_H
#define NOT_A_GAME_BATTLE_FX_H

#include <string>
#include <vector>

#include "Math.h"

class Camera;
namespace particle { class Emitter; }
namespace ui { class UIElement; class UIText; }
namespace game { class Unit; }

namespace fx {

bool worldToScreen(const Camera& cam, const math::Vec3& p, int w, int h, math::Vec2& out);

class FloatingTexts {
public:
    struct Entry {
        math::Vec3  world;
        std::string text;
        math::Vec4  color;
        float       age = 0.0f;
        float       life = 1.0f;
        float       rise = 0.8f;
        math::Vec2  screen{0.0f, 0.0f};
        bool        onScreen = false;
        float       alpha() const { return color.w * (1.0f - age / life); }
    };

    void attach(ui::UIElement* layer, int poolSize = 16, int scale = 3);
    void spawn(const math::Vec3& world, std::string text, const math::Vec4& color,
               float life = 1.0f, float rise = 0.8f);
    void update(float dt);
    void layout(const Camera& cam, int w, int h);
    void clear();

    const std::vector<Entry>& entries() const { return entries_; }

private:
    std::vector<Entry> entries_;
    std::vector<ui::UIText*> pool_;
    int scale_ = 3;
};

class StatusIcons {
public:
    void attach(ui::UIElement* layer, int poolSize = 12);
    void update(const std::vector<game::Unit*>& units, const Camera& cam, int w, int h, float headY = 1.1f);

    static std::string iconText(const game::Unit& u);
    const std::string& textFor(size_t unitIndex) const;
    math::Vec2 screenFor(size_t unitIndex) const;

private:
    std::vector<ui::UIText*> pool_;
    std::vector<std::string> texts_;
    std::vector<math::Vec2> screens_;
};

void emitTrail(particle::Emitter& e, const math::Vec3& pos, float dt, float rate = 90.0f);

void selfTest();

} // namespace fx

#endif
