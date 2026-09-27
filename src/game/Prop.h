#ifndef NOT_A_GAME_PROP_H
#define NOT_A_GAME_PROP_H

#include "Math.h"
#include "Object.h"

class SceneNode;

namespace game {

class Prop : public Object {
public:
    explicit Prop(SceneNode* node);

    SceneNode* node() override { return node_; }
    void update(float dt) override;
    void save(engine::SaveWriter& w) const override;
    void load(engine::SaveReader& r) override;

    void         setSelected(bool sel) { selected_ = sel; }
    bool         isSelected() const    { return selected_; }
    void         setScale(const math::Vec3& s);
    const math::Vec3& scale() const    { return scale_; }

    static void selfTest();

private:
    SceneNode*  node_     = nullptr;
    math::Vec3  scale_{1.0f, 1.0f, 1.0f};
    bool        selected_ = false;
};

} // namespace game

#endif
