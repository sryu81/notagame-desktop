#include "UIRoot.h"

#include <cassert>
#include <cstring>

#include <GLES3/gl3.h>

#include "AndroidOut.h"
#include "BitmapFont.h"

namespace ui {

static const char *kUiVertex = R"vertex(#version 300 es
in vec2 inPos;
in vec2 inUV;
uniform vec2 uScreen;
uniform vec4 uRect;
uniform vec4 uUV;
out vec2 fragUV;
void main() {
    vec2 worldPos = uRect.xy + inPos * uRect.zw;
    vec2 ndc = vec2(worldPos.x / uScreen.x * 2.0 - 1.0,
                    1.0 - worldPos.y / uScreen.y * 2.0);
    fragUV = uUV.xy + inUV * (uUV.zw - uUV.xy);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)vertex";

static const char *kUiFragment = R"fragment(#version 300 es
precision mediump float;
in vec2 fragUV;
uniform sampler2D uTexture;
uniform vec4 uColor;
uniform float uUseTexture;
out vec4 outColor;
void main() {
    float a = mix(1.0, texture(uTexture, fragUV).r, uUseTexture);
    if (uColor.a < 0.001 && a < 0.001) discard;
    outColor = vec4(uColor.rgb, uColor.a * a);
}
)fragment";

UIRoot::UIRoot() = default;

UIRoot::~UIRoot() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    if (quadVbo_ != 0) {
        glDeleteBuffers(1, &quadVbo_);
        quadVbo_ = 0;
    }
    if (quadIbo_ != 0) {
        glDeleteBuffers(1, &quadIbo_);
        quadIbo_ = 0;
    }
    if (fontTexture_ != 0) {
        glDeleteTextures(1, &fontTexture_);
        fontTexture_ = 0;
    }
}

static uint32_t compileShader(uint32_t type, const char *src) {
    uint32_t s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int status = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        aout << "UI shader compile failed: " << log << std::endl;
        glDeleteShader(s);
        return 0;
    }
    return s;
}

void UIRoot::init() {
    uint32_t vs = compileShader(GL_VERTEX_SHADER, kUiVertex);
    assert(vs != 0);
    uint32_t fs = compileShader(GL_FRAGMENT_SHADER, kUiFragment);
    assert(fs != 0);
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glBindAttribLocation(program_, 0, "inPos");
    glBindAttribLocation(program_, 1, "inUV");
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    int linked = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    assert(linked != 0);
    uScreenLoc_ = glGetUniformLocation(program_, "uScreen");
    uRectLoc_ = glGetUniformLocation(program_, "uRect");
    uUVLoc_ = glGetUniformLocation(program_, "uUV");
    uColorLoc_ = glGetUniformLocation(program_, "uColor");
    uTextureLoc_ = glGetUniformLocation(program_, "uTexture");
    uUseTextureLoc_ = glGetUniformLocation(program_, "uUseTexture");

    constexpr float verts[] = {
        0.0f, 0.0f,  0.0f, 0.0f,
        1.0f, 0.0f,  1.0f, 0.0f,
        1.0f, 1.0f,  1.0f, 1.0f,
        0.0f, 1.0f,  0.0f, 1.0f,
    };
    constexpr uint16_t idx[] = {0, 1, 2, 0, 2, 3};
    glGenBuffers(1, &quadVbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glGenBuffers(1, &quadIbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIbo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    fontTexture_ = BitmapFont::createTexture();
}

void UIRoot::setScreenSize(int w, int h) {
    width_ = w;
    height_ = h;
}

void UIRoot::drawQuad(float x, float y, float w, float h,
                      float u0, float v0, float u1, float v1,
                      const math::Vec4 &color) {
    if (program_ == 0 || width_ == 0 || height_ == 0) return;
    glUseProgram(program_);
    glUniform2f(uScreenLoc_, static_cast<float>(width_), static_cast<float>(height_));
    glUniform4f(uRectLoc_, x, y, w, h);
    glUniform4f(uUVLoc_, u0, v0, u1, v1);
    glUniform4f(uColorLoc_, color.x, color.y, color.z, color.w);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTexture_);
    glUniform1i(uTextureLoc_, 0);
    glUniform1f(uUseTextureLoc_, 1.0f);

    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIbo_);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<const void *>(0));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<const void *>(2 * sizeof(float)));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void UIRoot::drawSolid(float x, float y, float w, float h, const math::Vec4 &color) {
    if (program_ == 0 || width_ == 0 || height_ == 0) return;
    glUseProgram(program_);
    glUniform2f(uScreenLoc_, static_cast<float>(width_), static_cast<float>(height_));
    glUniform4f(uRectLoc_, x, y, w, h);
    glUniform4f(uUVLoc_, 0.0f, 0.0f, 1.0f, 1.0f);
    glUniform4f(uColorLoc_, color.x, color.y, color.z, color.w);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTexture_);
    glUniform1i(uTextureLoc_, 0);
    glUniform1f(uUseTextureLoc_, 0.0f);

    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIbo_);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<const void *>(0));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<const void *>(2 * sizeof(float)));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void UIRoot::render() {
    if (root_ == nullptr || width_ == 0 || height_ == 0) return;
    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthWasEnabled) glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    root_->render(*this);
    if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

namespace {

UIButton *findButtonAt(UIElement *elem, const math::Vec2 &point) {
    if (elem == nullptr || !elem->visible()) return nullptr;
    for (auto it = elem->children().rbegin(); it != elem->children().rend(); ++it) {
        if (!*it) continue;
        if (UIButton *b = findButtonAt(it->get(), point)) return b;
    }
    if (auto *btn = dynamic_cast<UIButton *>(elem)) {
        if (btn->hitTest(point)) return btn;
    }
    return nullptr;
}

} // namespace

UIButton *UIRoot::hitTestButton(const math::Vec2 &point) const {
    if (!root_) return nullptr;
    return findButtonAt(root_.get(), point);
}

void UIRoot::selfTest() {
    aout << "[ui] self-test starting" << std::endl;
    BitmapFont::selfTest();

    {
        UIRoot ui;
        assert(ui.root() == nullptr);
        assert(ui.hitTestButton(math::Vec2(10, 10)) == nullptr);
    }
    {
        UIRoot ui;
        auto root = std::make_unique<UIPanel>(math::Vec2(0, 0), math::Vec2(800, 600),
                                              math::Vec4(0, 0, 0, 0));
        auto btn = std::make_unique<UIButton>(math::Vec2(10, 10), math::Vec2(100, 50),
                                              "OK", [](){});
        UIButton *btnPtr = btn.get();
        root->addChild(std::move(btn));
        ui.setRoot(std::move(root));
        assert(ui.hitTestButton(math::Vec2(50, 30)) == btnPtr);
        assert(ui.hitTestButton(math::Vec2(5, 5)) == nullptr);
        assert(ui.hitTestButton(math::Vec2(200, 30)) == nullptr);
    }
    {
        UIRoot ui;
        auto root = std::make_unique<UIPanel>(math::Vec2(0, 0), math::Vec2(800, 600),
                                              math::Vec4(0, 0, 0, 0));
        auto a = std::make_unique<UIButton>(math::Vec2(10, 10), math::Vec2(50, 50), "A", [](){});
        auto b = std::make_unique<UIButton>(math::Vec2(20, 20), math::Vec2(50, 50), "B", [](){});
        UIButton *aPtr = a.get();
        UIButton *bPtr = b.get();
        root->addChild(std::move(a));
        root->addChild(std::move(b));
        ui.setRoot(std::move(root));
        assert(ui.hitTestButton(math::Vec2(50, 50)) == bPtr);
        assert(ui.hitTestButton(math::Vec2(15, 15)) == aPtr);
    }
    {
        UIText t(math::Vec2(5, 5), "HELLO", math::Vec4(1, 1, 1, 1), 2);
        assert(t.text() == "HELLO");
        assert(t.size().x > 0);
        assert(t.size().y == 7 * 2);
    }
    aout << "[ui] self-test PASS" << std::endl;
}

} // namespace ui
