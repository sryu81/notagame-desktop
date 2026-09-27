#ifndef NOT_A_GAME_MODEL_H
#define NOT_A_GAME_MODEL_H

#include <GLES3/gl3.h>

#include "Math.h"
#include "Mesh.h"

class Model {
public:
    Model(const Mesh &mesh,
          GLuint sharedVbo,
          GLuint sharedIbo,
          GLsizeiptr vboByteOffset,
          GLsizeiptr iboByteOffset,
          GLint positionAttribLoc,
          GLint uvAttribLoc,
          GLint normalAttribLoc,
          GLint boneIndicesAttribLoc = -1,
          GLint boneWeightsAttribLoc = -1);
    ~Model();

    Model(const Model &) = delete;
    Model &operator=(const Model &) = delete;
    Model(Model &&) = default;
    Model &operator=(Model &&) = default;

    void draw() const;

    GLsizei indexCount() const { return indexCount_; }

private:
    GLuint sharedVbo_;
    GLuint sharedIbo_;
    GLsizeiptr vboByteOffset_;
    GLsizeiptr iboByteOffset_;
    GLsizeiptr indexCount_;
    GLint positionAttribLoc_;
    GLint uvAttribLoc_;
    GLint normalAttribLoc_;
    GLint boneIndicesAttribLoc_;
    GLint boneWeightsAttribLoc_;
    GLsizei stride_;
};

#endif
