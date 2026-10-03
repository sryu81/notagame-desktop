#include "SceneManager.h"

#include <cassert>

#include "AndroidOut.h"
#include "Renderer.h"

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
    struct Counts { int enters = 0, exits = 0, updates = 0, renders = 0; };
    struct CountScene : Scene {
        Counts& c;
        explicit CountScene(Counts& counts) : c(counts) {}
        void enter(Renderer&)  override { ++c.enters; }
        void exit(Renderer&)   override { ++c.exits; }
        void update(float, Renderer&) override { ++c.updates; }
        void render(Renderer&) override { ++c.renders; }
    };

    alignas(Renderer) unsigned char rendererStorage[sizeof(Renderer)] = {};
    Renderer& fakeRenderer = *reinterpret_cast<Renderer*>(rendererStorage);

    SceneManager mgr;
    assert(mgr.current() == nullptr);

    Counts ca, cb;
    auto* a = new CountScene(ca);
    auto* b = new CountScene(cb);

    mgr.start(std::unique_ptr<Scene>(a), fakeRenderer);
    assert(mgr.current() == a);
    assert(ca.enters == 1 && ca.exits == 0);

    mgr.update(0.1f, fakeRenderer);
    mgr.render(fakeRenderer);
    assert(ca.updates == 1 && ca.renders == 1);

    mgr.replace(std::unique_ptr<Scene>(b));
    assert(mgr.current() == a);

    mgr.flush(fakeRenderer);
    assert(mgr.current() == b);
    assert(ca.exits == 1 && cb.enters == 1);

    aout << "[scene] self-test PASS" << std::endl;
}
