#include "LabChecklist.h"

#include <cassert>

#include "BitmapFont.h"
#include "Engine.h"

namespace game {

const math::Vec4 LabChecklist::kPendingColor{0.85f, 0.35f, 0.30f, 1.0f};
const math::Vec4 LabChecklist::kPassColor{0.35f, 0.90f, 0.40f, 1.0f};

LabChecklist::LabChecklist(math::Vec2 origin, int textScale)
        : origin_(origin), textScale_(textScale) {}

LabChecklist::Row* LabChecklist::find(const std::string& name) {
    for (auto& r : rows_) if (r.name == name) return &r;
    return nullptr;
}

const LabChecklist::Row* LabChecklist::find(const std::string& name) const {
    for (const auto& r : rows_) if (r.name == name) return &r;
    return nullptr;
}

void LabChecklist::add(const std::string& name) {
    if (!find(name)) rows_.push_back({name});
}

void LabChecklist::applyColor(Row& r) {
    if (!r.text) return;
    r.text->setColor(r.passed ? kPassColor : kPendingColor);
    r.text->setText((r.passed ? "+ " : "- ") + r.name);
}

void LabChecklist::check(const std::string& name, bool condition) {
    Row* r = find(name);
    if (!r) {
        add(name);
        r = &rows_.back();
    }
    if (!condition || r->passed) return;
    r->passed = true;
    applyColor(*r);
}

void LabChecklist::reset() {
    for (auto& r : rows_) {
        r.passed = false;
        applyColor(r);
    }
}

bool LabChecklist::passed(const std::string& name) const {
    const Row* r = find(name);
    return r && r->passed;
}

size_t LabChecklist::passedCount() const {
    size_t n = 0;
    for (const auto& r : rows_) if (r.passed) ++n;
    return n;
}

bool LabChecklist::allPassed() const {
    return !rows_.empty() && passedCount() == rows_.size();
}

std::unique_ptr<ui::UIPanel> LabChecklist::build() {
    const float lineH = static_cast<float>((ui::BitmapFont::glyphHeight() + 3) * textScale_);
    auto panel = std::make_unique<ui::UIPanel>(
        origin_, math::Vec2(0, lineH * static_cast<float>(rows_.size())),
        math::Vec4(0, 0, 0, 0));
    for (size_t i = 0; i < rows_.size(); ++i) {
        const math::Vec2 pos(origin_.x, origin_.y + lineH * static_cast<float>(i));
        auto text = std::make_unique<ui::UIText>(pos, "", kPendingColor, textScale_);
        rows_[i].text = text.get();
        applyColor(rows_[i]);
        panel->addChild(std::move(text));
    }
    return panel;
}

void LabChecklist::selfTest() {
    aout << "[lab-checklist] self-test starting" << std::endl;

    {
        LabChecklist c(math::Vec2(0, 0));
        assert(c.size() == 0);
        assert(!c.allPassed());
        c.add("a");
        c.add("b");
        c.add("a");
        assert(c.size() == 2);

        auto panel = c.build();
        assert(panel->children().size() == 2);
        const auto* ta = static_cast<const ui::UIText*>(panel->children()[0].get());
        assert(ta->text() == "- a");

        c.check("a", false);
        assert(!c.passed("a"));
        c.check("a", true);
        assert(c.passed("a"));
        assert(ta->text() == "+ a");
        c.check("a", false);
        assert(c.passed("a"));
        assert(c.passedCount() == 1);
        assert(!c.allPassed());

        c.check("b", true);
        assert(c.allPassed());

        c.reset();
        assert(c.passedCount() == 0);
        assert(ta->text() == "- a");
    }

    {
        LabChecklist c(math::Vec2(0, 0));
        c.check("late", true);
        assert(c.size() == 1);
        assert(c.passed("late"));
        assert(!c.passed("missing"));
    }

    aout << "[lab-checklist] self-test PASS" << std::endl;
}

} // namespace game
