#ifndef NOT_A_GAME_SCENE_H
#define NOT_A_GAME_SCENE_H

class Renderer;
class SceneNode;

class Scene {
public:
    virtual ~Scene() = default;
    virtual void enter(Renderer& r) = 0;
    virtual void exit(Renderer& r) = 0;
    virtual void update(float dt, Renderer& r) = 0;
    virtual void render(Renderer& r) = 0;
    virtual void onResize(int w, int h, Renderer& r) {}
    virtual SceneNode* shadowRoot() const { return nullptr; }
};

#endif //NOT_A_GAME_SCENE_H
