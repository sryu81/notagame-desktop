#include "LevelLoader.h"

#include <cassert>
#include <cstring>

#include "AndroidOut.h"

namespace level {

static constexpr uint32_t kMagic   = 0x4C564C31u;
static constexpr uint32_t kVersion = 2u;

static constexpr size_t kHeaderSize = 20;
static constexpr size_t kCellSize   = 8;
static constexpr size_t kUnitSize   = 38;
static constexpr size_t kPropSize   = 32;

namespace {

struct Reader {
    const uint8_t* cur;
    const uint8_t* end;

    bool need(size_t n) const { return (cur + n) <= end; }
    bool eof() const { return cur >= end; }

    bool u32(uint32_t& v) {
        if (!need(4)) return false;
        std::memcpy(&v, cur, 4); cur += 4;
        return true;
    }
    bool u16(uint16_t& v) {
        if (!need(2)) return false;
        std::memcpy(&v, cur, 2); cur += 2;
        return true;
    }
    bool u8(uint8_t& v) {
        if (!need(1)) return false;
        v = *cur++;
        return true;
    }
    bool f32(float& v) {
        if (!need(4)) return false;
        std::memcpy(&v, cur, 4); cur += 4;
        return true;
    }
    bool skip(size_t n) {
        if (!need(n)) return false;
        cur += n;
        return true;
    }
    bool align4() {
        size_t pad = (reinterpret_cast<uintptr_t>(cur) & 3u);
        if (pad) return skip(4 - pad);
        return true;
    }
};

} // namespace

bool load(const uint8_t* bytes, size_t size, Level& out) {
    out = {};
    if (!bytes || size == 0) return false;

    Reader r{bytes, bytes + size};

    uint32_t magic = 0, version = 0;
    if (!r.u32(magic) || !r.u32(version)) return false;
    if (magic != kMagic || version != kVersion) return false;

    uint16_t width = 0, height = 0;
    uint16_t unitCount = 0, propCount = 0;
    float cellSize = 0.0f;
    if (!r.u16(width) || !r.u16(height) ||
        !r.u16(unitCount) || !r.u16(propCount) ||
        !r.f32(cellSize)) return false;
    if (width == 0 || height == 0) return false;
    if (cellSize <= 0.0f) return false;

    const size_t cellTotal = static_cast<size_t>(width) * static_cast<size_t>(height);

    if (!r.need(cellTotal * kCellSize)) return false;
    out.width    = width;
    out.height   = height;
    out.cellSize = cellSize;
    out.terrain.assign(cellTotal, 1);
    out.cellY.assign(cellTotal, 0.0f);
    for (size_t i = 0; i < cellTotal; ++i) {
        if (!r.u8(out.terrain[i])) return false;
        if (!r.skip(3)) return false;
        if (!r.f32(out.cellY[i])) return false;
    }

    if (!r.need(static_cast<size_t>(unitCount) * kUnitSize)) return false;
    out.units.resize(unitCount);
    for (uint16_t i = 0; i < unitCount; ++i) {
        UnitSpawn& u = out.units[i];
        uint8_t faction = 0;
        if (!r.u8(faction)) return false;
        if (faction > 3) return false;
        u.faction = faction;
        if (!r.u8(u.x) || !r.u8(u.y)) return false;
        if (u.x >= width || u.y >= height) return false;
        uint8_t skinned = 0;
        if (!r.u8(skinned)) return false;
        u.skinned = (skinned != 0);
        if (!r.f32(u.baseColor.x) || !r.f32(u.baseColor.y) || !r.f32(u.baseColor.z)) return false;
        if (!r.f32(u.selectedColor.x) || !r.f32(u.selectedColor.y) || !r.f32(u.selectedColor.z)) return false;
        uint16_t maxHp = 0, atk = 0, def = 0, mov = 0, range = 0;
        if (!r.u16(maxHp) || !r.u16(atk) || !r.u16(def) || !r.u16(mov) || !r.u16(range)) return false;
        u.maxHp = static_cast<int>(maxHp);
        u.atk   = static_cast<int>(atk);
        u.def   = static_cast<int>(def);
        u.mov   = static_cast<int>(mov);
        u.range = static_cast<int>(range);
    }

    if (!r.need(static_cast<size_t>(propCount) * kPropSize)) return false;
    out.props.resize(propCount);
    for (uint16_t i = 0; i < propCount; ++i) {
        Prop& p = out.props[i];
        if (!r.u8(p.type)) return false;
        if (!r.skip(3)) return false;
        if (!r.f32(p.pos.x) || !r.f32(p.pos.y) || !r.f32(p.pos.z)) return false;
        if (!r.f32(p.scale)) return false;
        if (!r.f32(p.color.x) || !r.f32(p.color.y) || !r.f32(p.color.z)) return false;
    }

    return r.eof();
}

void selfTest() {
    aout << "[level] self-test starting" << std::endl;

    Level l;
    assert(!load(nullptr, 0,   l));
    assert(!load(nullptr, 64, l));

    uint8_t garbage[64] = {};
    assert(!load(garbage, sizeof(garbage), l));

    {
        uint8_t buf[20] = {};
        std::memcpy(buf, &kMagic, 4);
        std::memcpy(buf + 4, &kVersion, 4);
        assert(!load(buf, 12, l));
    }
    {
        uint8_t buf[20] = {};
        std::memcpy(buf, &kMagic, 4);
        uint32_t bad = 99u;
        std::memcpy(buf + 4, &bad, 4);
        assert(!load(buf, 20, l));
    }
    {
        uint8_t buf[20] = {};
        std::memcpy(buf, &kMagic, 4);
        std::memcpy(buf + 4, &kVersion, 4);
        uint16_t zero = 0;
        std::memcpy(buf + 8, &zero, 2);
        assert(!load(buf, 20, l));
    }

    {
        uint8_t buf[28] = {};
        std::memcpy(buf, &kMagic, 4);
        std::memcpy(buf + 4, &kVersion, 4);
        uint16_t one = 1;
        std::memcpy(buf +  8, &one, 2);
        std::memcpy(buf + 10, &one, 2);
        std::memcpy(buf + 12, &one, 2);
        std::memcpy(buf + 14, &one, 2);
        float cs = 1.0f;
        std::memcpy(buf + 16, &cs, 4);
        assert(!load(buf, 24, l));
    }

    {
        uint8_t buf[20 + 8 + 38 + 32] = {};
        std::memcpy(buf, &kMagic, 4);
        std::memcpy(buf + 4, &kVersion, 4);
        uint16_t one = 1;
        std::memcpy(buf +  8, &one, 2);
        std::memcpy(buf + 10, &one, 2);
        std::memcpy(buf + 12, &one, 2);
        std::memcpy(buf + 14, &one, 2);
        float cs = 1.0f;
        std::memcpy(buf + 16, &cs, 4);
        uint8_t terrain = 0;
        std::memcpy(buf + 20, &terrain, 1);
        std::memcpy(buf + 24, &cs, 4);
        assert(load(buf, sizeof(buf), l));
        assert(l.width == 1);
        assert(l.height == 1);
        assert(l.cellSize == 1.0f);
        assert(l.terrain.size() == 1);
        assert(l.terrain[0] == 0);
        assert(l.cellY[0] == 1.0f);
        assert(l.units.size() == 1);
        assert(l.units[0].faction == 0);
        assert(l.units[0].x == 0);
        assert(l.units[0].y == 0);
        assert(l.units[0].maxHp == 0);
        assert(l.units[0].range == 0);
    }

    aout << "[level] self-test PASS" << std::endl;
}

} // namespace level
