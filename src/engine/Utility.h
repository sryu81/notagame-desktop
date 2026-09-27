#ifndef NOT_A_GAME_UTILITY_H
#define NOT_A_GAME_UTILITY_H

#include <cassert>

class Utility {
public:
    static bool checkAndLogGlError(bool alwaysLog = false);

    static inline void assertGlError() { assert(checkAndLogGlError()); }
};

#endif //NOT_A_GAME_UTILITY_H
