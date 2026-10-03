#include <GLFW/glfw3.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "Action.h"
#include "BattleGrid.h"
#include "Climate.h"
#include "Engine.h"
#include "Path.h"
#include "Prop.h"
#include "AILab.h"
#include "AnimLab.h"
#include "BattleScene.h"
#include "CameraLab.h"
#include "FxLab.h"
#include "HubScene.h"
#include "RenderLab.h"
#include "SystemsLab.h"
#include "LevelLab.h"
#include "TacticsLab.h"
#include "UnitLab.h"
#include "Rules.h"
#include "AI.h"
#include "BattleFlow.h"
#include "BattleFx.h"
#include "LevelSpawner.h"
#include "TitleScene.h"
#include "desktop/PngWriter.h"
#include "desktop/ScriptRunner.h"
#include "Turn.h"
#include "Unit.h"
#include "UnitCatalog.h"
#include "game/EntityBuilder.h"
#include "game/DebugPanel.h"
#include "game/GameState.h"
#include "game/LabChecklist.h"
#include "game/ClipEvents.h"
#include "game/FootIKRig.h"

namespace fs = std::filesystem;

class DesktopPlatform : public Platform {
public:
    DesktopPlatform(GLFWwindow* window, fs::path assetRoot, fs::path dataRoot)
            : window_(window), assetRoot_(std::move(assetRoot)), dataRoot_(std::move(dataRoot)) {}

    bool readAsset(const std::string& path, std::vector<uint8_t>& out) const override {
        std::ifstream in(assetRoot_ / path, std::ios::binary | std::ios::ate);
        if (!in) return false;
        const std::streamsize size = in.tellg();
        in.seekg(0);
        out.resize(static_cast<size_t>(size));
        return static_cast<bool>(in.read(reinterpret_cast<char*>(out.data()), size));
    }

    std::string dataPath() const override { return dataRoot_.string(); }

    void framebufferSize(int& width, int& height) const override {
        glfwGetFramebufferSize(window_, &width, &height);
    }

    void swapBuffers() override {
        if (!pendingShot_.empty()) {
            capture(pendingShot_);
            pendingShot_.clear();
        }
        glfwSwapBuffers(window_);
    }

    void requestScreenshot(const std::string& path) { pendingShot_ = path; }

    void requestQuit() override { glfwSetWindowShouldClose(window_, GLFW_TRUE); }

private:
    void capture(const std::string& path) const {
        int w = 0, h = 0;
        glfwGetFramebufferSize(window_, &w, &h);
        std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        std::vector<uint8_t> flipped(px.size());
        const size_t row = static_cast<size_t>(w) * 4;
        for (int y = 0; y < h; ++y)
            std::copy(px.begin() + static_cast<long>(y * row), px.begin() + static_cast<long>((y + 1) * row),
                      flipped.begin() + static_cast<long>((h - 1 - y) * row));
        for (size_t i = 3; i < flipped.size(); i += 4) flipped[i] = 255;
        const bool ok = desktop::writePngRgba(path, w, h, flipped);
        aout << "[script] shot " << path << (ok ? "" : " FAILED") << std::endl;
    }

    std::string pendingShot_;
    GLFWwindow* window_;
    fs::path assetRoot_;
    fs::path dataRoot_;
};

namespace {

struct InputState {
    Renderer* renderer = nullptr;
    bool mouseDown = false;
};

void cursorToFramebuffer(GLFWwindow* window, double& x, double& y) {
    int ww, wh, fw, fh;
    glfwGetWindowSize(window, &ww, &wh);
    glfwGetFramebufferSize(window, &fw, &fh);
    if (ww > 0 && wh > 0) {
        x *= static_cast<double>(fw) / ww;
        y *= static_cast<double>(fh) / wh;
    }
}

InputState& input(GLFWwindow* window) {
    return *static_cast<InputState*>(glfwGetWindowUserPointer(window));
}

void onMouseButton(GLFWwindow* window, int button, int action, int) {
    auto& in = input(window);
    if (!in.renderer || button != GLFW_MOUSE_BUTTON_LEFT) return;
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    cursorToFramebuffer(window, x, y);
    if (action == GLFW_PRESS) {
        in.mouseDown = true;
        in.renderer->pointerDown(0, static_cast<float>(x), static_cast<float>(y));
    } else if (action == GLFW_RELEASE && in.mouseDown) {
        in.mouseDown = false;
        in.renderer->pointerUp(0, static_cast<float>(x), static_cast<float>(y));
    }
}

void onCursorPos(GLFWwindow* window, double x, double y) {
    auto& in = input(window);
    if (!in.renderer || !in.mouseDown) return;
    cursorToFramebuffer(window, x, y);
    in.renderer->pointerMove(0, static_cast<float>(x), static_cast<float>(y));
}

void onScroll(GLFWwindow* window, double, double dy) {
    auto& in = input(window);
    if (in.renderer) in.renderer->scroll(static_cast<float>(dy));
}

void onKey(GLFWwindow* window, int key, int, int action, int) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return;
    }
    if (action == GLFW_PRESS)        EventBus::emit<KeyDownEvent>(key);
    else if (action == GLFW_RELEASE) EventBus::emit<KeyUpEvent>(key);
}

fs::path defaultDataRoot() {
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg) return fs::path(xdg) / "notagame";
    if (const char* home = std::getenv("HOME"); home && *home) return fs::path(home) / ".local/share/notagame";
    return fs::current_path() / "save";
}

std::unique_ptr<Scene> makeStartScene(const std::string& name, game::GameState& gs) {
    if (name == "hub")        return std::make_unique<HubScene>(gs);
    if (name == "battle")     return std::make_unique<BattleScene>(gs);
    if (name == "renderlab")  return std::make_unique<RenderLab>(gs);
    if (name == "animlab")    return std::make_unique<AnimLab>(gs);
    if (name == "fxlab")      return std::make_unique<FxLab>(gs);
    if (name == "cameralab")  return std::make_unique<CameraLab>(gs);
    if (name == "systemslab") return std::make_unique<SystemsLab>(gs);
    if (name == "unitlab")    return std::make_unique<UnitLab>(gs);
    if (name == "tacticslab") return std::make_unique<TacticsLab>(gs);
    if (name == "levellab")   return std::make_unique<LevelLab>(gs);
    if (name == "ailab")      return std::make_unique<AILab>(gs);
    if (name != "title") aout << "[desktop] unknown --start '" << name << "', using title" << std::endl;
    return std::make_unique<TitleScene>(gs);
}

fs::path defaultAssetRoot(const char* argv0) {
    std::error_code ec;
    fs::path exe = fs::canonical("/proc/self/exe", ec);
    if (ec) exe = fs::absolute(argv0);
    return exe.parent_path() / "assets";
}

} // namespace

int main(int argc, char** argv) {
    fs::path assetRoot = defaultAssetRoot(argv[0]);
    fs::path dataRoot  = defaultDataRoot();
    std::string scriptPath;
    std::string startScene = "title";
    for (int i = 1; i + 1 < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--assets")      assetRoot  = argv[++i];
        else if (arg == "--data")   dataRoot   = argv[++i];
        else if (arg == "--script") scriptPath = argv[++i];
        else if (arg == "--start")  startScene = argv[++i];
    }
    fs::create_directories(dataRoot);

    glfwSetErrorCallback([](int code, const char* desc) {
        aout << "[glfw] error " << code << ": " << desc << std::endl;
    });
    if (!glfwInit()) return EXIT_FAILURE;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_EGL_CONTEXT_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Not_A_Game", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return EXIT_FAILURE;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    DesktopPlatform platform(window, assetRoot, dataRoot);
    aout << "[desktop] assets=" << assetRoot << " data=" << dataRoot << std::endl;

    math::selfTest();
    Camera::selfTest();
    Mesh::selfTest();
    primitives::selfTest();
    lighting::selfTest();
    EventBus::selfTest();
    input::selfTest();
    audio::selfTest();
    picking::selfTest();
    SceneNode::selfTest();
    meshdata::selfTest();
    anim::selfTest();
    grid::BattleGrid::selfTest();
    path::selfTest();
    rules::selfTest();
    ai::selfTest();
    flow::selfTest();
    fx::selfTest();
    turn::Manager::selfTest();
    action::Manager::selfTest();
    save::selfTest(platform.dataPath().c_str());
    gltf::selfTest();
    level::selfTest();
    particle::Emitter::selfTest();
    engine::stars::selfTest();
    engine::sky::selfTest();
    engine::wx::selfTest();
    ui::UIRoot::selfTest();
    asset::Registry::selfTest();
    SceneManager::selfTest();
    anim::skinningSelfTest();
    anim::skeletalSelfTest();
    engine::foot_ik::selfTest();
    engine::clock::Clock::selfTest();
    game::ClimateState::selfTest();
    engine::scenegraph::selfTest();
    engine::scenegraph::spawnSelfTest();
    engine::catalogSelfTest();
    engine::bagSelfTest();
    engine::equipSlotsSelfTest();
    engine::saveableSelfTest();
    engine::TileGrid::selfTest();
    game::UnitCatalog::selfTest();
    game::Unit::selfTest();
    game::Prop::selfTest();
    game::EntityBuilder::selfTest();
    game::LevelSpawner::selfTest();
    game::EntityRegistry::selfTest();
    game::GameState::selfTest();
    game::DebugPanel::selfTest();
    game::LabChecklist::selfTest();
    game::clipEventsSelfTest();
    game::FootIKRig::selfTest();
    desktop::ScriptRunner::selfTest();
    desktop::pngSelfTest();

    desktop::ScriptRunner script;
    if (!scriptPath.empty() && !script.load(scriptPath)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    InputState inputState;
    glfwSetWindowUserPointer(window, &inputState);
    glfwSetMouseButtonCallback(window, onMouseButton);
    glfwSetCursorPosCallback(window, onCursorPos);
    glfwSetScrollCallback(window, onScroll);
    glfwSetKeyCallback(window, onKey);

    {
        auto gs = std::make_shared<game::GameState>();
        gs->initDefaultRoster(3);
        Renderer renderer(platform, [gs, startScene]() { return makeStartScene(startScene, *gs); });
        inputState.renderer = &renderer;

        int frame = 0;
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            script.step(frame, renderer,
                        [&platform](const std::string& path) { platform.requestScreenshot(path); },
                        [window]() { glfwSetWindowShouldClose(window, GLFW_TRUE); });
            renderer.render();
            ++frame;
        }
        inputState.renderer = nullptr;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
