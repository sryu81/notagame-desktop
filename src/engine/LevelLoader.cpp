#include "LevelLoader.h"

#include <algorithm>
#include <cassert>
#include <cstring>

#include "AndroidOut.h"

namespace level {

static constexpr uint32_t kMagic    = 0x4C564C31u;
static constexpr uint32_t kVersion  = 2u;
static constexpr uint32_t kVersion3 = 3u;

static constexpr size_t kCellSize    = 8;
static constexpr size_t kUnitSize    = 38;
static constexpr size_t kUnitSize3   = 56;
static constexpr size_t kPropSize    = 32;
static constexpr size_t kPropSize3   = 24;
static constexpr size_t kCatalogIdLen = 16;

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
    if (magic != kMagic || (version != kVersion && version != kVersion3)) return false;
    const bool v3 = version == kVersion3;

    uint16_t width = 0, height = 0;
    uint16_t unitCount = 0, propCount = 0;
    float cellSize = 0.0f;
    if (!r.u16(width) || !r.u16(height) ||
        !r.u16(unitCount) || !r.u16(propCount) ||
        !r.f32(cellSize)) return false;
    if (width == 0 || height == 0) return false;
    if (cellSize <= 0.0f) return false;
    out.version = version;
    if (v3) {
        uint8_t win = 0;
        if (!r.u8(win) || !r.u8(out.win.a) || !r.u8(out.win.b) || !r.skip(1)) return false;
        if (win > static_cast<uint8_t>(WinType::ReachCell)) return false;
        out.win.type = static_cast<WinType>(win);
    }

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

    if (!r.need(static_cast<size_t>(unitCount) * (v3 ? kUnitSize3 : kUnitSize))) return false;
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
        if (v3) {
            const char* id = reinterpret_cast<const char*>(r.cur);
            size_t len = 0;
            while (len < kCatalogIdLen && id[len] != 0) ++len;
            u.catalogId.assign(id, len);
            if (!r.skip(kCatalogIdLen) || !r.u16(u.unitId)) return false;
        }
    }

    if (!r.need(static_cast<size_t>(propCount) * (v3 ? kPropSize3 : kPropSize))) return false;
    out.props.resize(propCount);
    for (uint16_t i = 0; i < propCount; ++i) {
        Prop& p = out.props[i];
        if (v3) {
            uint8_t blocks = 0;
            if (!r.u8(p.type) || !r.u8(p.x) || !r.u8(p.y) || !r.u8(blocks)) return false;
            if (p.x >= width || p.y >= height) return false;
            p.blocks = blocks != 0;
            if (!r.f32(p.yaw) || !r.f32(p.scale)) return false;
        } else {
            if (!r.u8(p.type)) return false;
            if (!r.skip(3)) return false;
            if (!r.f32(p.pos.x) || !r.f32(p.pos.y) || !r.f32(p.pos.z)) return false;
            if (!r.f32(p.scale)) return false;
        }
        if (!r.f32(p.color.x) || !r.f32(p.color.y) || !r.f32(p.color.z)) return false;
    }

    return r.eof();
}

namespace {

struct Writer {
    std::vector<uint8_t> out;
    void raw(const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        out.insert(out.end(), b, b + n);
    }
    void u8(uint8_t v) { out.push_back(v); }
    void u16(uint16_t v) { raw(&v, 2); }
    void u32(uint32_t v) { raw(&v, 4); }
    void f32(float v) { raw(&v, 4); }
    void pad(size_t n) { out.insert(out.end(), n, 0); }
};

}

std::vector<uint8_t> save(const Level& l) {
    Writer w;
    w.u32(kMagic);
    w.u32(kVersion3);
    w.u16(l.width);
    w.u16(l.height);
    w.u16(static_cast<uint16_t>(l.units.size()));
    w.u16(static_cast<uint16_t>(l.props.size()));
    w.f32(l.cellSize);
    w.u8(static_cast<uint8_t>(l.win.type));
    w.u8(l.win.a);
    w.u8(l.win.b);
    w.pad(1);
    for (size_t i = 0; i < l.terrain.size(); ++i) {
        w.u8(l.terrain[i]);
        w.pad(3);
        w.f32(i < l.cellY.size() ? l.cellY[i] : 0.0f);
    }
    for (const auto& u : l.units) {
        w.u8(u.faction);
        w.u8(u.x);
        w.u8(u.y);
        w.u8(u.skinned ? 1 : 0);
        w.f32(u.baseColor.x); w.f32(u.baseColor.y); w.f32(u.baseColor.z);
        w.f32(u.selectedColor.x); w.f32(u.selectedColor.y); w.f32(u.selectedColor.z);
        for (int v : {u.maxHp, u.atk, u.def, u.mov, u.range}) w.u16(static_cast<uint16_t>(v));
        char id[kCatalogIdLen] = {};
        std::memcpy(id, u.catalogId.data(), std::min(u.catalogId.size(), kCatalogIdLen));
        w.raw(id, kCatalogIdLen);
        w.u16(u.unitId);
    }
    for (const auto& p : l.props) {
        w.u8(p.type);
        w.u8(p.x);
        w.u8(p.y);
        w.u8(p.blocks ? 1 : 0);
        w.f32(p.yaw);
        w.f32(p.scale);
        w.f32(p.color.x); w.f32(p.color.y); w.f32(p.color.z);
    }
    return w.out;
}

const char* toString(WinType t) {
    switch (t) {
        case WinType::DefeatAll:     return "DEFEAT ALL";
        case WinType::SurviveRounds: return "SURVIVE";
        case WinType::ReachCell:     return "REACH CELL";
    }
    return "?";
}

std::string describe(const WinCondition& w) {
    switch (w.type) {
        case WinType::DefeatAll:     return "DEFEAT ALL";
        case WinType::SurviveRounds: return "SURVIVE " + std::to_string(w.a) + " ROUNDS";
        case WinType::ReachCell:
            return "REACH CELL " + std::to_string(w.a) + "," + std::to_string(w.b);
    }
    return "?";
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
        assert(l.version == 2 && l.units[0].catalogId.empty());
    }

    {
        Level a;
        a.width = 2;
        a.height = 1;
        a.cellSize = 1.5f;
        a.win = {WinType::ReachCell, 1, 0};
        a.terrain = {1, 0};
        a.cellY = {0.25f, -0.5f};
        UnitSpawn u;
        u.faction = 1;
        u.x = 1;
        u.maxHp = 9;
        u.range = 2;
        u.catalogId = "goblin";
        u.unitId = 7;
        a.units.push_back(u);
        u.catalogId = "a_very_long_catalog_id";
        a.units.push_back(u);
        Prop p;
        p.type = 2;
        p.x = 1;
        p.blocks = false;
        p.yaw = 1.5f;
        p.scale = 0.8f;
        a.props.push_back(p);
        const auto bytes = save(a);
        assert(bytes.size() == 24 + 2 * kCellSize + 2 * kUnitSize3 + kPropSize3);
        Level b;
        assert(load(bytes.data(), bytes.size(), b));
        assert(b.version == 3 && b.win.type == WinType::ReachCell && b.win.a == 1);
        assert(b.cellSize == 1.5f && b.cellY[1] == -0.5f && b.terrain[1] == 0);
        assert(b.units[0].catalogId == "goblin" && b.units[0].unitId == 7 && b.units[0].range == 2);
        assert(b.units[1].catalogId == "a_very_long_cata");
        assert(b.props[0].type == 2 && b.props[0].x == 1 && !b.props[0].blocks && b.props[0].yaw == 1.5f);
        assert(save(b) == bytes);
        assert(!load(bytes.data(), bytes.size() - 1, b));
        assert(describe(a.win) == "REACH CELL 1,0");
    }

    aout << "[level] self-test PASS" << std::endl;
}

} // namespace level
