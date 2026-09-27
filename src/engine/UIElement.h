#ifndef NOT_A_GAME_UI_H
#define NOT_A_GAME_UI_H

#include <functional>
#include <deque>
#include <memory>
#include <string>

#include "Math.h"

namespace ui {

class UIRoot;

class UIElement {
public:
    UIElement() = default;
    explicit UIElement(math::Vec2 position, math::Vec2 size);
    virtual ~UIElement() = default;

    UIElement(const UIElement &) = delete;
    UIElement &operator=(const UIElement &) = delete;

    void addChild(std::unique_ptr<UIElement> child);
    void clearChildren() { children_.clear(); }

    const math::Vec2 &position() const { return position_; }
    const math::Vec2 &size() const { return size_; }
    void setPosition(const math::Vec2 &p) { position_ = p; }
    void setSize(const math::Vec2 &s) { size_ = s; }
    void setVisible(bool v) { visible_ = v; }
    bool visible() const { return visible_; }

    const std::deque<std::unique_ptr<UIElement>> &children() const { return children_; }

    virtual void render(UIRoot &root) const = 0;
    virtual bool hitTest(const math::Vec2 &point) const;

protected:
    math::Vec2 position_{0.0f, 0.0f};
    math::Vec2 size_{0.0f, 0.0f};
    bool visible_ = true;
    std::deque<std::unique_ptr<UIElement>> children_;
};

class UIPanel : public UIElement {
public:
    UIPanel(math::Vec2 position, math::Vec2 size, math::Vec4 color);
    void render(UIRoot &root) const override;

    void setColor(const math::Vec4 &c) { color_ = c; }
    const math::Vec4 &color() const { return color_; }

private:
    math::Vec4 color_;
};

class UIText : public UIElement {
public:
    UIText(math::Vec2 position, std::string text, math::Vec4 color, int scale = 2);
    void render(UIRoot &root) const override;

    void setText(std::string t) { text_ = std::move(t); }
    const std::string &text() const { return text_; }
    void setColor(const math::Vec4 &c) { color_ = c; }
    void setScale(int s) { scale_ = s; }

private:
    std::string text_;
    math::Vec4 color_;
    int scale_;
};

class UIButton : public UIElement {
public:
    using OnClick = std::function<void()>;
    UIButton(math::Vec2 position, math::Vec2 size, std::string text, OnClick onClick);

    void render(UIRoot &root) const override;
    bool handleClick();

    void setText(std::string t) { text_ = std::move(t); }
    void setEnabled(bool e) { enabled_ = e; }
    bool enabled() const { return enabled_; }
    void setTextScale(int s) { textScale_ = s; }
    int textScale() const { return textScale_; }

private:
    std::string text_;
    OnClick onClick_;
    int textScale_ = 4;
    bool enabled_ = true;
    math::Vec4 normalColor_{0.20f, 0.20f, 0.25f, 0.85f};
    math::Vec4 pressedColor_{0.40f, 0.40f, 0.50f, 0.95f};
    math::Vec4 disabledColor_{0.10f, 0.10f, 0.12f, 0.5f};
};

} // namespace ui

#endif
