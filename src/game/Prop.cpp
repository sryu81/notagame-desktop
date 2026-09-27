#include "Prop.h"

#include "Engine.h"

#include <cassert>

namespace game {

Prop::Prop(SceneNode* node) : node_(node) {}

void Prop::setScale(const math::Vec3& s) {
    scale_ = s;
}

void Prop::update(float /*dt*/) {
}

void Prop::save(engine::SaveWriter& w) const {
    w.writeFloat(scale_.x);
    w.writeFloat(scale_.y);
    w.writeFloat(scale_.z);
    w.writeU32(selected_ ? 1u : 0u);
}

void Prop::load(engine::SaveReader& r) {
    r.readFloat(scale_.x);
    r.readFloat(scale_.y);
    r.readFloat(scale_.z);
    uint32_t sel = 0;
    r.readU32(sel);
    selected_ = (sel != 0u);
}

void Prop::selfTest() {
    aout << "[prop] self-test starting" << std::endl;

    {
        SceneNode n;
        Prop p(&n);
        assert(p.node() == &n);
        assert(p.isSelected() == false);
        assert(math::approxEqual(p.scale().x, 1.0f));
        p.setSelected(true);
        assert(p.isSelected());
        p.setScale(math::Vec3(2.0f, 3.0f, 4.0f));
        assert(math::approxEqual(p.scale().x, 2.0f));
    }

    {
        SceneNode n;
        Prop p(&n);
        p.setScale(math::Vec3(0.5f, 1.5f, 2.0f));
        p.setSelected(true);

        engine::SaveWriter w;
        p.save(w);
        const auto& buf = w.bytes();
        assert(!buf.empty());

        Prop p2(&n);
        engine::SaveReader r(buf.data(), buf.size());
        p2.load(r);
        assert(math::approxEqual(p2.scale().x, 0.5f));
        assert(math::approxEqual(p2.scale().y, 1.5f));
        assert(math::approxEqual(p2.scale().z, 2.0f));
        assert(p2.isSelected());
    }

    aout << "[prop] self-test PASS" << std::endl;
}

} // namespace game
