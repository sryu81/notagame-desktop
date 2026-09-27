#include "WavLoader.h"

#include <cstring>
#include <cassert>

namespace wav {

namespace {

inline uint32_t readU32LE(const uint8_t *p) {
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

inline uint16_t readU16LE(const uint8_t *p) {
    return static_cast<uint16_t>(p[0])
         | (static_cast<uint16_t>(p[1]) << 8);
}

inline bool fourCC(const uint8_t *p, const char *tag) {
    return p[0] == tag[0] && p[1] == tag[1]
        && p[2] == tag[2] && p[3] == tag[3];
}

} // namespace

bool parse(const uint8_t *bytes, size_t size, Info &out) {
    if (size < 12) return false;
    if (!fourCC(bytes + 0, "RIFF")) return false;
    if (!fourCC(bytes + 8, "WAVE")) return false;

    out = Info{};

    size_t pos = 12;
    while (pos + 8 <= size) {
        const uint8_t *chunkId = bytes + pos;
        uint32_t chunkSize = readU32LE(bytes + pos + 4);

        if (fourCC(chunkId, "fmt ")) {
            if (chunkSize < 16) return false;
            if (pos + 8 + chunkSize > size) return false;
            out.audioFormat   = readU16LE(bytes + pos + 8);
            out.channels      = readU16LE(bytes + pos + 10);
            out.sampleRate    = readU32LE(bytes + pos + 12);
            // byteRate = bytes+16, blockAlign = bytes+20 (ignored)
            out.bitsPerSample = readU16LE(bytes + pos + 22);
        } else if (fourCC(chunkId, "data")) {
            if (pos + 8 + chunkSize > size) return false;
            out.data = bytes + pos + 8;
            out.dataSize = chunkSize;
        }

        // chunks are word-aligned
        pos += 8 + chunkSize + (chunkSize & 1);
    }

    return out.data != nullptr
        && out.sampleRate > 0
        && out.channels > 0
        && out.bitsPerSample > 0;
}

bool loadMonoFloat(const uint8_t *bytes, size_t size,
                   std::vector<float> &outSamples,
                   uint32_t &outSampleRate) {
    Info info;
    if (!parse(bytes, size, info)) return false;
    if (info.audioFormat != 1 && info.audioFormat != 3) return false;

    outSamples.clear();
    outSampleRate = info.sampleRate;

    const uint8_t *p = info.data;
    const size_t bytesPerSampleFrame = info.channels * (info.bitsPerSample / 8);
    if (bytesPerSampleFrame == 0) return false;
    const size_t frameCount = info.dataSize / bytesPerSampleFrame;

    outSamples.reserve(frameCount);

    for (size_t f = 0; f < frameCount; f++) {
        float sum = 0.0f;
        for (uint16_t c = 0; c < info.channels; c++) {
            const uint8_t *s = p + f * bytesPerSampleFrame + c * (info.bitsPerSample / 8);
            float v = 0.0f;
            if (info.audioFormat == 1 && info.bitsPerSample == 16) {
                int16_t i = static_cast<int16_t>(
                    static_cast<uint16_t>(s[0]) | (static_cast<uint16_t>(s[1]) << 8));
                v = static_cast<float>(i) / 32768.0f;
            } else if (info.audioFormat == 1 && info.bitsPerSample == 8) {
                int8_t i = static_cast<int8_t>(s[0]);
                v = static_cast<float>(i) / 128.0f;
            } else if (info.audioFormat == 3 && info.bitsPerSample == 32) {
                float fv;
                std::memcpy(&fv, s, 4);
                v = fv;
            } else {
                return false;
            }
            sum += v;
        }
        outSamples.push_back(sum / static_cast<float>(info.channels));
    }

    return true;
}

} // namespace wav
