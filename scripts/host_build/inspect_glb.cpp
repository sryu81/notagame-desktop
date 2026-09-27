#include "GltfLoader.h"
#include "Animation.h"
#include "Bone.h"
#include "Mesh.h"
#include "Math.h"
#include "AndroidOut.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include <set>

const char* PASS = "  \033[32mPASS\033[0m";
const char* FAIL = "  \033[31mFAIL\033[0m";
const char* WARN = "  \033[33mWARN\033[0m";

int main(int argc, char** argv) {
    const float cellSize = (argc >= 3) ? static_cast<float>(atof(argv[2])) : 1.0f;
    const float maxCellSpan = cellSize * 1.5f;

    if (argc < 2) {
        fprintf(stderr, "usage: inspect_glb <file.glb> [cellSize]\n");
        fprintf(stderr, "  cellSize  grid cell size in world units (default 1.0)\n");
        return 1;
    }

    const char* path = argv[1];
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "FAIL: cannot open %s\n", path); return 1; }

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<unsigned char> data(fileSize);
    fread(data.data(), 1, fileSize, f);
    fclose(f);

    printf("=== inspect_glb: %s ===\n", path);
    printf("file size:  %.1f KB\n", fileSize / 1024.0f);
    printf("cell size:  %.2f (game grid)\n\n", cellSize);

    int errors = 0, warnings = 0;

    // ---- Mobile suitability ----
    printf("--- Mobile suitability ---\n");
    if (fileSize > 500 * 1024)
        printf("%s  %.1f KB — >500 KB, too big for mobile\n", FAIL, fileSize / 1024.0f), ++errors;
    else if (fileSize > 200 * 1024)
        printf("%s  %.1f KB — >200 KB, may cause slow load\n", WARN, fileSize / 1024.0f), ++warnings;

    // ---- Load ----
    gltf::LoadResult r;
    bool ok = gltf::load(data.data(), data.size(), r);
    if (!ok) {
        printf("FAIL: gltf::load returned false\n");
        return 1;
    }

    size_t verts = r.mesh.positions.size();
    size_t tris  = r.mesh.indices.size() / 3;
    size_t idxCount = r.mesh.indices.size();
    printf("vertices:   %zu\n", verts);
    printf("triangles:  %zu\n", tris);

    if (tris > 3000)
        printf("%s  %zu tris — >3000, expensive for mobile\n", FAIL, tris), ++errors;
    else if (tris > 1000)
        printf("%s  %zu tris — >1000, consider LOD\n", WARN, tris), ++warnings;
    else
        printf("%s  %zu tris — good for mobile\n", PASS, tris);

    // Tri/vert ratio
    if (verts > 0) {
        float ratio = static_cast<float>(tris) / static_cast<float>(verts);
        printf("tri/vert:   %.1f\n", ratio);
        if (ratio < 0.5f)
            printf("%s  very low tri/vert ratio (%.1f) — likely degenerate mesh\n", FAIL, ratio), ++errors;
        else if (ratio > 10.0f)
            printf("%s  very high tri/vert ratio (%.1f) — possibly over-tessellated\n", WARN, ratio), ++warnings;
    }

    // Unreferenced vertices
    if (verts > 0 && idxCount > 0) {
        std::vector<bool> used(verts, false);
        for (size_t i = 0; i < idxCount; ++i)
            if (r.mesh.indices[i] < verts)
                used[r.mesh.indices[i]] = true;
        size_t unused = 0;
        for (size_t i = 0; i < verts; ++i)
            if (!used[i]) ++unused;
        if (unused > 0)
            printf("%s  %zu/%zu vertices unreferenced by any index — wasted GPU memory\n",
                   WARN, unused, verts), ++warnings;
    }

    // Degenerate triangles
    if (idxCount >= 3) {
        size_t degen = 0;
        for (size_t i = 0; i + 2 < idxCount; i += 3) {
            uint16_t a = r.mesh.indices[i], b = r.mesh.indices[i+1], c = r.mesh.indices[i+2];
            if (a == b || b == c || a == c) ++degen;
        }
        if (degen > 0)
            printf("%s  %zu degenerate triangles (duplicate indices) — wasted fill rate\n",
                   WARN, degen), ++warnings;
    }

    bool hasNormals = !r.mesh.normals.empty();
    bool hasUVs     = !r.mesh.uvs.empty();
    printf("normals:    %s\n", hasNormals ? "yes" : "no");
    printf("UVs:        %s\n", hasUVs ? "yes" : "no");
    if (!hasNormals) printf("%s  no normals — lighting will be flat\n", WARN), ++warnings;

    // ---- Scale & pivot against game grid ----
    printf("\n--- Grid fit (cellSize=%.2f) ---\n", cellSize);
    if (verts > 0) {
        math::Vec3 bmin = r.mesh.positions[0], bmax = r.mesh.positions[0];
        for (size_t i = 1; i < verts; ++i) {
            bmin.x = std::min(bmin.x, r.mesh.positions[i].x);
            bmin.y = std::min(bmin.y, r.mesh.positions[i].y);
            bmin.z = std::min(bmin.z, r.mesh.positions[i].z);
            bmax.x = std::max(bmax.x, r.mesh.positions[i].x);
            bmax.y = std::max(bmax.y, r.mesh.positions[i].y);
            bmax.z = std::max(bmax.z, r.mesh.positions[i].z);
        }
        math::Vec3 ext  = bmax - bmin;
        math::Vec3 ctr  = (bmin + bmax) * 0.5f;
        printf("bbox:       (%.2f,%.2f,%.2f) - (%.2f,%.2f,%.2f)\n",
               bmin.x, bmin.y, bmin.z, bmax.x, bmax.y, bmax.z);
        printf("extent:     (%.2f, %.2f, %.2f)\n", ext.x, ext.y, ext.z);
        printf("center:     (%.2f, %.2f, %.2f)\n", ctr.x, ctr.y, ctr.z);

        // Width/depth fit in cell
        float maxHD = std::max(ext.x, ext.z);
        if (maxHD > maxCellSpan)
            printf("%s  %.2f wide (max XZ=%.2f) — >%.2f (%.1f cells), overlaps adjacent cells\n",
                   FAIL, maxHD, ext.x > ext.z ? ext.x : ext.z, maxCellSpan, maxHD / cellSize), ++errors;
        else if (maxHD > cellSize)
            printf("%s  %.2f wide — >%.2f fits but tight, may clip into neighbor\n",
                   WARN, maxHD, cellSize), ++warnings;
        else
            printf("%s  %.2f wide — fits within one cell\n", PASS, maxHD);

        // Height check
        if (ext.y > 3.0f)
            printf("%s  %.2f tall — >3.0, may clip above camera view\n", FAIL, ext.y), ++errors;
        else if (ext.y > 2.0f)
            printf("%s  %.2f tall — >2.0, tall for camera framing\n", WARN, ext.y), ++warnings;
        else
            printf("%s  %.2f tall — good\n", PASS, ext.y);

        // Pivot centered in XZ?
        float centerOffsetXZ = std::sqrt(ctr.x * ctr.x + ctr.z * ctr.z);
        if (centerOffsetXZ > maxCellSpan * 0.5f)
            printf("%s  pivot offset %.2f from origin in XZ — model will not center on cell\n",
                   FAIL, centerOffsetXZ), ++errors;
        else if (centerOffsetXZ > 0.25f)
            printf("%s  pivot offset %.2f from origin in XZ — slightly off-center\n",
                   WARN, centerOffsetXZ), ++warnings;
        else
            printf("%s  pivot centered at origin in XZ (offset %.2f)\n", PASS, centerOffsetXZ);

        // Feet on ground?
        if (bmin.y > 0.1f)
            printf("%s  feet at Y=%.2f > 0.1 — model floats above ground\n", WARN, bmin.y), ++warnings;
        else if (bmin.y < -0.1f)
            printf("%s  feet at Y=%.2f < -0.1 — model sinks below ground\n", WARN, bmin.y), ++warnings;
        else
            printf("%s  feet on ground (Y=%.2f)\n", PASS, bmin.y);
    }

    // ---- Rigging ----
    printf("\n--- Rigging ---\n");
    size_t bones = r.skeleton.bones.size();
    printf("bones:      %zu\n", bones);

    bool hasBoneIndices = !r.mesh.boneIndices.empty();
    bool hasBoneWeights = !r.mesh.boneWeights.empty();
    printf("bone idx/vert: %s\n", hasBoneIndices ? "yes" : "no");
    printf("bone wgt/vert: %s\n", hasBoneWeights ? "yes" : "no");

    if (bones > 0 && !hasBoneIndices)
        printf("%s  has skeleton but no per-vertex bone indices — skinning won't work\n", FAIL), ++errors;

    if (bones > 0 && !hasBoneWeights)
        printf("%s  has skeleton but no per-vertex bone weights — skinning won't work\n", FAIL), ++errors;

    if (bones == 0 && (hasBoneIndices || hasBoneWeights)) {
        printf("%s  bone data present but skeleton has 0 bones\n", FAIL), ++errors;
    }

    if (bones > 64)
        printf("%s  %zu bones — >64, may exceed uniform buffer limits\n", WARN, bones), ++warnings;
    else if (bones > 0)
        printf("%s  %zu bones — reasonable\n", PASS, bones);

    // Validate bone weights
    if (hasBoneWeights) {
        size_t badSum = 0;
        size_t zeroWeightVerts = 0;
        for (size_t i = 0; i < verts; ++i) {
            float sum = 0;
            int activeWeights = 0;
            for (int j = 0; j < 4; ++j) {
                float w = r.mesh.boneWeights[i][j];
                sum += w;
                if (w > 1e-4f) ++activeWeights;
            }
            if (std::abs(sum - 1.0f) > 1e-3f)
                ++badSum;
            if (activeWeights == 0)
                ++zeroWeightVerts;
        }
        if (badSum > 0)
            printf("%s  %zu/%zu verts have bone weights not summing to 1\n",
                   FAIL, badSum, verts), ++errors;
        else
            printf("%s  all bone weights sum to 1\n", PASS);
        if (zeroWeightVerts > 0)
            printf("%s  %zu/%zu verts have zero bone weight — unskinned, won't follow skeleton\n",
                   WARN, zeroWeightVerts, verts), ++warnings;
    }

    // Validate bone index range
    if (hasBoneIndices && bones > 0) {
        size_t outOfRange = 0;
        for (size_t i = 0; i < verts; ++i)
            for (int j = 0; j < 4; ++j)
                if (r.mesh.boneIndices[i][j] >= bones)
                    ++outOfRange;
        if (outOfRange > 0)
            printf("%s  %zu bone indices out of range (>= %zu bones)\n",
                   FAIL, outOfRange, bones), ++errors;
        else
            printf("%s  all bone indices in range\n", PASS);
    }

    // Validate bone parent hierarchy
    if (bones > 0) {
        int badParent = 0;
        bool hasRoot = false;
        for (size_t i = 0; i < bones; ++i) {
            int p = r.skeleton.bones[i].parent;
            if (p == -1) hasRoot = true;
            else if (p >= static_cast<int>(bones)) {
                printf("%s  bone[%zu] parent=%d out of range\n", FAIL, i, p);
                ++badParent;
            }
        }
        if (!hasRoot)
            printf("%s  no root bone (all bones have a parent)\n", WARN), ++warnings;
        if (badParent == 0)
            printf("%s  bone parent hierarchy valid\n", PASS);
    }

    // ---- Animation clips ----
    printf("\n--- Animation clips ---\n");
    printf("clips:      %zu\n", r.clips.size());

    // Warn if rigged but no clips
    if (bones > 0 && r.clips.empty())
        printf("%s  rigged model with no animation clips — will be T-pose\n", WARN), ++warnings;

    for (auto& [name, clip] : r.clips) {
        printf("  '%s'  loop=%s  tracks=%zu\n",
               name.c_str(), clip.loop ? "yes" : "no", clip.tracks.size());
        for (auto& t : clip.tracks) {
            bool boneValid = t.boneIndex >= 0 && static_cast<size_t>(t.boneIndex) < bones;
            printf("    track bone=%d %s  keyframes=%zu",
                   t.boneIndex, boneValid ? "" : "[OUT OF RANGE]", t.keyframes.size());
            // Check auto-clamp keyframes identity
            if (t.keyframes.size() >= 2) {
                const auto& first = t.keyframes.front();
                const auto& last  = t.keyframes.back();
                if (std::abs(last.time - first.time) < 1e-6f)
                    printf("  [ZERO DURATION]");
            }
            printf("\n");
            if (!boneValid && bones > 0)
                printf("%s  track bone=%d out of range (max %zu)\n",
                       FAIL, t.boneIndex, bones - 1), ++errors;
        }
    }

    // ---- SUMMARY ----
    printf("\n=== SUMMARY ===\n");
    if (errors > 0)
        printf("%s  %d critical issue(s) — not suitable\n", FAIL, errors);
    else if (warnings > 0)
        printf("%s  %d warnings — usable, review\n", WARN, warnings);
    else
        printf("%s  no issues — ready for game\n", PASS);

    return errors > 0 ? 1 : 0;
}
