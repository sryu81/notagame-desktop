#include "Model.h"

#include <cassert>
#include <vector>

static constexpr GLsizei kBaseStride = 8 * sizeof(float); // pos3 + uv2 + normal3
static constexpr GLsizei kBoneExtra  = 4 * sizeof(uint8_t) + 4 * sizeof(float); // boneIndices(4b) + boneWeights(4f)

Model::Model(const Mesh &mesh,
             GLuint sharedVbo,
             GLuint sharedIbo,
             GLsizeiptr vboByteOffset,
             GLsizeiptr iboByteOffset,
             GLint positionAttribLoc,
             GLint uvAttribLoc,
             GLint normalAttribLoc,
             GLint boneIndicesAttribLoc,
             GLint boneWeightsAttribLoc) :
        sharedVbo_(sharedVbo),
        sharedIbo_(sharedIbo),
        vboByteOffset_(vboByteOffset),
        iboByteOffset_(iboByteOffset),
        indexCount_(static_cast<GLsizei>(mesh.indices.size())),
        positionAttribLoc_(positionAttribLoc),
        uvAttribLoc_(uvAttribLoc),
        normalAttribLoc_(normalAttribLoc),
        boneIndicesAttribLoc_(boneIndicesAttribLoc),
        boneWeightsAttribLoc_(boneWeightsAttribLoc),
        stride_(mesh.hasBones() ? (kBaseStride + kBoneExtra) : kBaseStride) {
    assert(!mesh.positions.empty());
    assert(mesh.positions.size() == mesh.uvs.size());
    assert(mesh.positions.size() == mesh.normals.size());
    assert(mesh.indices.size() % 3 == 0);
    if (mesh.hasBones()) {
        assert(mesh.positions.size() == mesh.boneIndices.size());
        assert(mesh.positions.size() == mesh.boneWeights.size());
    }

    const size_t vertCount = mesh.positions.size();
    const GLsizei stride = stride_;

    std::vector<uint8_t> interleaved(vertCount * stride);
    for (size_t i = 0; i < vertCount; ++i) {
        uint8_t *vert = &interleaved[i * stride];
        float *fvert = reinterpret_cast<float *>(vert);

        fvert[0] = mesh.positions[i].x;
        fvert[1] = mesh.positions[i].y;
        fvert[2] = mesh.positions[i].z;

        fvert[3] = mesh.uvs[i].x;
        fvert[4] = mesh.uvs[i].y;

        fvert[5] = mesh.normals[i].x;
        fvert[6] = mesh.normals[i].y;
        fvert[7] = mesh.normals[i].z;

        if (mesh.hasBones()) {
            uint8_t *boneIdx = vert + kBaseStride;
            boneIdx[0] = mesh.boneIndices[i][0];
            boneIdx[1] = mesh.boneIndices[i][1];
            boneIdx[2] = mesh.boneIndices[i][2];
            boneIdx[3] = mesh.boneIndices[i][3];

            float *boneW = reinterpret_cast<float *>(boneIdx + 4);
            boneW[0] = mesh.boneWeights[i][0];
            boneW[1] = mesh.boneWeights[i][1];
            boneW[2] = mesh.boneWeights[i][2];
            boneW[3] = mesh.boneWeights[i][3];
        }
    }

    const GLsizeiptr vboUploadBytes =
            static_cast<GLsizeiptr>(interleaved.size());
    glBindBuffer(GL_ARRAY_BUFFER, sharedVbo_);
    GLint64 vboSize = 0;
    glGetBufferParameteri64v(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &vboSize);
    assert(vboByteOffset_ >= 0 && vboUploadBytes >= 0);
    assert(vboByteOffset_ + vboUploadBytes <= vboSize);
    glBufferSubData(GL_ARRAY_BUFFER, vboByteOffset_, vboUploadBytes, interleaved.data());

    const GLsizeiptr iboUploadBytes =
            static_cast<GLsizeiptr>(mesh.indices.size()) * sizeof(uint32_t);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sharedIbo_);
    GLint64 iboSize = 0;
    glGetBufferParameteri64v(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &iboSize);
    assert(iboByteOffset_ >= 0 && iboUploadBytes >= 0);
    assert(iboByteOffset_ + iboUploadBytes <= iboSize);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, iboByteOffset_, iboUploadBytes, mesh.indices.data());
}

Model::~Model() = default;

void Model::draw() const {
    constexpr GLsizeiptr uvByteOffset   = 3 * sizeof(float);
    constexpr GLsizeiptr normalByteOff  = 5 * sizeof(float);
    const GLsizeiptr boneIdxOffset      = static_cast<GLsizeiptr>(kBaseStride);
    const GLsizeiptr boneWeightOffset   = boneIdxOffset + 4 * sizeof(uint8_t);
    const GLsizei stride                = stride_;

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, sharedVbo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sharedIbo_);

    glEnableVertexAttribArray(positionAttribLoc_);
    glVertexAttribPointer(positionAttribLoc_, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void *>(vboByteOffset_));
    glEnableVertexAttribArray(uvAttribLoc_);
    glVertexAttribPointer(uvAttribLoc_, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void *>(vboByteOffset_ + uvByteOffset));
    glEnableVertexAttribArray(normalAttribLoc_);
    glVertexAttribPointer(normalAttribLoc_, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<const void *>(vboByteOffset_ + normalByteOff));

    if (boneIndicesAttribLoc_ >= 0 && boneWeightsAttribLoc_ >= 0) {
        glEnableVertexAttribArray(boneIndicesAttribLoc_);
        glVertexAttribIPointer(boneIndicesAttribLoc_, 4, GL_UNSIGNED_BYTE, stride,
                               reinterpret_cast<const void *>(vboByteOffset_ + boneIdxOffset));
        glEnableVertexAttribArray(boneWeightsAttribLoc_);
        glVertexAttribPointer(boneWeightsAttribLoc_, 4, GL_FLOAT, GL_FALSE, stride,
                              reinterpret_cast<const void *>(vboByteOffset_ + boneWeightOffset));
    }

    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT,
                   reinterpret_cast<const void *>(iboByteOffset_));
}
