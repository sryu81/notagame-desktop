#include "LevelLoader.h"
#include "AndroidOut.h"
#include <cstdio>
#include <cmath>

int main() {
    const char* path = "/home/soo/AndroidStudioProjects/Not_A_Game/app/src/main/assets/level_01.lvl";
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "open failed: %s\n", path); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* data = new unsigned char[size];
    fread(data, 1, size, f);
    fclose(f);

    level::Level lvl;
    bool ok = level::load(data, size, lvl);
    fprintf(stderr, "load() returned %s\n", ok ? "true" : "false");
    if (!ok) { delete[] data; return 2; }

    fprintf(stderr, "grid %ux%u cellSize=%.2f\n", lvl.width, lvl.height, lvl.cellSize);
    fprintf(stderr, "cells=%zu (terrain+Y)\n", lvl.terrain.size());
    int blocked = 0;
    for (auto t : lvl.terrain) if (t == 0) ++blocked;
    fprintf(stderr, "blocked cells: %d\n", blocked);

    int impassableIdx[] = {6*10+4, 6*10+5, 7*10+4, 7*10+5};
    for (int i : impassableIdx) {
        if (lvl.terrain[i] != 0) {
            fprintf(stderr, "FAIL: cell idx %d should be blocked\n", i);
            delete[] data; return 3;
        }
    }
    if (lvl.terrain[0] != 1) { fprintf(stderr, "FAIL: cell 0 not passable\n"); delete[] data; return 4; }

    fprintf(stderr, "units=%zu\n", lvl.units.size());
    for (size_t i = 0; i < lvl.units.size(); ++i) {
        auto& u = lvl.units[i];
        const char* fn = "?";
        switch (u.faction) {
            case 0: fn = "Player"; break;
            case 1: fn = "Enemy";  break;
            case 2: fn = "Ally";   break;
            case 3: fn = "Neutral"; break;
        }
        fprintf(stderr, "  [%zu] faction=%s pos=(%u,%u) skinned=%d stats={hp=%d atk=%d def=%d mov=%d range=%d}\n",
                i, fn, u.x, u.y, (int)u.skinned,
                u.maxHp, u.atk, u.def, u.mov, u.range);
        fprintf(stderr, "       base=(%.2f,%.2f,%.2f) sel=(%.2f,%.2f,%.2f)\n",
                u.baseColor.x, u.baseColor.y, u.baseColor.z,
                u.selectedColor.x, u.selectedColor.y, u.selectedColor.z);
    }

    if (lvl.units.size() != 3) { fprintf(stderr, "FAIL: expected 3 units\n"); delete[] data; return 5; }
    if (lvl.units[0].faction != 0 || lvl.units[0].x != 2 || lvl.units[0].y != 7) {
        fprintf(stderr, "FAIL: unit[0] mismatch\n"); delete[] data; return 6;
    }
    if (lvl.units[0].maxHp != 12 || lvl.units[0].atk != 6 ||
        lvl.units[0].def != 4 || lvl.units[0].mov != 4 ||
        lvl.units[0].range != 1) {
        fprintf(stderr, "FAIL: unit[0] stats mismatch\n"); delete[] data; return 7;
    }
    if (lvl.units[1].faction != 2 || lvl.units[1].x != 5 || lvl.units[1].y != 3 ||
        lvl.units[1].skinned) {
        fprintf(stderr, "FAIL: unit[1] mismatch\n"); delete[] data; return 8;
    }
    if (lvl.units[1].maxHp != 8 || lvl.units[1].atk != 4 ||
        lvl.units[1].def != 3 || lvl.units[1].mov != 3 ||
        lvl.units[1].range != 2) {
        fprintf(stderr, "FAIL: unit[1] stats mismatch\n"); delete[] data; return 9;
    }
    if (lvl.units[2].faction != 1 || !lvl.units[2].skinned) {
        fprintf(stderr, "FAIL: unit[2] mismatch\n"); delete[] data; return 10;
    }
    if (lvl.units[2].maxHp != 9 || lvl.units[2].range != 1) {
        fprintf(stderr, "FAIL: unit[2] stats mismatch\n"); delete[] data; return 11;
    }

    fprintf(stderr, "props=%zu\n", lvl.props.size());
    if (lvl.props.size() != 0) { fprintf(stderr, "FAIL: expected 0 props\n"); delete[] data; return 12; }

    fprintf(stderr, "ALL PASS\n");
    delete[] data;
    return 0;
}
