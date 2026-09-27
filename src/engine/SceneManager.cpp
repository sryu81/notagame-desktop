#include "SceneManager.h"

#include <cassert>

#include "AndroidOut.h"

void SceneManager::start(std::unique_ptr<Scene> initial, Renderer& r) {
    assert(initial != nullptr);
    current_ = std::move(initial);
    current_->enter(r);
}

void SceneManager::replace(std::unique_ptr<Scene> next) {
    assert(next != nullptr);
    pending_ = std::move(next);
    pendingFactory_ = nullptr;
}

void SceneManager::replace(SceneFactory factory) {
    assert(factory != nullptr);
    pendingFactory_ = std::move(factory);
    pending_ = nullptr;
}

void SceneManager::update(float dt, Renderer& r) {
    if (current_) current_->update(dt, r);
}

void SceneManager::render(Renderer& r) {
    if (current_) current_->render(r);
}

void SceneManager::flush(Renderer& r) {
    std::unique_ptr<Scene> next;
    if (pendingFactory_) {
        next = pendingFactory_();
        pendingFactory_ = nullptr;
    } else if (pending_) {
        next = std::move(pending_);
    }
    if (!next) return;
    if (current_) current_->exit(r);
    current_ = std::move(next);
    current_->enter(r);
}

void SceneManager::selfTest() {
    struct CountScene : Scene {
        int enters = 0, exits = 0, updates = 0, renders = 0;
        void enter(Renderer&)  override { ++enters; }
        void exit(Renderer&)   override { ++exits; }
        void update(float, Renderer&) override { ++updates; }
        void render(Renderer&) override { ++renders; }
    };

    SceneManager mgr;
    assert(mgr.current() == nullptr);

    auto* a = new CountScene();
    auto* b = new CountScene();

    mgr.start(std::unique_ptr<Scene>(a), *static_cast<Renderer*>(nullptr));
    assert(mgr.current() == a);
    assert(a->enters == 1 && a->exits == 0);

    mgr.update(0.1f, *static_cast<Renderer*>(nullptr));
    mgr.render(*static_cast<Renderer*>(nullptr));
    assert(a->updates == 1 && a->renders == 1);

    mgr.replace(std::unique_ptr<Scene>(b));
    assert(mgr.current() == a);

    mgr.flush(*static_cast<Renderer*>(nullptr));
    assert(mgr.current() == b);
    assert(a->exits == 1 && b->enters == 1);

    aout << "[scene] self-test PASS" << std::endl;
}
