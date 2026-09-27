#ifndef NOT_A_GAME_LAB_CHECKLIST_H
#define NOT_A_GAME_LAB_CHECKLIST_H

#include <memory>
#include <string>
#include <vector>

#include "Math.h"
#include "UIElement.h"

namespace game {

class LabChecklist {
public:
    explicit LabChecklist(math::Vec2 origin, int textScale = 2);

    void add(const std::string& name);
    void check(const std::string& name, bool condition);
    void reset();

    bool passed(const std::string& name) const;
    bool allPassed() const;
    size_t size() const { return rows_.size(); }
    size_t passedCount() const;

    std::unique_ptr<ui::UIPanel> build();

    static const math::Vec4 kPendingColor;
    static const math::Vec4 kPassColor;

    static void selfTest();

private:
    struct Row {
        std::string name;
        bool passed = false;
        ui::UIText* text = nullptr;
    };

    Row* find(const std::string& name);
    const Row* find(const std::string& name) const;
    void applyColor(Row& r);

    math::Vec2 origin_;
    int textScale_;
    std::vector<Row> rows_;
};

} // namespace game

#endif
