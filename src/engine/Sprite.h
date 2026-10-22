#ifndef NOT_A_GAME_SPRITE_H
#define NOT_A_GAME_SPRITE_H

#include <string>
#include <vector>

#include "Animation.h"
#include "Math.h"
#include "Mesh.h"

namespace sprite {

enum Facing { kFront = 0, kRight = 1, kBack = 2, kLeft = 3 };

struct Clip {
    std::string name;
    int frames = 1;
    float fps = 8.0f;
    bool loop = true;
    int row = 0;
    std::vector<anim::AnimEvent> events;

    float duration() const { return fps > 0.0f ? static_cast<float>(frames) / fps : 0.0f; }
};

struct Sheet {
    int frameW = 0;
    int frameH = 0;
    float pixelsPerUnit = 1.0f;
    float anchorX = 0.0f;
    float anchorY = 0.0f;
    int dirs = 4;
    int columns = 0;
    int atlasW = 0;
    int atlasH = 0;
    std::vector<Clip> clips;

    const Clip* find(const std::string& name) const;
    int rowsNeeded() const;
    float anchorV() const { return frameH > 0 ? 1.0f - anchorY / static_cast<float>(frameH) : 0.0f; }
    float worldW() const { return static_cast<float>(frameW) / pixelsPerUnit; }
    float worldH() const { return static_cast<float>(frameH) / pixelsPerUnit; }
};

bool parse(const std::string& text, Sheet& out, std::string* error = nullptr);
bool fitsAtlas(const Sheet& s, int atlasW, int atlasH);

struct View {
    int row = 0;
    bool mirror = false;
};

int facingIndex(float unitYawDeg, float cameraYawDeg);
View viewFor(const Sheet& s, int facing);

math::Vec4 frameRect(const Sheet& s, const Clip& c, int frame, const View& v);

class Player {
public:
    void play(const Clip* clip);
    void update(float dt, std::vector<int>* events = nullptr);
    const Clip* clip() const { return clip_; }
    float time() const { return time_; }
    int frame() const;
    bool finished() const;
    void setRate(float r) { rate_ = r; }

private:
    const Clip* clip_ = nullptr;
    float time_ = 0.0f;
    float rate_ = 1.0f;
};

float cameraYawDeg(const math::Mat4& view);
float cameraPitchDeg(const math::Mat4& view);
math::Mat4 billboard(const Sheet& s, const math::Vec3& feet, float cameraYawDegrees,
                     float tiltDegrees = 0.0f, float foldPitchDegrees = 0.0f, float scale = 1.0f);
math::Vec3 snapToTexels(const math::Vec3& p, float pixelsPerUnit);

Mesh makeQuad(float anchorV = 0.0f);

struct ScreenRect {
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
    bool valid = false;
    bool contains(float x, float y) const { return valid && x >= x0 && x <= x1 && y >= y0 && y <= y1; }
};
ScreenRect screenRect(const math::Mat4& model, const math::Mat4& viewProj, int width, int height,
                      float anchorV = 0.0f);

void selfTest();

} // namespace sprite

#endif
