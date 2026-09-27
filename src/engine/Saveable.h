#ifndef NOT_A_GAME_SAVEABLE_H
#define NOT_A_GAME_SAVEABLE_H

#include <cstdint>
#include <string>
#include <vector>

#include "Math.h"

namespace engine {

class SaveWriter {
public:
    SaveWriter() = default;

    void writeU32(uint32_t v);
    void writeI32(int32_t v);
    void writeFloat(float v);
    void writeString(const std::string& s);
    void writeVec3(const math::Vec3& v);
    void writeBytes(const void* data, size_t n);

    const std::vector<uint8_t>& bytes() const { return buf_; }
    size_t size() const { return buf_.size(); }

    void clear() { buf_.clear(); }

private:
    std::vector<uint8_t> buf_;
};

class SaveReader {
public:
    SaveReader(const uint8_t* data, size_t size);

    bool readU32(uint32_t& v);
    bool readI32(int32_t& v);
    bool readFloat(float& v);
    bool readString(std::string& s);
    bool readVec3(math::Vec3& v);
    bool readBytes(void* out, size_t n);

    size_t remaining() const { return pos_ >= size_ ? 0 : size_ - pos_; }
    size_t pos() const { return pos_; }
    bool failed() const { return !ok_; }

private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_;
    bool ok_;
};

class Saveable {
public:
    virtual ~Saveable() = default;
    virtual void save(SaveWriter& w) const = 0;
    virtual void load(SaveReader& r) = 0;
};

void saveableSelfTest();

} // namespace engine

#endif
