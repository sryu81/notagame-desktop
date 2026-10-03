#include "LevelSelectScene.h"

#include <GLES3/gl3.h>

#include "BattleData.h"
#include "BattleScene.h"
#include "Engine.h"
#include "HubScene.h"

void LevelSelectScene::enter(Renderer& r) {
    ui_ = std::make_unique<ui::UIRoot>();
    ui_->init();
    buildUI(r.width() > 0 ? r.width() : 1280, r.height() > 0 ? r.height() : 720, r);
    Renderer* rptr = &r;
    auto alive = alive_;
    LevelSelectScene* self = this;
    subs_.push_back(EventBus::on<PointerDownEvent>([alive, self, rptr](const PointerDownEvent& e) {
        if (!*alive || !self->ui_) return;
        if (ui::UIButton* btn = self->ui_->hitTestButton(math::Vec2(e.x, e.y))) {
            btn->handleClick();
            rptr->cancelPan();
        }
    }));
}

void LevelSelectScene::exit(Renderer& r) {
    *alive_ = false;
    for (auto id : subs_) EventBus::unsubscribe(id);
    subs_.clear();
    ui_.reset();
    menu_.reset();
}

void LevelSelectScene::render(Renderer& r) {
    glClearColor(0.06f, 0.07f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r.renderUI(ui_.get());
}

void LevelSelectScene::onResize(int w, int h, Renderer& r) {
    if (!ui_ || w < 1 || h < 1) return;
    buildUI(w, h, r);
}

void LevelSelectScene::buildUI(int w, int h, Renderer& r) {
    ui_->setScreenSize(w, h);
    const float fw = static_cast<float>(w), fh = static_cast<float>(h);
    auto root = std::make_unique<ui::UIPanel>(math::Vec2(0, 0), math::Vec2(fw, fh), math::Vec4(0, 0, 0, 0));
    auto title = std::make_unique<ui::UIText>(math::Vec2(0, 0), "BATTLE", math::Vec4(1.0f, 0.9f, 0.3f, 1.0f), 6);
    title->setPosition(math::Vec2((fw - title->size().x) * 0.5f, fh * 0.08f));
    const float top = fh * 0.08f + title->size().y + 32.0f;
    root->addChild(std::move(title));

    Renderer* rptr = &r;
    game::GameState* gs = &gs_;
    menu_ = std::make_unique<game::DebugPanel>(math::Vec2((fw - 380.0f) * 0.5f, top), math::Vec2(380.0f, 60.0f), 10.0f, 3);
    menu_->addRow("LEVEL", [this]() { return std::string(battle::levelName(level_)); },
                  [this]() { level_ = level_ % battle::kLevelCount + 1; });
    menu_->addRow("MODE", [this]() { return std::string(autoBattle_ ? "AUTO" : "PLAY"); },
                  [this]() { autoBattle_ = !autoBattle_; });
    menu_->addAction("START", [this, rptr, gs]() {
        rptr->sceneManager().replace(std::make_unique<BattleScene>(*gs, level_, autoBattle_));
    });
    menu_->addAction("BACK", [rptr, gs]() { rptr->sceneManager().replace(std::make_unique<HubScene>(*gs)); });
    root->addChild(menu_->build());
    ui_->setRoot(std::move(root));
}
