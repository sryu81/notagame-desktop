#include "Sprite.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <sstream>

#include "AndroidOut.h"

namespace sprite {

const Clip* Sheet::find(const std::string& name) const {
    for (const Clip& c : clips)
        if (c.name == name) return &c;
    return nullptr;
}

int Sheet::rowsNeeded() const {
    int rows = 0;
    for (const Clip& c : clips) rows = std::max(rows, c.row + dirs);
    return rows;
}

bool parse(const std::string& text, Sheet& out, std::string* error) {
    Sheet s;
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    int nextRow = 0;
    const auto fail = [&](const std::string& why) {
        if (error) *error = "line " + std::to_string(lineNo) + ": " + why;
        return false;
    };
    while (std::getline(in, line)) {
        ++lineNo;
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        std::istringstream ls(line);
        std::string key;
        if (!(ls >> key)) continue;
        if (key == "frame") {
            if (!(ls >> s.frameW >> s.frameH) || s.frameW <= 0 || s.frameH <= 0) return fail("bad frame");
        } else if (key == "ppu") {
            if (!(ls >> s.pixelsPerUnit) || s.pixelsPerUnit <= 0.0f) return fail("bad ppu");
        } else if (key == "anchor") {
            if (!(ls >> s.anchorX >> s.anchorY)) return fail("bad anchor");
        } else if (key == "dirs") {
            if (!(ls >> s.dirs) || (s.dirs != 3 && s.dirs != 4 && s.dirs != 1)) return fail("dirs must be 1, 3 or 4");
        } else if (key == "clip") {
            Clip c;
            if (!(ls >> c.name >> c.frames >> c.fps) || c.frames <= 0 || c.fps <= 0.0f) return fail("bad clip");
            c.loop = anim::isLoopClipName(c.name);
            c.row = nextRow;
            nextRow += s.dirs;
            s.columns = std::max(s.columns, c.frames);
            s.clips.push_back(c);
        } else if (key == "event") {
            std::string name;
            anim::AnimEvent ev;
            if (!(ls >> name >> ev.id >> ev.time)) return fail("bad event");
            Clip* c = nullptr;
            for (Clip& k : s.clips)
                if (k.name == name) c = &k;
            if (!c) return fail("event for unknown clip " + name);
            c->events.push_back(ev);
        } else {
            return fail("unknown key " + key);
        }
    }
    if (s.frameW <= 0 || s.clips.empty()) {
        if (error) *error = "missing frame or clips";
        return false;
    }
    out = std::move(s);
    return true;
}

bool fitsAtlas(const Sheet& s, int atlasW, int atlasH) {
    return atlasW >= s.columns * s.frameW && atlasH >= s.rowsNeeded() * s.frameH;
}

int facingIndex(float unitYawDeg, float cameraYawDeg) {
    float rel = std::fmod(unitYawDeg - cameraYawDeg, 360.0f);
    if (rel < 0.0f) rel += 360.0f;
    return static_cast<int>(std::floor(rel / 90.0f + 0.5f)) % 4;
}

View viewFor(const Sheet& s, int facing) {
    if (s.dirs == 1) return {0, false};
    if (s.dirs == 3 && facing == kLeft) return {kRight, true};
    return {facing, false};
}

math::Vec4 frameRect(const Sheet& s, const Clip& c, int frame, const View& v) {
    const float aw = static_cast<float>(s.atlasW > 0 ? s.atlasW : s.columns * s.frameW);
    const float ah = static_cast<float>(s.atlasH > 0 ? s.atlasH : s.rowsNeeded() * s.frameH);
    const float u0 = static_cast<float>(frame * s.frameW) / aw;
    const float u1 = static_cast<float>((frame + 1) * s.frameW) / aw;
    const float v0 = static_cast<float>((c.row + v.row) * s.frameH) / ah;
    const float v1 = static_cast<float>((c.row + v.row + 1) * s.frameH) / ah;
    return v.mirror ? math::Vec4(u1, v0, u0, v1) : math::Vec4(u0, v0, u1, v1);
}

void Player::play(const Clip* clip) {
    clip_ = clip;
    time_ = 0.0f;
}

void Player::update(float dt, std::vector<int>* events) {
    if (!clip_) return;
    const float dur = clip_->duration();
    const float prev = time_;
    float now = time_ + dt * rate_;
    if (clip_->loop) {
        now = dur > 0.0f ? std::fmod(now, dur) : 0.0f;
    } else {
        now = std::min(now, dur);
    }
    if (events && now != prev) anim::crossedEvents(clip_->events, dur, prev, now, *events);
    time_ = now;
}

int Player::frame() const {
    if (!clip_) return 0;
    const int f = static_cast<int>(std::floor(time_ * clip_->fps));
    return clip_->loop ? f % clip_->frames : std::min(f, clip_->frames - 1);
}

bool Player::finished() const {
    return clip_ && !clip_->loop && time_ >= clip_->duration();
}

float cameraYawDeg(const math::Mat4& view) {
    return math::degrees(std::atan2(view.m[2], view.m[10]));
}

float cameraPitchDeg(const math::Mat4& view) {
    return math::degrees(std::asin(std::clamp(view.m[6], -1.0f, 1.0f)));
}

math::Mat4 billboard(const Sheet& s, const math::Vec3& feet, float cameraYawDegrees,
                     float tiltDegrees, float foldPitchDegrees, float scale) {
    const float yaw = math::radians(cameraYawDegrees);
    const float pitch = math::radians(tiltDegrees);
    const math::Vec3 right(std::cos(yaw), 0.0f, -std::sin(yaw));
    const math::Vec3 toward(std::sin(yaw), 0.0f, std::cos(yaw));
    const math::Vec3 normal = toward * std::cos(pitch) + math::Vec3(0.0f, std::sin(pitch), 0.0f);
    const math::Vec3 up = math::Vec3(0.0f, std::cos(pitch), 0.0f) - toward * std::sin(pitch);
    const float w = s.worldW() * scale;
    const float h = s.worldH() * scale;
    const float ax = s.anchorX / s.pixelsPerUnit * scale;
    const float ay = (static_cast<float>(s.frameH) - s.anchorY) / s.pixelsPerUnit * scale;
    const math::Vec3 origin = feet - right * ax - up * ay;
    math::Vec3 fold = normal;
    if (foldPitchDegrees > 0.0f && s.anchorV() > 0.0f) {
        const float view = math::radians(std::max(foldPitchDegrees, 10.0f));
        fold = (up + toward * (std::cos(view - pitch) / std::sin(view))) * h;
    }
    math::Mat4 m = math::Mat4::identity();
    m.m[0] = right.x * w;  m.m[1] = 0.0f;     m.m[2]  = right.z * w;
    m.m[4] = up.x * h;     m.m[5] = up.y * h; m.m[6]  = up.z * h;
    m.m[8] = fold.x;       m.m[9] = fold.y;   m.m[10] = fold.z;
    m.m[12] = origin.x;    m.m[13] = origin.y; m.m[14] = origin.z;
    return m;
}

math::Vec3 snapToTexels(const math::Vec3& p, float pixelsPerUnit) {
    const auto snap = [pixelsPerUnit](float v) { return std::round(v * pixelsPerUnit) / pixelsPerUnit; };
    return {snap(p.x), snap(p.y), snap(p.z)};
}

Mesh makeQuad(float anchorV) {
    Mesh m;
    if (anchorV <= 0.0f) {
        m.positions = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
        m.uvs = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
        m.indices = {0, 1, 2, 0, 2, 3};
    } else {
        const float a = std::min(anchorV, 1.0f);
        m.positions = {{0, 0, a}, {1, 0, a}, {1, a, 0}, {0, a, 0}, {1, 1, 0}, {0, 1, 0}};
        m.uvs = {{0, 1}, {1, 1}, {1, 1 - a}, {0, 1 - a}, {1, 0}, {0, 0}};
        m.indices = {0, 1, 2, 0, 2, 3, 3, 2, 4, 3, 4, 5};
    }
    m.normals.assign(m.positions.size(), math::Vec3(0, 0, 1));
    return m;
}

ScreenRect screenRect(const math::Mat4& model, const math::Mat4& viewProj, int width, int height,
                      float anchorV) {
    ScreenRect r;
    if (width <= 0 || height <= 0) return r;
    const math::Mat4 mvp = viewProj * model;
    r.x0 = r.y0 = 1e30f;
    r.x1 = r.y1 = -1e30f;
    for (const math::Vec3& c : {math::Vec3(0, 0, anchorV), math::Vec3(1, 0, anchorV), math::Vec3(1, 1, 0),
                                math::Vec3(0, 1, 0)}) {
        const math::Vec4 clip = mvp * math::Vec4(c.x, c.y, c.z, 1.0f);
        if (clip.w <= 1e-6f) return {};
        const float sx = (clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(width);
        const float sy = (0.5f - clip.y / clip.w * 0.5f) * static_cast<float>(height);
        r.x0 = std::min(r.x0, sx); r.x1 = std::max(r.x1, sx);
        r.y0 = std::min(r.y0, sy); r.y1 = std::max(r.y1, sy);
    }
    r.valid = true;
    return r;
}

void selfTest() {
    aout << "[sprite] self-test starting" << std::endl;
    const char* text =
        "# test\n"
        "frame 64 64\nppu 48\nanchor 32 62\ndirs 4\n"
        "clip Idle 2 4\nclip Walk 4 8\nclip Attack 4 8\nclip Hurt 2 8\nclip Death 3 6\n"
        "event Attack 1 0.35\n";
    Sheet s;
    std::string err;
    assert(parse(text, s, &err));
    assert(s.clips.size() == 5 && s.columns == 4 && s.rowsNeeded() == 20);
    assert(s.find("Walk")->row == 4 && s.find("Idle")->loop && !s.find("Attack")->loop);
    assert(s.find("Attack")->events.size() == 1 && s.find("Attack")->events[0].id == 1);
    assert(fitsAtlas(s, 256, 1280) && !fitsAtlas(s, 192, 1280) && !fitsAtlas(s, 256, 1216));
    assert(!parse("frame 64 64\nclip Idle 2 4\nevent Walk 1 0.1\n", s, &err));
    assert(!parse("frame 64 64\nbogus 1\n", s, &err));
    assert(parse(text, s));

    assert(facingIndex(0.0f, 0.0f) == kFront);
    assert(facingIndex(90.0f, 0.0f) == kRight);
    assert(facingIndex(180.0f, 0.0f) == kBack);
    assert(facingIndex(-90.0f, 0.0f) == kLeft);
    assert(facingIndex(0.0f, 90.0f) == kLeft);
    assert(facingIndex(30.0f, -30.0f) == kRight);
    Sheet three = s;
    three.dirs = 3;
    assert(viewFor(three, kLeft).row == kRight && viewFor(three, kLeft).mirror);
    assert(viewFor(s, kLeft).row == kLeft && !viewFor(s, kLeft).mirror);

    s.atlasW = 256;
    s.atlasH = 1280;
    const Clip& walk = *s.find("Walk");
    const math::Vec4 r = frameRect(s, walk, 1, {kBack, false});
    assert(math::approxEqual(r, math::Vec4(0.25f, 6.0f / 20.0f, 0.5f, 7.0f / 20.0f), 1e-6f));
    const math::Vec4 m = frameRect(s, walk, 1, {kRight, true});
    assert(m.x > m.z);

    Player p;
    p.play(&walk);
    p.update(0.13f);
    assert(p.frame() == 1);
    p.update(0.4f);
    assert(p.frame() == 0 && !p.finished());
    const Clip& atk = *s.find("Attack");
    p.play(&atk);
    std::vector<int> ev;
    p.update(0.3f, &ev);
    assert(ev.empty());
    p.update(0.1f, &ev);
    assert(ev.size() == 1 && ev[0] == 1);
    p.update(1.0f, &ev);
    assert(ev.size() == 1 && p.finished() && p.frame() == atk.frames - 1);

    const math::Mat4 view = math::Mat4::lookAt(math::Vec3(5, 3, 0), math::Vec3(0, 0, 0), math::Vec3(0, 1, 0));
    assert(math::approxEqual(cameraYawDeg(view), 90.0f, 1e-3f));
    const math::Mat4 bb = billboard(s, math::Vec3(1, 0, 2), 90.0f);
    const math::Vec3 feet = bb.transformPoint(math::Vec3(32.0f / 64.0f, 2.0f / 64.0f, 0.0f));
    assert(math::approxEqual(feet, math::Vec3(1, 0, 2), 1e-5f));
    const math::Vec3 n = bb.transformDirection(math::Vec3(0, 0, 1));
    assert(math::approxEqual(n, math::Vec3(1, 0, 0), 1e-5f));
    assert(math::approxEqual(bb.transformPoint(math::Vec3(0, 1, 0)).y - bb.transformPoint(math::Vec3(0, 0, 0)).y,
                             64.0f / 48.0f, 1e-5f));
    const math::Mat4 view35 = math::Mat4::lookAt(math::Vec3(0, std::sin(math::radians(35.0f)) * 10.0f,
                                                            std::cos(math::radians(35.0f)) * 10.0f),
                                                 math::Vec3(0, 0, 0), math::Vec3(0, 1, 0));
    assert(math::approxEqual(cameraPitchDeg(view35), 35.0f, 1e-3f) && math::approxEqual(cameraYawDeg(view35), 0.0f, 1e-3f));
    const math::Mat4 tilt = billboard(s, math::Vec3(0, 0, 0), 0.0f, 35.0f);
    const math::Vec3 camZ(view35.m[2], view35.m[6], view35.m[10]);
    assert(math::approxEqual(tilt.transformDirection(math::Vec3(0, 0, 1)), camZ, 1e-4f));
    assert(math::approxEqual(tilt.transformPoint(math::Vec3(0.5f, 2.0f / 64.0f, 0)), math::Vec3(0, 0, 0), 1e-5f));
    {
        Sheet low = s;
        low.anchorY = 51.0f;
        const float a = low.anchorV();
        const Mesh q = makeQuad(a);
        assert(q.positions.size() == 6 && q.indices.size() == 12);
        const float p = math::radians(35.0f);
        const math::Vec3 camUp(0.0f, std::cos(p), -std::sin(p));
        for (float tiltDeg : {35.0f, 0.0f}) {
            const math::Mat4 f = billboard(low, math::Vec3(1, 0, 2), 0.0f, tiltDeg, 35.0f);
            const math::Mat4 flat = billboard(low, math::Vec3(1, 0, 2), 0.0f, tiltDeg);
            const math::Vec3 lowFold = f.transformPoint(q.positions[0]);
            const math::Vec3 lowFlat = flat.transformPoint(math::Vec3(0, 0, 0));
            assert(math::approxEqual(lowFold.y, 0.0f, 1e-5f));
            assert(math::approxEqual(math::dot(lowFold - math::Vec3(1, 0, 2), camUp),
                                     math::dot(lowFlat - math::Vec3(1, 0, 2), camUp), 1e-4f));
            assert(math::approxEqual(f.transformPoint(q.positions[5]), flat.transformPoint(q.positions[5]), 1e-5f));
        }
    }
    assert(math::approxEqual(snapToTexels(math::Vec3(0.51f, 0, 0), 2.0f).x, 0.5f));

    const math::Mat4 vp = math::Mat4::perspective(math::radians(60.0f), 1.0f, 0.1f, 100.0f) * view;
    const ScreenRect sr = screenRect(billboard(s, math::Vec3(0, 0, 0), 90.0f), vp, 800, 800);
    assert(sr.valid && sr.contains(400.0f, 380.0f) && !sr.contains(10.0f, 10.0f));
    aout << "[sprite] self-test PASS" << std::endl;
}

} // namespace sprite
