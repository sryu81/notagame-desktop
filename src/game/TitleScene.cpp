#include "TitleScene.h"

#include <GLES3/gl3.h>

#include "BattleScene.h"
#include "Engine.h"
#include "GameState.h"

void TitleScene::enter(Renderer& r) {
    ui_ = std::make_unique<ui::UIRoot>();
    ui_->init();
    if (r.width() > 0 && r.height() > 0) {
        ui_->setScreenSize(r.width(), r.height());
        buildUI(r.width(), r.height(), &r);
    }

    Renderer* rptr = &r;
    auto alive = alive_;
    ui::UIRoot* uiRaw = ui_.get();
    game::GameState* gs = &gs_;
    subs_.push_back(EventBus::on<PointerDownEvent>([alive, uiRaw, rptr, gs](const PointerDownEvent& e) {
        if (!*alive) return;
        if (uiRaw) {
            if (ui::UIButton* btn = uiRaw->hitTestButton(math::Vec2(e.x, e.y))) {
                btn->handleClick();
                rptr->cancelPan();
                return;
            }
        }
        rptr->cancelPan();
        rptr->sceneManager().replace(std::make_unique<BattleScene>(*gs));
    }));
}

void TitleScene::exit(Renderer& r) {
    *alive_ = false;
    for (auto id : subs_) EventBus::unsubscribe(id);
    subs_.clear();
    ui_.reset();
}

void TitleScene::render(Renderer& r) {
    glClearColor(0.05f, 0.05f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r.renderUI(ui_.get());
}

void TitleScene::onResize(int w, int h, Renderer& r) {
    if (ui_) {
        ui_->setScreenSize(w, h);
        buildUI(w, h, &r);
    }
}

void TitleScene::buildUI(int w, int h, Renderer* r) {
    auto root = std::make_unique<ui::UIPanel>(
        math::Vec2(0, 0), math::Vec2(static_cast<float>(w), static_cast<float>(h)),
        math::Vec4(0, 0, 0, 0));

    auto title = std::make_unique<ui::UIText>(
        math::Vec2(0, 0), "NOT A GAME",
        math::Vec4(1.0f, 0.9f, 0.3f, 1.0f), 8);
    const float titleW = title->size().x;
    const float titleH = title->size().y;
    title->setPosition(math::Vec2(
        (static_cast<float>(w) - titleW) * 0.5f,
        static_cast<float>(h) * 0.38f - titleH * 0.5f));
    root->addChild(std::move(title));

    auto prompt = std::make_unique<ui::UIText>(
        math::Vec2(0, 0), "TAP TO START",
        math::Vec4(0.8f, 0.8f, 0.8f, 1.0f), 4);
    const float promptW = prompt->size().x;
    prompt->setPosition(math::Vec2(
        (static_cast<float>(w) - promptW) * 0.5f,
        static_cast<float>(h) * 0.58f));
    root->addChild(std::move(prompt));

    auto exitBtn = std::make_unique<ui::UIButton>(
        math::Vec2(0, 0), math::Vec2(160, 72), "EXIT",
        [r]() { if (r) r->finish(); });
    exitBtn->setTextScale(3);
    exitBtn->setPosition(math::Vec2(
        (static_cast<float>(w) - 160.0f) * 0.5f,
        static_cast<float>(h) * 0.72f));
    root->addChild(std::move(exitBtn));

    ui_->setRoot(std::move(root));
}
