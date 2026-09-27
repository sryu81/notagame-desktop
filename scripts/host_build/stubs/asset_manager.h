#pragma once
#include <iostream>
#include <sstream>
namespace android {
    struct AAssetManager {};
    using AAsset = void*;
    constexpr int AASSET_MODE_BUFFER = 0;
    inline AAsset* AAssetManager_open(void*, const char*, int) { return nullptr; }
    inline const void* AAsset_getBuffer(AAsset*) { return nullptr; }
    inline size_t AAsset_getLength(AAsset*) { return 0; }
    inline void AAsset_close(AAsset*) {}
}
#define AAssetManager_open(...) nullptr
#define AAsset_getBuffer(...) nullptr
#define AAsset_getLength(...) 0
#define AAsset_close(...)
