#include "GltfLoader.h"
#include "AndroidOut.h"
#include <cstdio>
int main() {
    fprintf(stderr, "Running gltf::selfTest()\n");
    gltf::selfTest();
    fprintf(stderr, "DONE\n");
    return 0;
}
