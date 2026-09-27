#ifndef NOT_A_GAME_ANDROIDOUT_H
#define NOT_A_GAME_ANDROIDOUT_H

#include <sstream>

extern std::ostream aout;

class AndroidOut: public std::stringbuf {
public:
    inline AndroidOut(const char* kLogTag) : logTag_(kLogTag){}

protected:
    int sync() override;

private:
    const char* logTag_;
};

#endif //NOT_A_GAME_ANDROIDOUT_H
