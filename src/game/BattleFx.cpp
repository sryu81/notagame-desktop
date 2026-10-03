#include "BattleFx.h"

#include <cassert>

#include "Engine.h"
#include "Unit.h"

namespace fx {

bool worldToScreen(const Camera& cam, const math::Vec3& p, int w, int h, math::Vec2& out) {
    const math::Mat4 vp = cam.viewProjection();
    const float x = vp.m[0] * p.x + vp.m[4] * p.y + vp.m[8] * p.z + vp.m[12];
    const float y = vp.m[1] * p.x + vp.m[5] * p.y + vp.m[9] * p.z + vp.m[13];
    const float cw = vp.m[3] * p.x + vp.m[7] * p.y + vp.m[11] * p.z + vp.m[15];
    if (cw <= 1e-6f) return false;
    out = math::Vec2((x / cw * 0.5f + 0.5f) * static_cast<float>(w),
                     (0.5f - y / cw * 0.5f) * static_cast<float>(h));
    return true;
}

void FloatingTexts::attach(ui::UIElement* layer, int poolSize, int scale) {
    pool_.clear();
    scale_ = scale;
    if (!layer) return;
    for (int i = 0; i < poolSize; ++i) {
        auto t = std::make_unique<ui::UIText>(math::Vec2(0, 0), "", math::Vec4(1, 1, 1, 1), scale);
        t->setVisible(false);
        pool_.push_back(t.get());
        layer->addChild(std::move(t));
    }
}

void FloatingTexts::spawn(const math::Vec3& world, std::string text, const math::Vec4& color,
                          float life, float rise) {
    Entry e;
    e.world = world;
    e.text = std::move(text);
    e.color = color;
    e.life = life;
    e.rise = rise;
    entries_.push_back(e);
    if (!pool_.empty() && entries_.size() > pool_.size()) entries_.erase(entries_.begin());
}

void FloatingTexts::update(float dt) {
    for (auto& e : entries_) e.age += dt;
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [](const Entry& e) { return e.age >= e.life; }),
                   entries_.end());
}

void FloatingTexts::layout(const Camera& cam, int w, int h) {
    for (auto& e : entries_) {
        const math::Vec3 p = e.world + math::Vec3(0.0f, e.rise * (e.age / e.life), 0.0f);
        e.onScreen = worldToScreen(cam, p, w, h, e.screen);
    }
    for (size_t i = 0; i < pool_.size(); ++i) {
        ui::UIText* t = pool_[i];
        if (i >= entries_.size() || !entries_[i].onScreen) {
            t->setVisible(false);
            continue;
        }
        const Entry& e = entries_[i];
        const float tw = static_cast<float>(ui::BitmapFont::stringWidth(e.text) * scale_);
        const float th = static_cast<float>(ui::BitmapFont::glyphHeight() * scale_);
        t->setText(e.text);
        t->setColor(math::Vec4(e.color.x, e.color.y, e.color.z, e.alpha()));
        t->setPosition(math::Vec2(e.screen.x - tw * 0.5f, e.screen.y - th));
        t->setVisible(true);
    }
}

void FloatingTexts::clear() {
    entries_.clear();
    for (ui::UIText* t : pool_) t->setVisible(false);
}

void StatusIcons::attach(ui::UIElement* layer, int poolSize) {
    pool_.clear();
    if (!layer) return;
    for (int i = 0; i < poolSize; ++i) {
        auto t = std::make_unique<ui::UIText>(math::Vec2(0, 0), "", math::Vec4(1.0f, 0.85f, 0.3f, 1.0f), 2);
        t->setVisible(false);
        pool_.push_back(t.get());
        layer->addChild(std::move(t));
    }
}

std::string StatusIcons::iconText(const game::Unit& u) {
    if (u.isDead()) return "";
    std::string s;
    if (u.hasStatus(unit::StatusType::Stunned)) s += "STN";
    if (u.hasStatus(unit::StatusType::Poisoned)) s += s.empty() ? "PSN" : " PSN";
    return s;
}

void StatusIcons::update(const std::vector<game::Unit*>& units, const Camera& cam, int w, int h, float headY) {
    texts_.assign(units.size(), "");
    screens_.assign(units.size(), math::Vec2(0, 0));
    for (size_t i = 0; i < units.size(); ++i) {
        game::Unit* u = units[i];
        if (!u || !u->node()) continue;
        texts_[i] = iconText(*u);
        const math::Mat4 m = u->node()->world();
        if (!worldToScreen(cam, math::Vec3(m.m[12], m.m[13] + headY, m.m[14]), w, h, screens_[i]))
            texts_[i].clear();
    }
    for (size_t i = 0; i < pool_.size(); ++i) {
        ui::UIText* t = pool_[i];
        if (i >= texts_.size() || texts_[i].empty()) {
            t->setVisible(false);
            continue;
        }
        const float tw = static_cast<float>(ui::BitmapFont::stringWidth(texts_[i]) * 2);
        t->setText(texts_[i]);
        t->setPosition(math::Vec2(screens_[i].x - tw * 0.5f, screens_[i].y - 16.0f));
        t->setVisible(true);
    }
}

const std::string& StatusIcons::textFor(size_t i) const {
    static const std::string kEmpty;
    return i < texts_.size() ? texts_[i] : kEmpty;
}

math::Vec2 StatusIcons::screenFor(size_t i) const {
    return i < screens_.size() ? screens_[i] : math::Vec2(0, 0);
}

void emitTrail(particle::Emitter& e, const math::Vec3& pos, float dt, float rate) {
    const int n = std::max(1, static_cast<int>(rate * dt + 0.5f));
    for (int i = 0; i < n; ++i)
        e.emit(pos, math::Vec3(0.0f, 0.2f, 0.0f), math::Vec4(1.0f, 0.85f, 0.4f, 0.9f), 0.12f, 0.35f);
}

void selfTest() {
    aout << "[fx] self-test starting" << std::endl;

    Camera cam;
    cam.setViewportSize(1280, 720);
    cam.setPerspective(math::radians(30.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
    cam.setViewLookAt(math::Vec3(0, 5, 10), math::Vec3(0, 0, 0), math::Vec3(0, 1, 0));
    math::Vec2 s;
    assert(worldToScreen(cam, math::Vec3(0, 0, 0), 1280, 720, s));
    assert(math::approxEqual(s.x, 640.0f, 0.5f) && math::approxEqual(s.y, 360.0f, 0.5f));
    assert(!worldToScreen(cam, math::Vec3(0, 5, 20), 1280, 720, s));

    FloatingTexts ft;
    ft.spawn(math::Vec3(0, 0, 0), "7", math::Vec4(1, 0.3f, 0.2f, 1), 1.0f, 1.0f);
    ft.layout(cam, 1280, 720);
    const math::Vec2 s0 = ft.entries()[0].screen;
    ft.update(0.5f);
    ft.layout(cam, 1280, 720);
    assert(ft.entries()[0].screen.y < s0.y && math::approxEqual(ft.entries()[0].alpha(), 0.5f, 1e-4f));
    ft.update(0.6f);
    assert(ft.entries().empty());

    game::Unit u(nullptr, unit::Faction::Enemy, unit::Stats{});
    assert(StatusIcons::iconText(u).empty());
    u.applyStatus({unit::StatusType::Poisoned, 2, 1});
    u.applyStatus({unit::StatusType::Stunned, 1, 0});
    assert(StatusIcons::iconText(u) == "STN PSN");

    particle::Emitter e(64);
    emitTrail(e, math::Vec3(1, 2, 3), 1.0f / 60.0f);
    assert(e.activeCount() >= 1 && math::approxEqual(e.particles()[0].position.y, 2.0f));

    aout << "[fx] self-test PASS" << std::endl;
}

} // namespace fx
