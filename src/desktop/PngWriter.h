#ifndef NOT_A_GAME_PNG_WRITER_H
#define NOT_A_GAME_PNG_WRITER_H

#include <cstdint>
#include <string>
#include <vector>

namespace desktop {

bool writePngRgba(const std::string& path, int width, int height, const std::vector<uint8_t>& rgbaTopDown);
std::vector<uint8_t> encodePngRgba(int width, int height, const std::vector<uint8_t>& rgbaTopDown);

void pngSelfTest();

} // namespace desktop

#endif
