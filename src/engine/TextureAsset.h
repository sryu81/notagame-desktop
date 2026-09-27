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
    loadAsset(const Platform &platform, const std::string &assetPath);

    ~TextureAsset();

    /*!
     * @return the texture id for use with OpenGL
     */
    constexpr GLuint getTextureID() const { return textureID_; }

private:
    inline TextureAsset(GLuint textureId) : textureID_(textureId) {}

    GLuint textureID_;
};

#endif //NOT_A_GAME_TEXTUREASSET_H