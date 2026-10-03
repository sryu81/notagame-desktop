#include "AndroidOut.h"

#ifdef __ANDROID__
#include <android/log.h>
#else
#include <cstdio>
#endif

int AndroidOut::sync() {
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_DEBUG, logTag_, "%s", str().c_str());
#else
    std::fprintf(stderr, "%s: %s", logTag_, str().c_str());
    std::fflush(stderr);
#endif
    str("");
    return 0;
}

AndroidOut androidOut("AO");
std::ostream aout(&androidOut);
