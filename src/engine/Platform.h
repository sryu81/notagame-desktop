#ifndef NOT_A_GAME_PLATFORM_H
#define NOT_A_GAME_PLATFORM_H

#include <cstdint>
#include <string>
#include <vector>

class Platform {
public:
    virtual ~Platform() = default;

    virtual bool readAsset(const std::string& path, std::vector<uint8_t>& out) const = 0;
    virtual std::string dataPath() const = 0;
    virtual void framebufferSize(int& width, int& height) const = 0;
    virtual void swapBuffers() = 0;
    virtual void requestQuit() = 0;
};

#endif //NOT_A_GAME_PLATFORM_H
