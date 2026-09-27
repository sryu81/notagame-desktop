#include "BitmapFont.h"

#include <cassert>
#include <cstring>

#include <GLES3/gl3.h>

#include "AndroidOut.h"
#include "FontData.h"

namespace ui {

uint32_t BitmapFont::createTexture() {
    constexpr int kPadCols = 1;
    constexpr int kPadRows = 1;
    uint8_t atlas[kAtlasHeight][kAtlasWidth];
    std::memset(atlas, 0, sizeof(atlas));
    for (int i = 0; i < kGlyphCount; ++i) {
        const int col = i % kAtlasCols;
        const int row = i / kAtlasCols;
        const int ox = col * 5;
        const int oy = row * 7;
        for (int r = 0; r < 7; ++r) {
            const uint8_t bits = kFontData[i][r];
            for (int c = 0; c < 5; ++c) {
                const int px = ox + c;
                const int py = oy + r;
                if (px < 0 || px >= kAtlasWidth || py < 0 || py >= kAtlasHeight) continue;
                atlas[py][px] = ((bits >> (4 - c)) & 1u) ? 255u : 0u;
            }
        }
    }
    (void)kPadCols; (void)kPadRows;  // reserved for future padding
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, kAtlasWidth, kAtlasHeight, 0,
                 GL_RED, GL_UNSIGNED_BYTE, atlas);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void BitmapFont::glyphUv(int ascii, float &u0, float &v0, float &u1, float &v1) {
    int idx = ascii - kFirstAscii;
    if (idx < 0 || idx >= kGlyphCount) idx = 0;
    const int col = idx % kAtlasCols;
    const int row = idx / kAtlasCols;
    const float gw = static_cast<float>(glyphWidth());
    const float gh = static_cast<float>(glyphHeight());
    const float aw = static_cast<float>(kAtlasWidth);
    const float ah = static_cast<float>(kAtlasHeight);
    u0 = (col * gw) / aw;
    v0 = (row * gh) / ah;
    u1 = ((col + 1) * gw) / aw;
    v1 = ((row + 1) * gh) / ah;
}

int BitmapFont::stringWidth(const std::string &s) {
    if (s.empty()) return 0;
    return static_cast<int>(s.size()) * (glyphWidth() + 1) - 1;
}

void BitmapFont::selfTest() {
    aout << "[font] self-test starting" << std::endl;
    {
        float u0, v0, u1, v1;
        BitmapFont::glyphUv('A', u0, v0, u1, v1);
        assert(u1 > u0);
        assert(v1 > v0);
        BitmapFont::glyphUv(127, u0, v0, u1, v1);
        BitmapFont::glyphUv(31, u0, v0, u1, v1);
        assert(BitmapFont::stringWidth("HELLO") == 5 * 6 - 1);
        assert(BitmapFont::stringWidth("") == 0);
        assert(BitmapFont::stringWidth("A") == 5);
    }
    aout << "[font] self-test PASS" << std::endl;
}

} // namespace ui
