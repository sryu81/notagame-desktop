#ifndef NOT_A_GAME_WAV_LOADER_H
#define NOT_A_GAME_WAV_LOADER_H

#include <cstdint>
#include <cstddef>
#include <vector>

namespace wav {

struct Info {
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    uint16_t audioFormat = 0; // 1=PCM, 3=IEEE float
    const uint8_t *data = nullptr;
    size_t dataSize = 0;
};

bool parse(const uint8_t *bytes, size_t size, Info &out);

bool loadMonoFloat(const uint8_t *bytes, size_t size,
                   std::vector<float> &outSamples,
                   uint32_t &outSampleRate);

} // namespace wav

#endif
