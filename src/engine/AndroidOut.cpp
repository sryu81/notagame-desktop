#include "AndroidOut.h"

#include <cstdio>

int AndroidOut::sync() {
    std::fprintf(stderr, "%s: %s", logTag_, str().c_str());
    std::fflush(stderr);
    str("");
    return 0;
}

AndroidOut androidOut("AO");
std::ostream aout(&androidOut);
