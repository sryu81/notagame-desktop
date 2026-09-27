#ifndef NOT_A_GAME_UIROOT_H
#define NOT_A_GAME_UIROOT_H

#include <cstdint>
#include <memory>

#include "Math.h"
#include "UIElement.h"

namespace ui {

class UIRoot {
public:
    UIRoot();
    ~UIRoot();

    UIRoot(const UIRoot &) = delete;
    UIRoot &operator=(const UIRoot &) = delete;

    /*!
     * Allocate the GL-side resources (font texture, shared quad VBO/IBO,
     * UI shader). Must be called once after a GL context is current.
     */
    void init();

    void setScreenSize(int w, int h);
    int width() const { return width_; }
    int height() const { return height_; }

    void setRoot(std::unique_ptr<UIElement> root) { root_ = std::move(root); }
    UIElement *root() const { return root_.get(); }

    /*!
     * Issue a textured-quad draw at screen position (x, y) with the given
     * size and UV range, tinted by `color`. Used by UIElement subclasses.
     */
    void drawQuad(float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  const math::Vec4 &color);

    /*!
     * Issue a flat-colored quad (no texture sampled) at screen position
     * (x, y) with the given size, tinted by `color`.
     */
    void drawSolid(float x, float y, float w, float h, const math::Vec4 &color);

    /*!
     * Walk the tree and issue one draw call per visible element. Disables
     * depth test (UI is on top of the 3D pass).
     */
    void render();

    /*!
     * Return the deepest UIButton whose AABB contains `point`, or nullptr.
     * Walks back-to-front (last child wins).
     */
    UIButton *hitTestButton(const math::Vec2 &point) const;

    static void selfTest();

private:
    int width_ = 0;
    int height_ = 0;
    uint32_t program_ = 0;
    int32_t uScreenLoc_ = -1;
    int32_t uRectLoc_ = -1;
    int32_t uUVLoc_ = -1;
    int32_t uColorLoc_ = -1;
    int32_t uTextureLoc_ = -1;
    int32_t uUseTextureLoc_ = -1;
    uint32_t quadVbo_ = 0;
    uint32_t quadIbo_ = 0;
    uint32_t fontTexture_ = 0;
    std::unique_ptr<UIElement> root_;
};

} // namespace ui

#endif
