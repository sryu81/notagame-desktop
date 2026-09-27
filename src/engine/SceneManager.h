#ifndef NOT_A_GAME_SCENEMANAGER_H
#define NOT_A_GAME_SCENEMANAGER_H

#include <functional>
#include <memory>
#include "Scene.h"

class Renderer;

using SceneFactory = std::function<std::unique_ptr<Scene>()>;

class SceneManager {
public:
    void start(std::unique_ptr<Scene> initial, Renderer& r);
    void replace(std::unique_ptr<Scene> next);
    void replace(SceneFactory factory);
    Scene* current() const { return current_.get(); }
    void update(float dt, Renderer& r);
    void render(Renderer& r);
    void flush(Renderer& r);

    static void selfTest();

private:
    std::unique_ptr<Scene> current_;
    std::unique_ptr<Scene> pending_;
    SceneFactory pendingFactory_;
};

#endif //NOT_A_GAME_SCENEMANAGER_H
