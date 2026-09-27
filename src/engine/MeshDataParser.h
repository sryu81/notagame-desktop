#ifndef NOT_A_GAME_MESH_DATA_PARSER_H
#define NOT_A_GAME_MESH_DATA_PARSER_H

#include <cstdint>
#include <cstddef>

#include "Mesh.h"

namespace meshdata {

bool load(const uint8_t *bytes, size_t size, Mesh &out);

void selfTest();

} // namespace meshdata

#endif
