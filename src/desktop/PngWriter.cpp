#include "PngWriter.h"

#include <algorithm>
#include <cassert>
#include <cstdio>

#include "Engine.h"

namespace desktop {

namespace {

uint32_t crc32(const uint8_t* data, size_t n, uint32_t crc = 0xFFFFFFFFu) {
    for (size_t i = 0; i < n; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc;
}

void put32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v >> 24));
    out.push_back(static_cast<uint8_t>(v >> 16));
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v));
}

void chunk(std::vector<uint8_t>& out, const char* tag, const std::vector<uint8_t>& data) {
    put32(out, static_cast<uint32_t>(data.size()));
    const size_t start = out.size();
    out.insert(out.end(), tag, tag + 4);
    out.insert(out.end(), data.begin(), data.end());
    put32(out, crc32(out.data() + start, out.size() - start) ^ 0xFFFFFFFFu);
}

}

std::vector<uint8_t> encodePngRgba(int width, int height, const std::vector<uint8_t>& rgba) {
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(height) * (width * 4 + 1));
    for (int y = 0; y < height; ++y) {
        raw.push_back(0);
        const uint8_t* row = rgba.data() + static_cast<size_t>(y) * width * 4;
        raw.insert(raw.end(), row, row + width * 4);
    }

    std::vector<uint8_t> z = {0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) { a = (a + v) % 65521u; b = (b + a) % 65521u; }
    size_t pos = 0;
    do {
        const size_t len = std::min<size_t>(65535, raw.size() - pos);
        const bool last = pos + len == raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(static_cast<uint8_t>(len & 0xFF));
        z.push_back(static_cast<uint8_t>(len >> 8));
        z.push_back(static_cast<uint8_t>(~len & 0xFF));
        z.push_back(static_cast<uint8_t>((~len >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + static_cast<long>(pos), raw.begin() + static_cast<long>(pos + len));
        pos += len;
    } while (pos < raw.size());
    put32(z, (b << 16) | a);

    std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<uint8_t> ihdr;
    put32(ihdr, static_cast<uint32_t>(width));
    put32(ihdr, static_cast<uint32_t>(height));
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
    chunk(png, "IHDR", ihdr);
    chunk(png, "IDAT", z);
    chunk(png, "IEND", {});
    return png;
}

bool writePngRgba(const std::string& path, int width, int height, const std::vector<uint8_t>& rgba) {
    if (width <= 0 || height <= 0 || rgba.size() < static_cast<size_t>(width) * height * 4) return false;
    const std::vector<uint8_t> png = encodePngRgba(width, height, rgba);
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(png.data(), 1, png.size(), f) == png.size();
    std::fclose(f);
    return ok;
}

void pngSelfTest() {
    aout << "[png] self-test starting" << std::endl;
    const uint8_t iend[] = {'I', 'E', 'N', 'D'};
    assert((crc32(iend, 4) ^ 0xFFFFFFFFu) == 0xAE426082u);
    const std::vector<uint8_t> px = {255, 0, 0, 255, 0, 255, 0, 255};
    const std::vector<uint8_t> png = encodePngRgba(2, 1, px);
    assert(png.size() > 8 && png[1] == 'P' && png[2] == 'N' && png[3] == 'G');
    assert(png[png.size() - 8] == 'I' && png[png.size() - 5] == 'D');
    aout << "[png] self-test PASS" << std::endl;
}

} // namespace desktop
