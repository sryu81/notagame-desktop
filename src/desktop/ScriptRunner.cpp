#include "ScriptRunner.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <sstream>

#include "Engine.h"

namespace desktop {

bool ScriptRunner::parseLine(const std::string& line, Command& out, std::string& error) {
    std::istringstream in(line);
    std::string cmd;
    if (!(in >> out.frame >> cmd)) {
        error = "expected '<frame> <command>'";
        return false;
    }
    if (out.frame < 0) {
        error = "negative frame";
        return false;
    }
    auto needXY = [&]() {
        if (in >> out.x >> out.y) return true;
        error = cmd + " needs x y";
        return false;
    };
    if (cmd == "tap")       { out.kind = Kind::Tap;  return needXY(); }
    if (cmd == "down")      { out.kind = Kind::Down; return needXY(); }
    if (cmd == "move")      { out.kind = Kind::Move; return needXY(); }
    if (cmd == "up")        { out.kind = Kind::Up;   return needXY(); }
    if (cmd == "key") {
        out.kind = Kind::Key;
        if (in >> out.key) return true;
        error = "key needs a key code";
        return false;
    }
    if (cmd == "shot") {
        out.kind = Kind::Shot;
        if (in >> out.path) return true;
        error = "shot needs a file name";
        return false;
    }
    if (cmd == "quit") { out.kind = Kind::Quit; return true; }
    error = "unknown command '" + cmd + "'";
    return false;
}

bool ScriptRunner::load(const std::string& file) {
    std::ifstream in(file);
    if (!in) {
        aout << "[script] cannot open " << file << std::endl;
        return false;
    }
    commands_.clear();
    next_ = 0;
    std::string line;
    int lineNo = 0;
    bool ok = true;
    while (std::getline(in, line)) {
        ++lineNo;
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        Command c;
        std::string error;
        if (!parseLine(line, c, error)) {
            aout << "[script] " << file << ":" << lineNo << ": " << error << std::endl;
            ok = false;
            continue;
        }
        commands_.push_back(c);
    }
    std::stable_sort(commands_.begin(), commands_.end(),
                     [](const Command& a, const Command& b) { return a.frame < b.frame; });
    aout << "[script] loaded " << commands_.size() << " commands from " << file << std::endl;
    return ok;
}

void ScriptRunner::dispatch(const Command& c, Renderer& r) {
    switch (c.kind) {
        case Kind::Tap:
            r.pointerDown(0, c.x, c.y);
            r.pointerUp(0, c.x, c.y);
            break;
        case Kind::Down: r.pointerDown(0, c.x, c.y); break;
        case Kind::Move: r.pointerMove(0, c.x, c.y); break;
        case Kind::Up:   r.pointerUp(0, c.x, c.y); break;
        case Kind::Key:  EventBus::emit<KeyDownEvent>(c.key); break;
        default: break;
    }
}

void ScriptRunner::selfTest() {
    aout << "[script] self-test starting" << std::endl;
    Command c;
    std::string err;
    assert(parseLine("30 tap 640 360", c, err) && c.frame == 30 && c.kind == Kind::Tap && c.x == 640.0f);
    assert(parseLine("5 move 1.5 2", c, err) && c.kind == Kind::Move && c.y == 2.0f);
    assert(parseLine("7 key 51", c, err) && c.kind == Kind::Key && c.key == 51);
    assert(parseLine("9 shot out/a.png", c, err) && c.kind == Kind::Shot && c.path == "out/a.png");
    assert(parseLine("10 quit", c, err) && c.kind == Kind::Quit);
    assert(!parseLine("10 tap 5", c, err));
    assert(!parseLine("10 jump 1 2", c, err));
    assert(!parseLine("tap 1 2", c, err));
    assert(!parseLine("-1 quit", c, err));
    aout << "[script] self-test PASS" << std::endl;
}

} // namespace desktop
