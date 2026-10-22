#ifndef NOT_A_GAME_TEXTUREASSET_H
#define NOT_A_GAME_TEXTUREASSET_H

#include <memory>
#include <GLES3/gl3.h>
#include <string>
#include <vector>

class Platform;

class TextureAsset {
public:
    static std::shared_ptr<TextureAsset>
    loadAsset(const Platform &platform, const std::string &assetPath, bool nearest = false);

    ~TextureAsset();

    /*!
     * @return the texture id for use with OpenGL
     */
    constexpr GLuint getTextureID() const { return textureID_; }
    int width() const { return width_; }
    int height() const { return height_; }
    bool nearest() const { return nearest_; }

private:
    inline TextureAsset(GLuint textureId, int w, int h, bool nearest)
            : textureID_(textureId), width_(w), height_(h), nearest_(nearest) {}

    GLuint textureID_;
    int width_;
    int height_;
    bool nearest_;
};

#endif //NOT_A_GAME_TEXTUREASSET_H