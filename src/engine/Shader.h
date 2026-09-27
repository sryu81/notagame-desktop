#ifndef NOT_A_GAME_SHADER_H
#define NOT_A_GAME_SHADER_H

#include <string>
#include <GLES3/gl3.h>

struct AAssetManager;

struct ShaderBinding {
    const char *modelMatrixUniform    = "uModel";
    const char *colorUniform          = "uColor";
    const char *textureUniform        = "uTexture";
    const char *hasTextureUniform     = "uHasTexture";
    const char *boneMatricesUniform   = "uBoneMatrices";
    const char *shadowMapUniform      = "uShadowMap";
    const char *lightVPUniform        = "uLightVP";
};

class Shader {
public:
    static constexpr GLint kPositionAttrib    = 0;
    static constexpr GLint kUVAttrib          = 1;
    static constexpr GLint kNormalAttrib      = 2;
    static constexpr GLint kBoneIndicesAttrib = 3;
    static constexpr GLint kBoneWeightsAttrib = 4;
    static constexpr int kMaxBones = 48;

    static Shader *loadShader(
            const std::string &vertexSource,
            const std::string &fragmentSource,
            const ShaderBinding &binding = {},
            bool requireModelAndColor = true);

    inline ~Shader() {
        if (program_) {
            glDeleteProgram(program_);
            program_ = 0;
        }
    }

    void activate() const;
    void deactivate() const;

    void bindUniformBlock(const char *blockName, GLuint bindingPoint) const;

    void setModelMatrix(const float *m) const;
    void setColor(float r, float g, float b, float a) const;
    void setTexture(GLint unit) const;
    void setHasTexture(bool has) const;
    void setBoneMatrices(const float *m, int count) const;
    void setLightVP(const float *m) const;
    void setShadowMap(GLint unit) const;

    void setFloat(const char *name, float value) const;
    void setFloat3(const char *name, float x, float y, float z) const;

    GLint positionAttrib()    const { return kPositionAttrib; }
    GLint uvAttrib()          const { return kUVAttrib; }
    GLint normalAttrib()      const { return kNormalAttrib; }
    GLint boneIndicesAttrib() const { return kBoneIndicesAttrib; }
    GLint boneWeightsAttrib() const { return kBoneWeightsAttrib; }

private:
    static GLuint loadShader(GLenum shaderType, const std::string &shaderSource);

    constexpr Shader(GLuint program, GLint model, GLint color, GLint texture,
                     GLint hasTexture, GLint boneMatrices,
                     GLint shadowMap, GLint lightVP)
            : program_(program),
              modelMatrix_(model),
              color_(color),
              texture_(texture),
              hasTexture_(hasTexture),
              boneMatrices_(boneMatrices),
              shadowMap_(shadowMap),
              lightVP_(lightVP) {}

    GLuint program_;
    GLint modelMatrix_;
    GLint color_;
    GLint texture_;
    GLint hasTexture_;
    GLint boneMatrices_;
    GLint shadowMap_;
    GLint lightVP_;
};

namespace shader {

std::string loadSource(AAssetManager* mgr, const char* path);

} // namespace shader

#endif //NOT_A_GAME_SHADER_H
