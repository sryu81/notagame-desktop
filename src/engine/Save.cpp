#include "Save.h"

#include "AndroidOut.h"
#include "Saveable.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

namespace save {

bool writeBytes(const char *path, const void *data, size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, size, f) == size;
    fclose(f);
    return ok;
}

bool readBytes(const char *path, void *data, size_t size) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    bool ok = fread(data, 1, size, f) == size;
    fclose(f);
    return ok;
}

bool fileExists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

bool writeAll(const char* path, const engine::Saveable& obj) {
    engine::SaveWriter w;
    obj.save(w);
    return writeBytes(path, w.bytes().data(), w.bytes().size());
}

bool loadAll(const char* path, engine::Saveable& obj) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return false; }
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    size_t read = fread(buf.data(), 1, buf.size(), f);
    fclose(f);
    if (read != buf.size()) return false;
    engine::SaveReader r(buf.data(), buf.size());
    obj.load(r);
    return !r.failed();
}

void selfTest(const char* tmpDir) {
    aout << "[save] self-test starting" << std::endl;

    assert(!fileExists("/nonexistent_xyz_nagsave.bin"));
    assert(!readBytes("/nonexistent_xyz_nagsave.bin", nullptr, 0));

    std::string path = std::string(tmpDir) + "/save_selftest.bin";

    struct TestData { uint32_t magic; int32_t hp; float x, y; };
    TestData wr = {0xDEADBEEF, 42, 1.5f, -3.7f};
    assert(writeBytes(path.c_str(), &wr, sizeof(wr)));
    assert(fileExists(path.c_str()));

    TestData rd = {};
    assert(readBytes(path.c_str(), &rd, sizeof(rd)));
    assert(rd.magic == wr.magic);
    assert(rd.hp    == wr.hp);
    assert(rd.x     == wr.x);
    assert(rd.y     == wr.y);

    TestData wr2 = {0x12345678, 99, 0.0f, 0.0f};
    assert(writeBytes(path.c_str(), &wr2, sizeof(wr2)));
    TestData rd2 = {};
    assert(readBytes(path.c_str(), &rd2, sizeof(rd2)));
    assert(rd2.magic == wr2.magic);
    assert(rd2.hp    == wr2.hp);

    struct BigData { uint32_t a[100]; };
    BigData big = {};
    assert(!readBytes(path.c_str(), &big, sizeof(big)));

    std::remove(path.c_str());
    assert(!fileExists(path.c_str()));

    aout << "[save] self-test PASS" << std::endl;
}

} // namespace save
