#include "LabAssets.h"

#include <cassert>
#include <memory>

#include "Engine.h"

namespace game {

namespace {

std::unique_ptr<Shader> loadFrameShader(const Platform& p, const char* vsPath, const char* fsPath) {
    const std::string vs = shader::loadSource(p, vsPath);
    const std::string fs = shader::loadSource(p, fsPath);
    std::unique_ptr<Shader> sh(Shader::loadShader(vs, fs));
    assert(sh);
    sh->bindUniformBlock("FrameData", 0);
    return sh;
}

} // namespace

LabAssets::LabAssets(const Platform& platform) {
    const asset::Handle hLit = registry_.addShader(
        "lit", loadFrameShader(platform, "shaders/lit.vert", "shaders/lit.frag"));
    const asset::Handle hSkinned = registry_.addShader(
        "skinned", loadFrameShader(platform, "shaders/skinned.vert", "shaders/lit.frag"));
    const asset::Handle hUnlit = registry_.addShader(
        "unlit", loadFrameShader(platform, "shaders/unlit.vert", "shaders/unlit.frag"));
    registry_.addMaterial("lit",     {registry_.shader(hLit),     nullptr});
    registry_.addMaterial("skinned", {registry_.shader(hSkinned), nullptr});
    registry_.addMaterial("unlit",   {registry_.shader(hUnlit),   nullptr});

    registry_.addMesh("plane", primitives::makePlane(1.0f));
    registry_.addMesh("cube", primitives::makeCube(1.0f));
}

LabAssets::~LabAssets() {
    registry_.releaseGL();
}

asset::Handle LabAssets::addMesh(std::string name, Mesh mesh) {
    assert(!uploaded_);
    return registry_.addMesh(std::move(name), std::move(mesh));
}

void LabAssets::upload() {
    if (uploaded_) return;
    registry_.upload(Shader::kPositionAttrib, Shader::kUVAttrib, Shader::kNormalAttrib,
                     Shader::kBoneIndicesAttrib, Shader::kBoneWeightsAttrib);
    uploaded_ = true;
}

const Material* LabAssets::material(const std::string& name) const {
    return registry_.material(registry_.findMaterial(name));
}

const Mesh* LabAssets::mesh(const std::string& name) const {
    return registry_.mesh(registry_.find(name));
}

const Model* LabAssets::model(const std::string& name) const {
    return registry_.model(registry_.find(name));
}

} // namespace game
