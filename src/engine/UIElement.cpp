#include "UIElement.h"

#include <cassert>

#include "AndroidOut.h"
#include "BitmapFont.h"
#include "UIRoot.h"

namespace ui {

UIElement::UIElement(math::Vec2 position, math::Vec2 size) :
        position_(position), size_(size) {}

void UIElement::addChild(std::unique_ptr<UIElement> child) {
    children_.push_back(std::move(child));
}

bool UIElement::hitTest(const math::Vec2 &point) const {
    if (!visible_) return false;
    if (point.x < position_.x || point.x > position_.x + size_.x) return false;
    if (point.y < position_.y || point.y > position_.y + size_.y) return false;
    return true;
}

UIPanel::UIPanel(math::Vec2 position, math::Vec2 size, math::Vec4 color) :
        UIElement(position, size), color_(color) {}

void UIPanel::render(UIRoot &root) const {
    if (!visible_) return;
    root.drawSolid(position_.x, position_.y, size_.x, size_.y, color_);
    for (const auto &child : children_) child->render(root);
}

UIText::UIText(math::Vec2 position, std::string text, math::Vec4 color, int scale) :
        UIElement(position, math::Vec2(0, 0)), text_(std::move(text)), color_(color), scale_(scale) {
    const int w = BitmapFont::stringWidth(text_) * scale_;
    const int h = BitmapFont::glyphHeight() * scale_;
    size_ = math::Vec2(static_cast<float>(w), static_cast<float>(h));
}

void UIText::render(UIRoot &root) const {
    if (!visible_) return;
    const float gw = static_cast<float>(BitmapFont::glyphWidth());
    const float gh = static_cast<float>(BitmapFont::glyphHeight());
    const float advance = (gw + 1.0f) * static_cast<float>(scale_);
    const float quadW = gw * static_cast<float>(scale_);
    const float quadH = gh * static_cast<float>(scale_);
    float x = position_.x;
    const float y = position_.y;
    for (const char c : text_) {
        float u0, v0, u1, v1;
        BitmapFont::glyphUv(static_cast<unsigned char>(c), u0, v0, u1, v1);
        root.drawQuad(x, y, quadW, quadH, u0, v0, u1, v1, color_);
        x += advance;
    }
    for (const auto &child : children_) child->render(root);
}

UIButton::UIButton(math::Vec2 position, math::Vec2 size, std::string text, OnClick onClick) :
        UIElement(position, size), text_(std::move(text)), onClick_(std::move(onClick)) {}

void UIButton::render(UIRoot &root) const {
    if (!visible_) return;
    const math::Vec4 bg = enabled_ ? normalColor_ : disabledColor_;
    root.drawSolid(position_.x, position_.y, size_.x, size_.y, bg);
    if (!text_.empty()) {
        const float gw = static_cast<float>(BitmapFont::glyphWidth());
        const float gh = static_cast<float>(BitmapFont::glyphHeight());
        const int scale = textScale_;
        const float advance = (gw + 1.0f) * static_cast<float>(scale);
        const float quadW = gw * static_cast<float>(scale);
        const float quadH = gh * static_cast<float>(scale);
        const float totalW = static_cast<float>(BitmapFont::stringWidth(text_)) * static_cast<float>(scale);
        const float ox = position_.x + (size_.x - totalW) * 0.5f;
        const float oy = position_.y + (size_.y - quadH) * 0.5f;
        float x = ox;
        const math::Vec4 textColor = enabled_
                ? math::Vec4(0.95f, 0.95f, 0.95f, 1.0f)
                : math::Vec4(0.55f, 0.55f, 0.55f, 1.0f);
        for (const char c : text_) {
            float u0, v0, u1, v1;
            BitmapFont::glyphUv(static_cast<unsigned char>(c), u0, v0, u1, v1);
            root.drawQuad(x, oy, quadW, quadH, u0, v0, u1, v1, textColor);
            x += advance;
        }
    }
    for (const auto &child : children_) child->render(root);
}

bool UIButton::handleClick() {
    if (!enabled_ || !visible_ || !onClick_) return false;
    onClick_();
    return true;
}

} // namespace ui
