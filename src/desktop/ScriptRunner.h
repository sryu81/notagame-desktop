#ifndef NOT_A_GAME_SCRIPT_RUNNER_H
#define NOT_A_GAME_SCRIPT_RUNNER_H

#include <string>
#include <vector>

class Renderer;

namespace desktop {

class ScriptRunner {
public:
    enum class Kind { Tap, Down, Move, Up, Hover, Key, Shot, Quit };
    struct Command {
        int frame = 0;
        Kind kind = Kind::Tap;
        float x = 0.0f, y = 0.0f;
        int key = 0;
        std::string path;
    };

    bool load(const std::string& file);
    static bool parseLine(const std::string& line, Command& out, std::string& error);

    template <typename ShotFn, typename QuitFn>
    void step(int frame, Renderer& r, ShotFn shot, QuitFn quit);

    bool empty() const { return commands_.empty(); }
    size_t size() const { return commands_.size(); }
    bool done() const { return next_ >= commands_.size(); }

    static void selfTest();

private:
    void dispatch(const Command& c, Renderer& r);

    std::vector<Command> commands_;
    size_t next_ = 0;
};

template <typename ShotFn, typename QuitFn>
void ScriptRunner::step(int frame, Renderer& r, ShotFn shot, QuitFn quit) {
    while (next_ < commands_.size() && commands_[next_].frame <= frame) {
        const Command& c = commands_[next_++];
        if (c.kind == Kind::Shot) shot(c.path);
        else if (c.kind == Kind::Quit) quit();
        else dispatch(c, r);
    }
}

} // namespace desktop

#endif
