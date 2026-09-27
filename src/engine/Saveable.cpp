#include "Saveable.h"

#include "AndroidOut.h"

#include <cassert>
#include <cstring>

namespace engine {

void SaveWriter::writeU32(uint32_t v) {
    for (int i = 0; i < 4; ++i) buf_.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
}

void SaveWriter::writeI32(int32_t v) {
    writeU32(static_cast<uint32_t>(v));
}

void SaveWriter::writeFloat(float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, sizeof(bits));
    writeU32(bits);
}

void SaveWriter::writeString(const std::string& s) {
    writeU32(static_cast<uint32_t>(s.size()));
    buf_.insert(buf_.end(), s.begin(), s.end());
}

void SaveWriter::writeVec3(const math::Vec3& v) {
    writeFloat(v.x);
    writeFloat(v.y);
    writeFloat(v.z);
}

void SaveWriter::writeBytes(const void* data, size_t n) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    buf_.insert(buf_.end(), p, p + n);
}

SaveReader::SaveReader(const uint8_t* data, size_t size)
    : data_(data), size_(size), pos_(0), ok_(true) {}

bool SaveReader::readU32(uint32_t& v) {
    if (!ok_ || pos_ + 4 > size_) { ok_ = false; return false; }
    v = 0;
    for (int i = 0; i < 4; ++i) {
        v |= static_cast<uint32_t>(data_[pos_++]) << (i * 8);
    }
    return true;
}

bool SaveReader::readI32(int32_t& v) {
    uint32_t u;
    if (!readU32(u)) return false;
    v = static_cast<int32_t>(u);
    return true;
}

bool SaveReader::readFloat(float& v) {
    uint32_t bits;
    if (!readU32(bits)) return false;
    std::memcpy(&v, &bits, sizeof(v));
    return true;
}

bool SaveReader::readString(std::string& s) {
    uint32_t len;
    if (!readU32(len)) return false;
    if (pos_ + len > size_) { ok_ = false; return false; }
    s.assign(reinterpret_cast<const char*>(data_ + pos_), len);
    pos_ += len;
    return true;
}

bool SaveReader::readVec3(math::Vec3& v) {
    if (!readFloat(v.x)) return false;
    if (!readFloat(v.y)) return false;
    if (!readFloat(v.z)) return false;
    return true;
}

bool SaveReader::readBytes(void* out, size_t n) {
    if (!ok_ || pos_ + n > size_) { ok_ = false; return false; }
    std::memcpy(out, data_ + pos_, n);
    pos_ += n;
    return true;
}

namespace {

struct MockUnit : public Saveable {
    int hp = 0;
    int atk = 0;
    float x = 0, y = 0, z = 0;
    std::string name;

    void save(SaveWriter& w) const override {
        w.writeI32(hp);
        w.writeI32(atk);
        w.writeFloat(x);
        w.writeFloat(y);
        w.writeFloat(z);
        w.writeString(name);
    }

    void load(SaveReader& r) override {
        r.readI32(hp);
        r.readI32(atk);
        r.readFloat(x);
        r.readFloat(y);
        r.readFloat(z);
        r.readString(name);
    }
};

} // namespace

void saveableSelfTest() {
    aout << "[saveable] self-test starting" << std::endl;

    {
        SaveWriter w;
        w.writeU32(0xDEADBEEF);
        w.writeI32(-42);
        w.writeFloat(3.14f);
        w.writeString("hello");
        w.writeVec3(math::Vec3(1.5f, -2.5f, 3.5f));
        assert(w.size() > 0);

        SaveReader r(w.bytes().data(), w.size());

        uint32_t u32;
        assert(r.readU32(u32));
        assert(u32 == 0xDEADBEEF);

        int32_t i32;
        assert(r.readI32(i32));
        assert(i32 == -42);

        float f;
        assert(r.readFloat(f));
        assert(f == 3.14f);

        std::string s;
        assert(r.readString(s));
        assert(s == "hello");

        math::Vec3 v;
        assert(r.readVec3(v));
        assert(v.x == 1.5f);
        assert(v.y == -2.5f);
        assert(v.z == 3.5f);

        assert(r.remaining() == 0);
        assert(!r.failed());
    }

    {
        MockUnit a;
        a.hp = 100;
        a.atk = 25;
        a.x = 1.0f; a.y = 2.0f; a.z = 3.0f;
        a.name = "knight";

        SaveWriter w;
        a.save(w);
        assert(w.size() > 0);

        MockUnit b;
        SaveReader r(w.bytes().data(), w.size());
        b.load(r);

        assert(b.hp == 100);
        assert(b.atk == 25);
        assert(b.x == 1.0f);
        assert(b.y == 2.0f);
        assert(b.z == 3.0f);
        assert(b.name == "knight");
        assert(!r.failed());
    }

    {
        uint8_t buf[3] = {1, 2, 3};
        SaveReader r(buf, 3);
        uint32_t v;
        assert(!r.readU32(v));
        assert(r.failed());
    }

    {
        SaveWriter w;
        w.writeString("0123456789");
        SaveReader r(w.bytes().data(), w.size());
        std::string s;
        assert(r.readString(s));
        assert(s == "0123456789");
    }

    aout << "[saveable] self-test PASS" << std::endl;
}

} // namespace engine
