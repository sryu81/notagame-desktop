#pragma once
#include <iostream>
#include <sstream>
#define ANDROID_LOG_DEBUG 3
inline int __android_log_print(int, const char*, const char*, ...) { return 0; }
extern std::ostream aout;
class AndroidOut : public std::stringbuf {
public:
    inline AndroidOut(const char* tag) : logTag_(tag) {}
protected:
    int sync() override {
        std::cout << "[" << logTag_ << "] " << str();
        str("");
        return 0;
    }
    const char* logTag_;
};
