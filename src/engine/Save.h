#ifndef NOT_A_GAME_SAVE_H
#define NOT_A_GAME_SAVE_H

#include <cstddef>

#include "Saveable.h"

namespace save {

bool writeBytes(const char *path, const void *data, size_t size);
bool readBytes(const char *path, void *data, size_t size);
bool fileExists(const char *path);

bool writeAll(const char* path, const engine::Saveable& obj);
bool loadAll(const char* path, engine::Saveable& obj);

void selfTest(const char* tmpDir);

} // namespace save

#endif
