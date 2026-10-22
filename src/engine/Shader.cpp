#include "Shader.h"

#include "Platform.h"
#include <vector>
#include <cstring>

#include "AndroidOut.h"
#include "Utility.h"

static_assert(Shader::clampBoneCount(100) == Shader::kMaxBones, "bone clamp");
static_assert(Shader::clampBoneCount(-1) == 0, "bone clamp");
static_assert(Shader::clampBoneCount(20) == 20, "bone clamp");

Shader *Shader::loadShader(
        const std::string &vertexSource,
        const std::string &fragmentSource,
        const ShaderBinding &binding,
        bool requireModelAndColor) {
    Shader *shader = nullptr;

    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertexSource);
    if (!vertexShader) {
        return nullptr;
    }

    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!fragmentShader) {
        glDeleteShader(vertexShader);
        return nullptr;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);

        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint logLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength) {
                GLchar *log = new GLchar[logLength];
                glGetProgramInfoLog(program, logLength, nullptr, log);
                aout << "Failed to link program with:\n" << log << std::endl;
                delete[] log;
            }
            glDeleteProgram(program);
        } else {
            const GLint model      = glGetUniformLocation(program, binding.modelMatrixUniform);
            const GLint color      = glGetUniformLocation(program, binding.colorUniform);
            const GLint texture    = glGetUniformLocation(program, binding.textureUniform);
            const GLint hasTexture = glGetUniformLocation(program, binding.hasTextureUniform);
            const GLint boneMat    = glGetUniformLocation(program, binding.boneMatricesUniform);
            const GLint shadowMap  = glGetUniformLocation(program, binding.shadowMapUniform);
            const GLint lightVP    = glGetUniformLocation(program, binding.lightVPUniform);
            const GLint uvRect     = glGetUniformLocation(program, binding.uvRectUniform);

            if (requireModelAndColor && (model == -1 || color == -1)) {
                aout << "Shader missing required uniforms (uModel or uColor)" << std::endl;
                glDeleteProgram(program);
            } else {
                shader = new Shader(program, model, color, texture, hasTexture, boneMat,
                                    shadowMap, lightVP, uvRect);
            }
        }
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shader;
}

GLuint Shader::loadShader(GLenum shaderType, const std::string &shaderSource) {
    Utility::assertGlError();
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        auto *shaderRawString = (GLchar *) shaderSource.c_str();
        GLint shaderLength = shaderSource.length();
        glShaderSource(shader, 1, &shaderRawString, &shaderLength);
        glCompileShader(shader);

        GLint shaderCompiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &shaderCompiled);

        if (!shaderCompiled) {
            GLint infoLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLength);
            if (infoLength) {
                auto *infoLog = new GLchar[infoLength];
                glGetShaderInfoLog(shader, infoLength, nullptr, infoLog);
                aout << "Failed to compile with:\n" << infoLog << std::endl;
                delete[] infoLog;
            }
            glDeleteShader(shader);
            shader = 0;
        }
    }
    return shader;
}

void Shader::activate() const {
    glUseProgram(program_);
}

void Shader::deactivate() const {
    glUseProgram(0);
}

void Shader::bindUniformBlock(const char *blockName, GLuint bindingPoint) const {
    const GLuint idx = glGetUniformBlockIndex(program_, blockName);
    if (idx != GL_INVALID_INDEX) glUniformBlockBinding(program_, idx, bindingPoint);
}

void Shader::setModelMatrix(const float *m) const {
    glUniformMatrix4fv(modelMatrix_, 1, false, m);
}

void Shader::setColor(float r, float g, float b, float a) const {
    glUniform4f(color_, r, g, b, a);
}

void Shader::setTexture(GLint unit) const {
    if (texture_ != -1) glUniform1i(texture_, unit);
}

void Shader::setHasTexture(bool has) const {
    if (hasTexture_ != -1) glUniform1i(hasTexture_, has ? 1 : 0);
}

void Shader::setBoneMatrices(const float *m, int count) const {
    count = clampBoneCount(count);
    if (boneMatrices_ != -1 && count > 0) {
        glUniformMatrix4fv(boneMatrices_, count, false, m);
    }
}

void Shader::setLightVP(const float *m) const {
    if (lightVP_ != -1) glUniformMatrix4fv(lightVP_, 1, false, m);
}

void Shader::setShadowMap(GLint unit) const {
    if (shadowMap_ != -1) glUniform1i(shadowMap_, unit);
}

void Shader::setUVRect(float u0, float v0, float u1, float v1) const {
    if (uvRect_ != -1) glUniform4f(uvRect_, u0, v0, u1, v1);
}

void Shader::setFloat(const char *name, float value) const {
    if (!name) return;
    const GLint loc = glGetUniformLocation(program_, name);
    if (loc != -1) glUniform1f(loc, value);
}

void Shader::setFloat3(const char *name, float x, float y, float z) const {
    if (!name) return;
    const GLint loc = glGetUniformLocation(program_, name);
    if (loc != -1) glUniform3f(loc, x, y, z);
}

namespace shader {

std::string loadSource(const Platform& platform, const char* path) {
    std::vector<uint8_t> bytes;
    if (!platform.readAsset(path, bytes)) {
        aout << "[shader] failed to open " << path << std::endl;
        return {};
    }
    return std::string(bytes.begin(), bytes.end());
}

} // namespace shader
