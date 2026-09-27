#include "AssetRegistry.h"

#include <cassert>
#include <GLES3/gl3.h>

#include "AndroidOut.h"

namespace asset {

Handle Registry::addMesh(std::string name, Mesh mesh) {
    assert(!name.empty());
    assert(vbo_ == 0 && "addMesh called after upload");
    entries_.push_back({std::move(name), std::move(mesh), nullptr});
    return static_cast<Handle>(entries_.size());
}

Handle Registry::addShader(std::string name, std::unique_ptr<Shader> shader) {
    assert(!name.empty());
    assert(shader != nullptr);
    shaderEntries_.push_back({std::move(name), std::move(shader)});
    return static_cast<Handle>(shaderEntries_.size());
}

Handle Registry::addMaterial(std::string name, Material mat) {
    assert(!name.empty());
    materialEntries_.push_back({std::move(name), mat});
    return static_cast<Handle>(materialEntries_.size());
}

void Registry::upload(int posAttrib, int uvAttrib, int normalAttrib,
                      int boneIndicesAttrib, int boneWeightsAttrib) {
    if (entries_.empty()) return;

    static constexpr GLsizei kBaseStride = 8 * sizeof(float);
    static constexpr GLsizei kBoneExtra  = 4 * sizeof(uint8_t) + 4 * sizeof(float);

    GLsizeiptr totalVbo = 0, totalIbo = 0;
    for (const auto& e : entries_) {
        const GLsizei stride = e.mesh.hasBones() ? (kBaseStride + kBoneExtra) : kBaseStride;
        totalVbo += static_cast<GLsizeiptr>(e.mesh.positions.size()) * stride;
        totalIbo += static_cast<GLsizeiptr>(e.mesh.indices.size()) * sizeof(uint32_t);
    }

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, totalVbo, nullptr, GL_STATIC_DRAW);

    glGenBuffers(1, &ibo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, totalIbo, nullptr, GL_STATIC_DRAW);

    GLsizeiptr vboOff = 0, iboOff = 0;
    for (auto& e : entries_) {
        e.model = std::make_unique<Model>(
            e.mesh, vbo_, ibo_, vboOff, iboOff,
            posAttrib, uvAttrib, normalAttrib,
            e.mesh.hasBones() ? boneIndicesAttrib : -1,
            e.mesh.hasBones() ? boneWeightsAttrib : -1);
        const GLsizei stride = e.mesh.hasBones() ? (kBaseStride + kBoneExtra) : kBaseStride;
        vboOff += static_cast<GLsizeiptr>(e.mesh.positions.size()) * stride;
        iboOff += static_cast<GLsizeiptr>(e.mesh.indices.size()) * sizeof(uint32_t);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Registry::releaseGL() {
    entries_.clear();
    shaderEntries_.clear();
    materialEntries_.clear();
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (ibo_) { glDeleteBuffers(1, &ibo_); ibo_ = 0; }
}

const Mesh* Registry::mesh(Handle h) const {
    if (h == kNull || h > static_cast<Handle>(entries_.size())) return nullptr;
    return &entries_[h - 1].mesh;
}

const Model* Registry::model(Handle h) const {
    if (h == kNull || h > static_cast<Handle>(entries_.size())) return nullptr;
    return entries_[h - 1].model.get();
}

const Shader* Registry::shader(Handle h) const {
    if (h == kNull || h > static_cast<Handle>(shaderEntries_.size())) return nullptr;
    return shaderEntries_[h - 1].shader.get();
}

const Material* Registry::material(Handle h) const {
    if (h == kNull || h > static_cast<Handle>(materialEntries_.size())) return nullptr;
    return &materialEntries_[h - 1].mat;
}

Handle Registry::find(const std::string& name) const {
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].name == name) return static_cast<Handle>(i + 1);
    }
    return kNull;
}

Handle Registry::findShader(const std::string& name) const {
    for (size_t i = 0; i < shaderEntries_.size(); ++i) {
        if (shaderEntries_[i].name == name) return static_cast<Handle>(i + 1);
    }
    return kNull;
}

Handle Registry::findMaterial(const std::string& name) const {
    for (size_t i = 0; i < materialEntries_.size(); ++i) {
        if (materialEntries_[i].name == name) return static_cast<Handle>(i + 1);
    }
    return kNull;
}

void Registry::selfTest() {
    {
        Registry reg;
        assert(reg.mesh(kNull)     == nullptr);
        assert(reg.model(kNull)    == nullptr);
        assert(reg.material(kNull) == nullptr);
        assert(reg.find("x")       == kNull);
        assert(reg.findMaterial("x") == kNull);
        assert(reg.mesh(99)        == nullptr);
        assert(reg.material(99)    == nullptr);
    }

    {
        Registry reg;
        Mesh tri;
        tri.positions = {{0,0,0}, {1,0,0}, {0,1,0}};
        tri.uvs       = {{0,0},   {1,0},   {0,1}};
        tri.normals   = {{0,0,1}, {0,0,1}, {0,0,1}};
        tri.indices   = {0, 1, 2};

        const Handle hA = reg.addMesh("tri_a", tri);
        const Handle hB = reg.addMesh("tri_b", tri);
        assert(hA != kNull && hB != kNull && hA != hB);
        assert(reg.find("tri_a") == hA);
        assert(reg.find("tri_b") == hB);
        assert(reg.find("missing") == kNull);
        assert(reg.mesh(hA) != nullptr);
        assert(reg.model(hA) == nullptr);
    }

    {
        Registry reg;
        const Handle hLit   = reg.addMaterial("lit",   {nullptr, nullptr});
        const Handle hUnlit = reg.addMaterial("unlit", {nullptr, nullptr});
        assert(hLit != kNull && hUnlit != kNull && hLit != hUnlit);
        assert(reg.findMaterial("lit")   == hLit);
        assert(reg.findMaterial("unlit") == hUnlit);
        assert(reg.findMaterial("nope")  == kNull);
        assert(reg.material(hLit)        != nullptr);
        assert(reg.material(hLit)->shader == nullptr);
    }

    aout << "[asset] self-test PASS" << std::endl;
}

} // namespace asset
