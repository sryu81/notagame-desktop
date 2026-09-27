#ifndef NOT_A_GAME_MATERIAL_H
#define NOT_A_GAME_MATERIAL_H

class Shader;
class TextureAsset;

struct Material {
    const Shader *shader = nullptr;
    const TextureAsset *texture = nullptr;
};

#endif //NOT_A_GAME_MATERIAL_H
