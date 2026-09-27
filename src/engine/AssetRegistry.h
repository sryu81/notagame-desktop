#ifndef NOT_A_GAME_ASSETREGISTRY_H
#define NOT_A_GAME_ASSETREGISTRY_H

#include <GLES3/gl3.h>
#include <memory>
#include <string>
#include <vector>

#include "Material.h"
#include "Mesh.h"
#include "Model.h"
#include "Shader.h"

namespace asset {

using Handle = uint32_t;
static constexpr Handle kNull = 0;

class Registry {
public:
    Handle addMesh(std::string name, Mesh mesh);

    void upload(int posAttrib, int uvAttrib, int normalAttrib,
                int boneIndicesAttrib = -1, int boneWeightsAttrib = -1);

    void releaseGL();

    const Mesh*  mesh (Handle h) const;
    const Model* model(Handle h) const;

    Handle find(const std::string& name) const;

    Handle addShader(std::string name, std::unique_ptr<Shader> shader);
    const Shader* shader(Handle h) const;
    Handle findShader(const std::string& name) const;

    Handle addMaterial(std::string name, Material mat);
    const Material* material(Handle h) const;
    Handle findMaterial(const std::string& name) const;

    static void selfTest();

private:
    struct MeshEntry {
        std::string name;
        Mesh mesh;
        std::unique_ptr<Model> model;
    };
    struct ShaderEntry {
        std::string name;
        std::unique_ptr<Shader> shader;
    };
    struct MaterialEntry {
        std::string name;
        Material mat;
    };
    std::vector<MeshEntry>     entries_;
    std::vector<ShaderEntry>   shaderEntries_;
    std::vector<MaterialEntry> materialEntries_;
    GLuint vbo_ = 0;
    GLuint ibo_ = 0;
};

} // namespace asset

#endif //NOT_A_GAME_ASSETREGISTRY_H
