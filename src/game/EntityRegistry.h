#ifndef NOT_A_GAME_ENTITY_REGISTRY_H
#define NOT_A_GAME_ENTITY_REGISTRY_H

#include <deque>
#include <functional>
#include <memory>
#include <vector>

#include "Animation.h"
#include "Object.h"
#include "Prop.h"
#include "Saveable.h"
#include "Unit.h"

class SceneNode;

namespace game {

class Prop;

class EntityRegistry : public engine::Saveable {
public:
    EntityRegistry() = default;
    ~EntityRegistry() = default;

    EntityRegistry(const EntityRegistry&) = delete;
    EntityRegistry& operator=(const EntityRegistry&) = delete;

    // Ownership
    size_t add(std::unique_ptr<Object> entity, SceneNode* shadowNode = nullptr);

    // Access by index
    size_t     size()  const { return entities_.size(); }
    bool       empty() const { return entities_.empty(); }
    Object*    at(size_t i)       { return entities_[i].get(); }
    const Object* at(size_t i) const { return entities_[i].get(); }

    Unit* unitAt(size_t i);
    Prop* propAt(size_t i);

    // Shadow nodes (parallel index)
    SceneNode* shadowAt(size_t i) const;
    void       setShadow(size_t i, SceneNode* sn);
    size_t     shadowCount() const { return shadows_.size(); }

    // Clip pool — used by EntityBuilder during spawn
    std::deque<anim::Clip>& clipPool() { return clipPool_; }

    // Iteration
    template<typename Fn> void eachUnit(Fn fn) {
        for (auto& e : entities_)
            if (auto* u = dynamic_cast<Unit*>(e.get())) fn(*u);
    }

    template<typename Fn> void eachProp(Fn fn) {
        for (auto& e : entities_)
            if (auto* p = dynamic_cast<Prop*>(e.get())) fn(*p);
    }

    template<typename Fn> void each(Fn fn) {
        for (auto& e : entities_) fn(*e);
    }

    // Bulk
    std::vector<Unit*> allUnits();
    size_t unitCount() const;
    void   updateAll(float dt);
    void   clear();

    // Serialization
    void save(engine::SaveWriter& w) const override;
    void load(engine::SaveReader& r) override;

    static void selfTest();

private:
    std::vector<std::unique_ptr<Object>> entities_;
    std::vector<SceneNode*>             shadows_;
    std::deque<anim::Clip>              clipPool_;
};

} // namespace game

#endif
