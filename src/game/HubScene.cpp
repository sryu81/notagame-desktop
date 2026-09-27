#include "HubScene.h"

#include <GLES3/gl3.h>

#include "BattleScene.h"
#include "Engine.h"
#include "GameState.h"
#include "TitleScene.h"

namespace {
constexpr float kRowW = 380.0f;
constexpr float kRowH = 60.0f;
constexpr float kRowGap = 10.0f;
}

std::vector<HubScene::Entry> HubScene::entries(game::GameState& gs) {
    game::GameState* g = &gs;
    return {
        {"RENDER LAB", nullptr},
        {"ANIM LAB", nullptr},
        {"FX LAB", nullptr},
        {"CAMERA LAB", nullptr},
        {"SYSTEMS LAB", nullptr},
        {"BATTLE", [g]() { return std::unique_ptr<Scene>(new BattleScene(*g)); }},
    };
}

void HubScene::enter(Renderer& r) {
    ui_ = std::make_unique<ui::UIRoot>();
    ui_->init();
    checks_ = std::make_unique<game::LabChecklist>(math::Vec2(16, 16));
    checks_->add("ENTER");
    checks_->add("RESIZE");
    checks_->add("POINTER");
    if (r.width() > 0 && r.height() > 0) {
        ui_->setScreenSize(r.width(), r.height());
        buildUI(r.width(), r.height(), r);
    }
    checks_->check("ENTER", true);

    Renderer* rptr = &r;
    auto alive = alive_;
    HubScene* self = this;
    subs_.push_back(EventBus::on<PointerDownEvent>([alive, self, rptr](const PointerDownEvent& e) {
        if (!*alive || !self->ui_) return;
        self->checks_->check("POINTER", true);
        if (ui::UIButton* btn = self->ui_->hitTestButton(math::Vec2(e.x, e.y))) {
            btn->handleClick();
        }
        rptr->cancelPan();
    }));
}

void HubScene::exit(Renderer& r) {
    *alive_ = false;
    for (auto id : subs_) EventBus::unsubscribe(id);
    subs_.clear();
    ui_.reset();
    menu_.reset();
    checks_.reset();
}

void HubScene::render(Renderer& r) {
    glClearColor(0.06f, 0.07f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r.renderUI(ui_.get());
}

void HubScene::onResize(int w, int h, Renderer& r) {
    if (!ui_ || w < 1 || h < 1) return;
    ui_->setScreenSize(w, h);
    buildUI(w, h, r);
    checks_->check("RESIZE", true);
}

void HubScene::buildUI(int w, int h, Renderer& r) {
    const float fw = static_cast<float>(w);
    const float fh = static_cast<float>(h);
    auto root = std::make_unique<ui::UIPanel>(
        math::Vec2(0, 0), math::Vec2(fw, fh), math::Vec4(0, 0, 0, 0));

    auto title = std::make_unique<ui::UIText>(
        math::Vec2(0, 0), "HUB", math::Vec4(1.0f, 0.9f, 0.3f, 1.0f), 6);
    title->setPosition(math::Vec2((fw - title->size().x) * 0.5f, fh * 0.08f));
    const float menuTop = fh * 0.08f + title->size().y + 32.0f;
    root->addChild(std::move(title));

    Renderer* rptr = &r;
    game::GameState* gs = &gs_;
    menu_ = std::make_unique<game::DebugPanel>(
        math::Vec2((fw - kRowW) * 0.5f, menuTop), math::Vec2(kRowW, kRowH), kRowGap, 3);
    for (auto& e : entries(gs_)) {
        SceneFactory f = e.factory;
        const int row = menu_->addAction(e.label, [rptr, f]() {
            if (f) rptr->sceneManager().replace(f);
        });
        menu_->setRowEnabled(row, static_cast<bool>(f));
    }
    menu_->addAction("BACK", [rptr, gs]() {
        rptr->sceneManager().replace(std::make_unique<TitleScene>(*gs));
    });
    root->addChild(menu_->build());
    root->addChild(checks_->build());

    ui_->setRoot(std::move(root));
}
