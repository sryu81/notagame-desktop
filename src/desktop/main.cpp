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
#include "TitleScene.h"
#include "Turn.h"
#include "Unit.h"
#include "UnitCatalog.h"
#include "game/EntityBuilder.h"
#include "game/GameState.h"

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

    void swapBuffers() override { glfwSwapBuffers(window_); }

    void requestQuit() override { glfwSetWindowShouldClose(window_, GLFW_TRUE); }

private:
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
    for (int i = 1; i + 1 < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--assets")    assetRoot = argv[++i];
        else if (arg == "--data") dataRoot  = argv[++i];
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
    turn::Manager::selfTest();
    action::Manager::selfTest();
    save::selfTest(platform.dataPath().c_str());
    gltf::selfTest();
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
    game::EntityRegistry::selfTest();
    game::GameState::selfTest();

    InputState inputState;
    glfwSetWindowUserPointer(window, &inputState);
    glfwSetMouseButtonCallback(window, onMouseButton);
    glfwSetCursorPosCallback(window, onCursorPos);
    glfwSetScrollCallback(window, onScroll);
    glfwSetKeyCallback(window, onKey);

    {
        auto gs = std::make_shared<game::GameState>();
        gs->initDefaultRoster(3);
        Renderer renderer(platform, [gs]() {
            return std::unique_ptr<Scene>(new TitleScene(*gs));
        });
        inputState.renderer = &renderer;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            renderer.render();
        }
        inputState.renderer = nullptr;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
