#include "Picker.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

#include "BattleFx.h"
#include "BattleGrid.h"
#include "Engine.h"
#include "Unit.h"

namespace pick {

const char* toString(How h) {
    switch (h) {
        case How::None: return "NONE";
        case How::Body: return "BODY";
        case How::Cell: return "CELL";
        case How::Near: return "NEAR";
    }
    return "?";
}

picking::Ray rayAt(const Camera& cam, float x, float y, int w, int h) {
    return picking::fromScreen(x, y, w, h, cam.viewProjection().inverted());
}

bool cellAt(const grid::BattleGrid& g, const picking::Ray& ray, math::Vec2& out) {
    if (std::abs(ray.direction.y) < 1e-6f) return false;
    float best = std::numeric_limits<float>::infinity();
    for (int cy = 0; cy < g.height(); ++cy)
        for (int cx = 0; cx < g.width(); ++cx) {
            const float t = (g.cellY(cx, cy) - ray.origin.y) / ray.direction.y;
            if (t < 0.0f || t >= best) continue;
            const math::Vec2 c = g.worldToCell(ray.origin + ray.direction * t);
            if (static_cast<int>(c.x) == cx && static_cast<int>(c.y) == cy) {
                best = t;
                out = math::Vec2(static_cast<float>(cx), static_cast<float>(cy));
            }
        }
    return best < std::numeric_limits<float>::infinity();
}

Body project(const Camera& cam, int w, int h, const game::Unit& u, const Options& o) {
    Body b;
    SceneNode* n = const_cast<game::Unit&>(u).node();
    if (!n) return b;
    const math::Mat4 m = n->world();
    const math::Vec3 feet(m.m[12], m.m[13], m.m[14]);
    const float s = u.baseScale();
    const math::Vec3 head = feet + math::Vec3(0.0f, o.bodyHeight * s, 0.0f);
    if (!fx::worldToScreen(cam, feet, w, h, b.feet) || !fx::worldToScreen(cam, head, w, h, b.head)) return b;
    const float len = (b.head - b.feet).length();
    b.radiusPx = std::max(4.0f, len * o.bodyRadius / o.bodyHeight);
    b.depth = (feet - cam.eye()).length();
    b.visible = true;
    return b;
}

float distanceToBody(const Body& b, float x, float y) {
    const math::Vec2 p(x, y);
    const math::Vec2 ab = b.head - b.feet;
    const float len2 = ab.dot(ab);
    const float t = len2 > 1e-6f ? std::clamp((p - b.feet).dot(ab) / len2, 0.0f, 1.0f) : 0.0f;
    return (p - (b.feet + ab * t)).length();
}

Result Picker::pick(const Camera& cam, int w, int h, float x, float y, const grid::BattleGrid& g,
                    const std::vector<game::Unit*>& units, const Prefer& prefer) {
    Result res;
    res.onGrid = cellAt(g, rayAt(cam, x, y, w, h), res.cell);

    struct Cand { game::Unit* u; int tier; float key; };
    std::vector<Cand> cands;
    for (game::Unit* u : units) {
        if (!u || !u->pickable()) continue;
        const Body b = project(cam, w, h, *u, opt_);
        if (!b.visible) continue;
        const float d = distanceToBody(b, x, y);
        if (d <= b.radiusPx) cands.push_back({u, 0, b.depth});
        else if (res.onGrid && u->cell().x == res.cell.x && u->cell().y == res.cell.y) cands.push_back({u, 1, 0.0f});
        else if (d <= b.radiusPx + opt_.tolerancePx) cands.push_back({u, 2, d});
    }
    if (prefer && std::any_of(cands.begin(), cands.end(), [&](const Cand& c) { return prefer(c.u); }))
        cands.erase(std::remove_if(cands.begin(), cands.end(), [&](const Cand& c) { return !prefer(c.u); }),
                    cands.end());
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
        return a.tier != b.tier ? a.tier < b.tier : a.key < b.key;
    });
    res.candidates = static_cast<int>(cands.size());
    if (cands.empty()) {
        lastStack_.clear();
        return res;
    }
    std::vector<const game::Unit*> stack;
    for (const Cand& c : cands) if (c.tier == cands.front().tier) stack.push_back(c.u);
    const bool sameSpot = std::abs(x - lastX_) <= opt_.cycleRadiusPx && std::abs(y - lastY_) <= opt_.cycleRadiusPx;
    lastIndex_ = (sameSpot && stack == lastStack_ && stack.size() > 1) ? (lastIndex_ + 1) % stack.size() : 0;
    lastStack_ = stack;
    lastX_ = x;
    lastY_ = y;
    res.unit = const_cast<game::Unit*>(stack[lastIndex_]);
    res.how = cands.front().tier == 0 ? How::Body : cands.front().tier == 1 ? How::Cell : How::Near;
    return res;
}

void selfTest() {
    aout << "[pick] self-test starting" << std::endl;

    Camera cam;
    cam.setViewportSize(1280, 720);
    cam.setPerspective(math::radians(30.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
    cam.setViewLookAt(math::Vec3(0, 8, 10), math::Vec3(0, 0, 0), math::Vec3(0, 1, 0));
    grid::BattleGrid g(6, 6, 1.0f);
    SceneNode root;
    auto unitAt = [&](int cx, int cy, unit::Faction f) {
        auto u = std::make_unique<game::Unit>(root.addChild(), f, unit::Stats{});
        u->setCell(math::Vec2(static_cast<float>(cx), static_cast<float>(cy)));
        u->node()->setLocal(math::Mat4::translation(g.cellToWorld(cx, cy)) * math::Mat4::scale(math::Vec3(0.5f)));
        u->setBaseScale(0.5f);
        return u;
    };
    auto a = unitAt(2, 3, unit::Faction::Player);
    auto b = unitAt(4, 3, unit::Faction::Enemy);
    std::vector<game::Unit*> all{a.get(), b.get()};
    Picker p;

    const Body ba = project(cam, 1280, 720, *a);
    assert(ba.visible && ba.head.y < ba.feet.y);
    const math::Vec2 mid = (ba.feet + ba.head) * 0.5f;
    Result r = p.pick(cam, 1280, 720, mid.x, mid.y, g, all);
    assert(r.unit == a.get() && r.how == How::Body);

    const math::Vec2 off = mid + math::Vec2(ba.radiusPx + 15.0f, 0.0f);
    r = p.pick(cam, 1280, 720, off.x, off.y, g, all);
    assert(r.unit == a.get() && r.how != How::Body);
    const math::Vec2 far = mid + math::Vec2(ba.radiusPx + 80.0f, -60.0f);
    r = p.pick(cam, 1280, 720, far.x, far.y, g, all);
    assert(r.unit == nullptr);

    r = p.pick(cam, 1280, 720, mid.x, mid.y, g, all, [&](const game::Unit* u) { return u == b.get(); });
    assert(r.unit == a.get());

    a->takeDamage(999);
    r = p.pick(cam, 1280, 720, mid.x, mid.y, g, all);
    assert(r.unit == nullptr);

    math::Vec2 cell;
    assert(cellAt(g, rayAt(cam, 640.0f, 360.0f, 1280, 720), cell));
    assert(g.inBounds(static_cast<int>(cell.x), static_cast<int>(cell.y)));

    aout << "[pick] self-test PASS" << std::endl;
}

} // namespace pick
