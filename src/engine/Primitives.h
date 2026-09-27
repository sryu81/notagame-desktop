#ifndef NOT_A_GAME_PRIMITIVES_H
#define NOT_A_GAME_PRIMITIVES_H

#include <functional>

#include "Mesh.h"

namespace primitives {

Mesh makePlane(float size);

Mesh makeCube(float size);

Mesh makeUVSphere(float radius, int rings, int segments);

Mesh makeHeightmap(int width, int height, float cellSize,
                   const std::function<float(int, int)>& yAt);

void selfTest();

}

#endif
