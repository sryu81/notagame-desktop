#include "MeshDataParser.h"

#include "AndroidOut.h"

#include <cassert>
#include <cstring>
#include <vector>

namespace meshdata {

namespace {

// Parse a single face-vertex token like "12", "12/3", "12//4", "12/3/4".
// 1-based. Out parts: position, uv, normal indices. Zero means absent.
// Negative values mean "relative to current count" (e.g. -1 = last pos).
struct FaceVertex {
    int v = 0;
    int vt = 0;
    int vn = 0;
};

bool parseFaceVertex(const char *p, const char *end, FaceVertex &out) {
    int sign = 1;
    if (p >= end) return false;
    if (*p == '-') { sign = -1; ++p; }
    if (p >= end || *p < '0' || *p > '9') return false;
    int v = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        ++p;
    }
    out.v = v * sign;

    if (p < end && *p == '/') {
        ++p;
        if (p < end && *p == '/') {
            // v//n
            ++p;
            sign = 1;
            if (p < end && *p == '-') { sign = -1; ++p; }
            if (p < end && *p >= '0' && *p <= '9') {
                int n = 0;
                while (p < end && *p >= '0' && *p <= '9') {
                    n = n * 10 + (*p - '0');
                    ++p;
                }
                out.vn = n * sign;
            }
        } else {
            sign = 1;
            if (p < end && *p == '-') { sign = -1; ++p; }
            if (p < end && *p >= '0' && *p <= '9') {
                int t = 0;
                while (p < end && *p >= '0' && *p <= '9') {
                    t = t * 10 + (*p - '0');
                    ++p;
                }
                out.vt = t * sign;
            }
            if (p < end && *p == '/') {
                ++p;
                sign = 1;
                if (p < end && *p == '-') { sign = -1; ++p; }
                if (p < end && *p >= '0' && *p <= '9') {
                    int n = 0;
                    while (p < end && *p >= '0' && *p <= '9') {
                        n = n * 10 + (*p - '0');
                        ++p;
                    }
                    out.vn = n * sign;
                }
            }
        }
    }

    return out.v != 0;
}

// Resolves 1-based positive + negative-relative indices against a count.
// Returns 0-based or -1 if invalid.
int resolveIndex(int raw, size_t count) {
    if (raw > 0) {
        if (static_cast<size_t>(raw) > count) return -1;
        return raw - 1;
    }
    if (raw < 0) {
        const size_t r = static_cast<size_t>(-raw);
        if (r > count) return -1;
        return static_cast<int>(count) - static_cast<int>(r);
    }
    return -1;
}

} // namespace

bool load(const uint8_t *bytes, size_t size, Mesh &out) {
    if (bytes == nullptr || size == 0) return false;

    out = Mesh{};

    std::vector<math::Vec3> positions;
    std::vector<math::Vec2> uvs;
    std::vector<math::Vec3> normals;

    // Each unique (v, vt, vn) triple becomes a separate vertex in the
    // output mesh — same as slice #4's "24 verts for a cube" approach.
    // Lookup keyed on the resolved 0-based position/uv/normal indices.
    struct Key {
        int v, vt, vn;
        bool operator==(const Key &o) const {
            return v == o.v && vt == o.vt && vn == o.vn;
        }
    };
    std::vector<Key> vertexKeys;
    auto findOrAdd = [&](int vIdx, int vtIdx, int vnIdx) -> uint32_t {
        Key k{vIdx, vtIdx, vnIdx};
        for (size_t i = 0; i < vertexKeys.size(); i++) {
            if (vertexKeys[i] == k) return static_cast<uint32_t>(i);
        }
        const uint32_t newIdx = static_cast<uint32_t>(out.positions.size());
        out.positions.push_back(positions[static_cast<size_t>(vIdx)]);
        if (vtIdx >= 0) {
            out.uvs.push_back(uvs[static_cast<size_t>(vtIdx)]);
        } else {
            out.uvs.push_back(math::Vec2(0.0f, 0.0f));
        }
        if (vnIdx >= 0) {
            out.normals.push_back(normals[static_cast<size_t>(vnIdx)]);
        } else {
            out.normals.push_back(math::Vec3(0.0f, 1.0f, 0.0f));
        }
        vertexKeys.push_back(k);
        return newIdx;
    };

    const char *p = reinterpret_cast<const char *>(bytes);
    const char *end = p + size;

    while (p < end) {
        // Skip leading whitespace.
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\r')) ++p;
        if (p >= end) break;
        if (*p == '\n') { ++p; continue; }
        if (*p == '#') {
            // Comment until end of line.
            while (p < end && *p != '\n') ++p;
            continue;
        }

        // Read first token (no spaces in it).
        const char *tokStart = p;
        while (p < end && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') ++p;
        const std::string_view tok(tokStart, static_cast<size_t>(p - tokStart));
        // Skip whitespace before the rest of the line.
        while (p < end && (*p == ' ' || *p == '\t')) ++p;
        const char *lineEnd = p;
        while (lineEnd < end && *lineEnd != '\n') ++lineEnd;

        if (tok.size() == 1 && tok[0] == 'v' && p < lineEnd) {
            float x, y, z;
            if (std::sscanf(p, "%f %f %f", &x, &y, &z) == 3) {
                positions.emplace_back(x, y, z);
            }
        } else if (tok.size() == 2 && tok[0] == 'v' && tok[1] == 't' && p < lineEnd) {
            float u, vCoord;
            if (std::sscanf(p, "%f %f", &u, &vCoord) == 2) {
                uvs.emplace_back(u, vCoord);
            }
        } else if (tok.size() == 2 && tok[0] == 'v' && tok[1] == 'n' && p < lineEnd) {
            float x, y, z;
            if (std::sscanf(p, "%f %f %f", &x, &y, &z) == 3) {
                normals.emplace_back(x, y, z);
            }
        } else if (tok.size() == 1 && tok[0] == 'f' && p < lineEnd) {
            // Parse face vertices.
            std::vector<int> faceIdx;
            const char *fp = p;
            while (fp < lineEnd) {
                while (fp < lineEnd && (*fp == ' ' || *fp == '\t')) ++fp;
                if (fp >= lineEnd) break;
                FaceVertex fv;
                if (parseFaceVertex(fp, lineEnd, fv)) {
                    const int vIdx = resolveIndex(fv.v, positions.size());
                    const int vtIdx = resolveIndex(fv.vt, uvs.size());
                    const int vnIdx = resolveIndex(fv.vn, normals.size());
                    if (vIdx < 0) return false;
                    // Each face-vertex becomes a unique (v, vt, vn) in
                    // the output — even if some are duplicated, this
                    // keeps the per-face shading correct (slice #4 style).
                    faceIdx.push_back(static_cast<int>(findOrAdd(vIdx,
                                                                  vtIdx >= 0 ? vtIdx : -1,
                                                                  vnIdx >= 0 ? vnIdx : -1)));
                }
                while (fp < lineEnd && *fp != ' ' && *fp != '\t') ++fp;
            }
            // Fan triangulate.
            if (faceIdx.size() < 3) return false;
            for (size_t i = 1; i + 1 < faceIdx.size(); i++) {
                out.indices.push_back(static_cast<uint32_t>(faceIdx[0]));
                out.indices.push_back(static_cast<uint32_t>(faceIdx[i]));
                out.indices.push_back(static_cast<uint32_t>(faceIdx[i + 1]));
            }
        }
        // else: unknown line (g, o, s, mtllib, usemtl, …) — skip.

        p = lineEnd;
        if (p < end && *p == '\n') ++p;
    }

    if (out.positions.empty() || out.indices.empty()) return false;
    if (out.positions.size() != out.uvs.size()) return false;
    if (out.positions.size() != out.normals.size()) return false;
    if (out.indices.size() % 3 != 0) return false;

    return true;
}

namespace {

// Build a small .obj string in memory for selfTest: a single triangle.
const char *kTriangleObj =
    "v 0 0 0\n"
    "v 1 0 0\n"
    "v 0 1 0\n"
    "vn 0 0 1\n"
    "f 1//1 2//1 3//1\n";

// A cube with per-face normals and UVs (8 positions, 4 uvs, 6 normals,
// 6 quad faces = 24 unique (v, vt, vn) triples, 12 triangles).
const char *kCubeObj =
    "v -1 -1 -1\n"
    "v  1 -1 -1\n"
    "v  1  1 -1\n"
    "v -1  1 -1\n"
    "v -1 -1  1\n"
    "v  1 -1  1\n"
    "v  1  1  1\n"
    "v -1  1  1\n"
    "vt 0 0\n"
    "vt 1 0\n"
    "vt 1 1\n"
    "vt 0 1\n"
    "vn  0  0 -1\n"
    "vn  0  0  1\n"
    "vn  0 -1  0\n"
    "vn  0  1  0\n"
    "vn -1  0  0\n"
    "vn  1  0  0\n"
    "f 1/1/1 4/2/1 3/3/1 2/4/1\n"
    "f 5/1/2 6/2/2 7/3/2 8/4/2\n"
    "f 1/1/3 2/2/3 6/3/3 5/4/3\n"
    "f 4/1/4 8/2/4 7/3/4 3/4/4\n"
    "f 1/1/5 5/2/5 8/3/5 4/4/5\n"
    "f 2/1/6 3/2/6 7/3/6 6/4/6\n";

} // namespace

void selfTest() {
    aout << "[meshdata] self-test starting" << std::endl;

    // Triangle: 3 positions, 1 normal, 3 unique (v,vn) triples, 3 indices.
    {
        Mesh m;
        bool ok = load(reinterpret_cast<const uint8_t *>(kTriangleObj),
                       std::strlen(kTriangleObj), m);
        assert(ok);
        assert(m.positions.size() == 3);
        assert(m.uvs.size() == 3);
        assert(m.normals.size() == 3);
        assert(m.indices.size() == 3);
        // First vertex should be the v0 position.
        assert(math::approxEqual(m.positions[0].x, 0.0f));
        assert(math::approxEqual(m.positions[1].x, 1.0f));
        assert(math::approxEqual(m.positions[2].y, 1.0f));
    }

    // Cube: 8 positions + 4 UVs + 6 normals, but per-face expansion
    // gives 24 unique vertices and 12 triangles = 36 indices.
    {
        Mesh m;
        bool ok = load(reinterpret_cast<const uint8_t *>(kCubeObj),
                       std::strlen(kCubeObj), m);
        assert(ok);
        assert(m.positions.size() == 24);
        assert(m.uvs.size() == 24);
        assert(m.normals.size() == 24);
        assert(m.indices.size() == 36);
        // All cube face normals are unit-axis aligned; spot-check.
        int plusZCount = 0;
        for (const auto &n : m.normals) {
            if (math::approxEqual(n.z, 1.0f, 1e-4f)) ++plusZCount;
        }
        assert(plusZCount == 4); // 4 verts on the +Z face
    }

    // Reject empty / null input.
    {
        Mesh m;
        assert(!load(nullptr, 0, m));
        assert(!load(reinterpret_cast<const uint8_t *>(""), 0, m));
    }

    // Reject malformed: no positions, no indices, or v/vt/vn count mismatch.
    {
        Mesh m;
        const char *noFace = "v 0 0 0\nv 1 0 0\nv 0 1 0\n";
        assert(!load(reinterpret_cast<const uint8_t *>(noFace),
                     std::strlen(noFace), m));
    }

    aout << "[meshdata] self-test PASS" << std::endl;
}

} // namespace meshdata
