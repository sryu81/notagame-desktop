#ifndef NOT_A_GAME_DEBUG_PANEL_H
#define NOT_A_GAME_DEBUG_PANEL_H

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Math.h"
#include "UIElement.h"

namespace game {

class DebugPanel {
public:
    using Getter = std::function<std::string()>;
    using Action = std::function<void()>;

    DebugPanel(math::Vec2 origin, math::Vec2 rowSize, float gap = 8.0f, int textScale = 3);

    int addRow(std::string label, Getter value, Action onClick);
    int addAction(std::string label, Action onClick);

    void setOrigin(math::Vec2 origin) { origin_ = origin; }
    std::unique_ptr<ui::UIPanel> build();
    void refresh();

    void setVisible(bool v);
    bool visible() const { return visible_; }
    void setRowEnabled(int row, bool enabled);
    bool rowEnabled(int row) const;

    size_t rowCount() const { return rows_.size(); }
    std::string rowText(int row) const;
    ui::UIButton* rowButton(int row) const;
    math::Vec2 panelSize() const;

    static void selfTest();

private:
    struct Row {
        std::string label;
        Getter value;
        Action onClick;
        bool enabled = true;
        ui::UIButton* button = nullptr;
    };

    math::Vec2 origin_;
    math::Vec2 rowSize_;
    float gap_;
    int textScale_;
    bool visible_ = true;
    std::vector<Row> rows_;
    ui::UIPanel* panel_ = nullptr;
};

} // namespace game

#endif
