#ifndef NOT_A_GAME_BITMAP_FONT_H
#define NOT_A_GAME_BITMAP_FONT_H

#include <cstdint>
#include <string>

#include "Math.h"

namespace ui {

class BitmapFont {
public:
    static constexpr int kAtlasCols = 16;
    static constexpr int kAtlasRows = 6;
    static constexpr int kAtlasWidth = kAtlasCols * 5;
    static constexpr int kAtlasHeight = kAtlasRows * 7;
    static constexpr int kFirstAscii = 32;
    static constexpr int kGlyphCount = 96;

    /*!
     * Build the GPU-side font texture from FontData.h. Must be called after
     * a GL context is current. The returned handle is owned by the caller.
     */
    static uint32_t createTexture();

    /*!
     * Returns the atlas UV range (u0, v0, u1, v1) for the glyph at `ascii`.
     * Out-of-range codes return the space-glyph range.
     */
    static void glyphUv(int ascii, float &u0, float &v0, float &u1, float &v1);

    /*!
     * Returns the width and height (in atlas pixels) of a single glyph cell.
     */
    static int glyphWidth() { return 5; }
    static int glyphHeight() { return 7; }

    /*!
     * Returns the advance width (in pixels) when rendering a string of `len`
     * characters. Uses 1px tracking between glyphs.
     */
    static int stringWidth(const std::string &s);

    static void selfTest();
};

} // namespace ui

#endif
