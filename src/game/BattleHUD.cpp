#include "BattleData.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include "GameState.h"
#include "HubScene.h"
#include "Unit.h"

namespace battle {

namespace {

const float kSpeeds[]      = {0.5f, 1.0f, 2.0f, 4.0f};
const char* kSpeedLabels[] = {"0.5X", "1X", "2X", "4X"};

}

void buildHUD(Renderer& r, game::GameState& gs) {
    if (!d().ui) {
        d().ui = std::make_unique<ui::UIRoot>();
        d().ui->init();
    }
    const int w = r.width()  > 0 ? r.width()  : 1280;
    const int h = r.height() > 0 ? r.height() : 720;
    d().ui->setScreenSize(w, h);
    const float fw = static_cast<float>(w), fh = static_cast<float>(h);
    const gui::Layout layout = gui::Layout::compute(w, h);
    const float u = layout.unit;

    auto root = std::make_unique<ui::UIPanel>(math::Vec2(0, 0), math::Vec2(fw, fh), math::Vec4(0, 0, 0, 0));
    auto fxLayer = std::make_unique<ui::UIPanel>(math::Vec2(0, 0), math::Vec2(fw, fh), math::Vec4(0, 0, 0, 0));
    d().numbers.attach(fxLayer.get());
    d().icons.attach(fxLayer.get());
    root->addChild(std::move(fxLayer));

    d().menu.build(root.get(), layout, [](gui::Action a) { onMenuAction(a); });
    d().card.build(root.get(), layout);
    d().bar.build(root.get(), layout);

    const float bannerY = layout.order.empty() ? 0.3f * u : layout.order.front().y + layout.order.front().h + 0.25f * u;
    auto banner = std::make_unique<ui::UIText>(math::Vec2(0, bannerY), "", math::Vec4(1, 1, 1, 1), 2);
    d().banner = banner.get();
    root->addChild(std::move(banner));
    auto fps = std::make_unique<ui::UIText>(math::Vec2(fw - 110.0f, 8.0f), "FPS:00",
                                            math::Vec4(0.5f, 1.0f, 0.5f, 1.0f), 2);
    d().fpsText = fps.get();
    root->addChild(std::move(fps));

    Renderer* rptr = &r;
    game::GameState* gsp = &gs;
    const float rowH = std::max(48.0f, 0.8f * u);
    const float rowW = 3.6f * u;
    const float top = bannerY + 1.2f * u;
    d().hud = std::make_unique<game::DebugPanel>(math::Vec2(fw - 0.4f * u - rowW, top), math::Vec2(rowW, rowH),
                                                 0.15f * rowH, layout.textScale);
    d().hud->addRow("SPEED", []() { return std::string(kSpeedLabels[d().speedIdx]); }, []() {
        d().speedIdx = (d().speedIdx + 1) % 4;
        d().speed = kSpeeds[d().speedIdx];
    });
    d().hud->addRow("AUTO", []() { return std::string(d().autoBattle ? "ON" : "OFF"); },
                    []() { d().autoBattle = !d().autoBattle; });
    d().hud->addRow("VIEW", []() { return std::string(d().sprites.enabled() ? "SPRITE" : "3D"); },
                    []() { d().sprites.setEnabled(!d().sprites.enabled()); });
    d().hud->addAction("SAVE", []() { saveGame(); });
    d().hud->addAction("LOAD", []() { loadGame(); });
    d().hud->addAction("BACK", [rptr, gsp]() { rptr->sceneManager().replace(std::make_unique<HubScene>(*gsp)); });
    root->addChild(d().hud->build());

    if (!d().checks) {
        d().checks = std::make_unique<game::LabChecklist>(math::Vec2(16, top));
        addChecklistRows(*d().checks);
    }
    root->addChild(d().checks->build());
    d().ui->setRoot(std::move(root));
}

void updateHUD(float dt, Renderer& r) {
    const int w = r.width() > 0 ? r.width() : 1280;
    const int h = r.height() > 0 ? r.height() : 720;
    if (d().banner && d().turns) {
        std::string who = "-";
        const int i = [] {
            for (size_t k = 0; k < d().units.size(); ++k) if (d().units[k] == d().turnUnit) return static_cast<int>(k);
            return -1;
        }();
        if (i >= 0) who = d().names[static_cast<size_t>(i)];
        char label[96];
        snprintf(label, sizeof(label), "%s  ROUND %d  %s'S TURN  WIN: %s", levelName(d().levelId),
                 d().turns->round(), who.c_str(), level::describe(d().level.win).c_str());
        d().banner->setText(label);
        const float tw = static_cast<float>(ui::BitmapFont::stringWidth(label) * 2);
        d().banner->setPosition(math::Vec2((static_cast<float>(w) - tw) * 0.5f, d().banner->position().y));
    }
    const bool menuOn = playerControls(d().turnUnit) && d().actions && d().actions->isIdle() &&
                        d().mode == Mode::Menu && d().outcome == flow::Outcome::Ongoing && !d().flow->waiting();
    d().menu.show(menuOn ? gui::legalActions(*d().grid, *d().turnUnit, d().units, d().turnState, d().tun)
                         : std::vector<gui::Action>{});
    const game::Unit* shown = d().hovered ? d().hovered : d().selected;
    std::string name;
    for (size_t k = 0; k < d().units.size(); ++k) if (d().units[k] == shown) name = d().names[k];
    d().card.show(shown, name);
    if (d().turns) d().bar.update(*d().turns, d().units, d().names);
    d().numbers.update(dt);
    d().numbers.layout(r.camera(), w, h);
    d().icons.update(d().units, r.camera(), w, h, 1.3f);

    d().fpsAccum += dt;
    d().fpsFrames++;
    if (d().fpsAccum >= 0.5f && d().fpsText) {
        d().fpsText->setText("FPS:" + std::to_string(static_cast<int>(d().fpsFrames / d().fpsAccum + 0.5f)));
        d().fpsAccum = 0.0f;
        d().fpsFrames = 0;
    }
}

} // namespace battle
