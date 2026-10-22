#include "TextureAsset.h"
#include "AndroidOut.h"
#include "Platform.h"
#include "Utility.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"

std::shared_ptr<TextureAsset>
TextureAsset::loadAsset(const Platform &platform, const std::string &assetPath, bool nearest) {
    std::vector<uint8_t> bytes;
    if (!platform.readAsset(assetPath, bytes)) {
        aout << "[texture] failed to open " << assetPath << std::endl;
        return nullptr;
    }

    int width = 0, height = 0, channels = 0;
    stbi_uc *pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                            &width, &height, &channels, 4);
    if (!pixels) {
        aout << "[texture] decode failed " << assetPath << ": " << stbi_failure_reason() << std::endl;
        return nullptr;
    }

    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, nearest ? GL_NEAREST : GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, nearest ? GL_NEAREST : GL_LINEAR);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    if (!nearest) glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(pixels);
    return std::shared_ptr<TextureAsset>(new TextureAsset(textureId, width, height, nearest));
}

TextureAsset::~TextureAsset() {
    glDeleteTextures(1, &textureID_);
    textureID_ = 0;
}
