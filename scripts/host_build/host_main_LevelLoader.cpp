#include "LevelLoader.h"
#include "AndroidOut.h"
#include <cstdio>
int main() {
    fprintf(stderr, "Running level::selfTest()\n");
    level::selfTest();
    fprintf(stderr, "DONE\n");
    return 0;
}
