#ifndef NOT_A_GAME_LAB_ASSETS_H
#define NOT_A_GAME_LAB_ASSETS_H

#include <string>

#include "AssetRegistry.h"

class Platform;
class Mesh;
class Model;
struct Material;

namespace game {

class LabAssets {
public:
    explicit LabAssets(const Platform& platform);
    ~LabAssets();

    LabAssets(const LabAssets&) = delete;
    LabAssets& operator=(const LabAssets&) = delete;

    asset::Handle addMesh(std::string name, Mesh mesh);
    void upload();
    bool uploaded() const { return uploaded_; }

    const Material* material(const std::string& name) const;
    const Mesh*  mesh (const std::string& name) const;
    const Model* model(const std::string& name) const;
    const Mesh*  mesh (asset::Handle h) const { return registry_.mesh(h); }
    const Model* model(asset::Handle h) const { return registry_.model(h); }

    asset::Registry& registry() { return registry_; }

private:
    asset::Registry registry_;
    bool uploaded_ = false;
};

} // namespace game

#endif
