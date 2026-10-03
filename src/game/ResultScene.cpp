#include "ResultScene.h"

#include <GLES3/gl3.h>

#include "Engine.h"
#include "HubScene.h"

ResultScene::ResultScene(game::GameState& gs, flow::Outcome outcome, std::string summary)
    : gs_(gs), outcome_(outcome), summary_(std::move(summary)) {}

std::unique_ptr<Scene> ResultScene::continueScene() const {
    return std::make_unique<HubScene>(gs_);
}

void ResultScene::enter(Renderer& r) {
    ui_ = std::make_unique<ui::UIRoot>();
    ui_->init();
    const int w = r.width() > 0 ? r.width() : 1280;
    const int h = r.height() > 0 ? r.height() : 720;
    buildUI(w, h, r);
    aout << "[result] " << flow::toString(outcome_) << " " << summary_ << std::endl;

    Renderer* rptr = &r;
    auto alive = alive_;
    ResultScene* self = this;
    subs_.push_back(EventBus::on<PointerDownEvent>([alive, self, rptr](const PointerDownEvent& e) {
        if (!*alive || !self->ui_) return;
        if (ui::UIButton* btn = self->ui_->hitTestButton(math::Vec2(e.x, e.y))) {
            btn->handleClick();
            rptr->cancelPan();
        }
    }));
}

void ResultScene::exit(Renderer& r) {
    *alive_ = false;
    for (auto id : subs_) EventBus::unsubscribe(id);
    subs_.clear();
    ui_.reset();
    menu_.reset();
}

void ResultScene::update(float, Renderer&) {}

void ResultScene::render(Renderer& r) {
    glClearColor(0.05f, 0.06f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r.renderUI(ui_.get());
}

void ResultScene::onResize(int w, int h, Renderer& r) {
    if (!ui_ || w < 1 || h < 1) return;
    buildUI(w, h, r);
}

void ResultScene::buildUI(int w, int h, Renderer& r) {
    ui_->setScreenSize(w, h);
    const float fw = static_cast<float>(w), fh = static_cast<float>(h);
    auto root = std::make_unique<ui::UIPanel>(math::Vec2(0, 0), math::Vec2(fw, fh), math::Vec4(0, 0, 0, 0));
    const bool won = outcome_ == flow::Outcome::Victory;
    auto title = std::make_unique<ui::UIText>(math::Vec2(0, 0), flow::toString(outcome_),
        won ? math::Vec4(1.0f, 0.9f, 0.3f, 1.0f) : math::Vec4(0.95f, 0.35f, 0.3f, 1.0f), 8);
    title->setPosition(math::Vec2((fw - title->size().x) * 0.5f, fh * 0.25f));
    root->addChild(std::move(title));
    auto sub = std::make_unique<ui::UIText>(math::Vec2(0, 0), summary_, math::Vec4(1, 1, 1, 1), 3);
    sub->setPosition(math::Vec2((fw - sub->size().x) * 0.5f, fh * 0.45f));
    root->addChild(std::move(sub));

    Renderer* rptr = &r;
    const ResultScene* self = this;
    menu_ = std::make_unique<game::DebugPanel>(math::Vec2((fw - 380.0f) * 0.5f, fh * 0.65f),
                                               math::Vec2(380.0f, 60.0f), 10.0f, 3);
    menu_->addAction("CONTINUE", [rptr, self]() { rptr->sceneManager().replace(self->continueScene()); });
    root->addChild(menu_->build());
    ui_->setRoot(std::move(root));
}
