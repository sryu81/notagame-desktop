#include "DebugPanel.h"

#include <cassert>

#include "Engine.h"

namespace game {

DebugPanel::DebugPanel(math::Vec2 origin, math::Vec2 rowSize, float gap, int textScale)
        : origin_(origin), rowSize_(rowSize), gap_(gap), textScale_(textScale) {}

int DebugPanel::addRow(std::string label, Getter value, Action onClick) {
    rows_.push_back({std::move(label), std::move(value), std::move(onClick)});
    return static_cast<int>(rows_.size()) - 1;
}

int DebugPanel::addAction(std::string label, Action onClick) {
    return addRow(std::move(label), nullptr, std::move(onClick));
}

std::string DebugPanel::rowText(int row) const {
    const Row& r = rows_[row];
    if (!r.value) return r.label;
    return r.label + " " + r.value();
}

math::Vec2 DebugPanel::panelSize() const {
    const float n = static_cast<float>(rows_.size());
    const float h = n > 0.0f ? n * rowSize_.y + (n - 1.0f) * gap_ : 0.0f;
    return math::Vec2(rowSize_.x, h);
}

std::unique_ptr<ui::UIPanel> DebugPanel::build() {
    auto panel = std::make_unique<ui::UIPanel>(origin_, panelSize(), math::Vec4(0, 0, 0, 0));
    for (size_t i = 0; i < rows_.size(); ++i) {
        Row& r = rows_[i];
        const math::Vec2 pos(origin_.x, origin_.y + static_cast<float>(i) * (rowSize_.y + gap_));
        const int idx = static_cast<int>(i);
        auto btn = std::make_unique<ui::UIButton>(pos, rowSize_, rowText(idx),
            [this, idx]() {
                if (rows_[idx].onClick) rows_[idx].onClick();
                refresh();
            });
        btn->setTextScale(textScale_);
        btn->setEnabled(r.enabled);
        r.button = btn.get();
        panel->addChild(std::move(btn));
    }
    panel->setVisible(visible_);
    panel_ = panel.get();
    return panel;
}

void DebugPanel::refresh() {
    for (size_t i = 0; i < rows_.size(); ++i) {
        if (rows_[i].button) rows_[i].button->setText(rowText(static_cast<int>(i)));
    }
}

void DebugPanel::setVisible(bool v) {
    visible_ = v;
    if (panel_) panel_->setVisible(v);
}

void DebugPanel::setRowEnabled(int row, bool enabled) {
    Row& r = rows_[row];
    r.enabled = enabled;
    if (r.button) r.button->setEnabled(enabled);
}

bool DebugPanel::rowEnabled(int row) const {
    return rows_[row].enabled;
}

ui::UIButton* DebugPanel::rowButton(int row) const {
    return rows_[row].button;
}

void DebugPanel::selfTest() {
    aout << "[debug-panel] self-test starting" << std::endl;

    {
        int value = 1;
        int clicks = 0;
        DebugPanel p(math::Vec2(10, 20), math::Vec2(200, 50), 10.0f, 2);
        const int r0 = p.addRow("SPEED", [&value]() { return std::to_string(value); },
                                [&value]() { value = value % 3 + 1; });
        const int r1 = p.addAction("RESET", [&clicks]() { ++clicks; });
        assert(r0 == 0 && r1 == 1);
        assert(p.rowCount() == 2);
        assert(p.rowText(0) == "SPEED 1");
        assert(p.rowText(1) == "RESET");
        assert(math::approxEqual(p.panelSize().y, 110.0f, 1e-5f));

        auto panel = p.build();
        assert(panel->children().size() == 2);
        ui::UIButton* b0 = p.rowButton(0);
        assert(b0 != nullptr);
        assert(math::approxEqual(b0->position().y, 20.0f, 1e-5f));
        assert(math::approxEqual(p.rowButton(1)->position().y, 80.0f, 1e-5f));

        assert(b0->handleClick());
        assert(value == 2);
        assert(p.rowText(0) == "SPEED 2");

        p.setRowEnabled(1, false);
        assert(!p.rowEnabled(1));
        assert(!p.rowButton(1)->handleClick());
        assert(clicks == 0);
        p.setRowEnabled(1, true);
        assert(p.rowButton(1)->handleClick());
        assert(clicks == 1);

        p.setVisible(false);
        assert(!panel->visible());
        assert(!p.visible());
    }

    {
        DebugPanel p(math::Vec2(0, 0), math::Vec2(100, 40));
        p.setRowEnabled(p.addAction("A", nullptr), false);
        p.setVisible(false);
        auto panel = p.build();
        assert(!panel->visible());
        assert(!p.rowButton(0)->enabled());
        assert(!p.rowButton(0)->handleClick());
    }

    aout << "[debug-panel] self-test PASS" << std::endl;
}

} // namespace game
