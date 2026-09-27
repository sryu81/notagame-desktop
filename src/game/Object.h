#ifndef NOT_A_GAME_OBJECT_H
#define NOT_A_GAME_OBJECT_H

class SceneNode;

namespace engine {
class SaveWriter;
class SaveReader;
}

namespace game {

class Object {
public:
    virtual ~Object() = default;
    virtual SceneNode* node() = 0;
    virtual void update(float dt) = 0;
    virtual void save(engine::SaveWriter& w) const = 0;
    virtual void load(engine::SaveReader& r) = 0;
};

}

#endif
