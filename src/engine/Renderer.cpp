#include "Renderer.h"

#include <cassert>
#include <GLES3/gl3.h>
#include <memory>

#include "AndroidOut.h"
#include "Camera.h"
#include "EventBus.h"
#include "Input.h"
#include "Material.h"
#include "Math.h"
#include "Model.h"
#include "TextureAsset.h"
#include "SceneManager.h"
#include "SceneNode.h"
#include "UIRoot.h"
#include "Utility.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <sstream>

#define PRINT_GL_STRING(s) {aout << #s": "<< glGetString(s) << std::endl;}
#define PRINT_GL_STRING_AS_LIST(s) { \
std::istringstream extensionStream((const char *) glGetString(s));\
std::vector<std::string> extensionList(\
        std::istream_iterator<std::string>{extensionStream},\
        std::istream_iterator<std::string>());\
aout << #s":\n";\
for (auto& extension: extensionList) aout << extension << "\n";\
aout << std::endl;}

#define CORNFLOWER_BLUE 100 / 255.f, 149 / 255.f, 237 / 255.f, 1

struct FrameUBO {
    float viewProj[16];
    float lightDirAmbient[4];
};
static_assert(sizeof(FrameUBO) == 80, "FrameUBO layout mismatch");



constexpr float kMusicVolume = 0.3f;

Renderer::Renderer(Platform& platform, SceneFactory initialSceneFactory)
        : platform_(platform),
          width_(0),
          height_(0),
          initialSceneFactory_(std::move(initialSceneFactory)),
          camera_(std::make_unique<Camera>()) {
    initRenderer();
    {
        audio_ = std::make_unique<Audio>();
        if (audio_->init(&platform_)) {
            clickSfx_ = std::make_unique<audio::Sound>();
            if (audio_->loadSound("click.wav", *clickSfx_))
                aout << "[audio] loaded click.wav ("
                     << clickSfx_->samples.size() << " samples)" << std::endl;
            else
                aout << "[audio] failed to load click.wav" << std::endl;
            for (int i = 1; i <= 9; ++i) {
                char name[24];
                snprintf(name, sizeof(name), "music_%02d.wav", i);
                auto track = std::make_unique<audio::Music>();
                if (!audio_->loadMusic(name, *track)) break;
                aout << "[audio] loaded " << name << " (" << track->samples.size() << " samples)" << std::endl;
                musicTracks_.push_back(std::move(track));
            }
            audio::Music musicLoop;
            if (audio_->loadMusic("music.wav", musicLoop))
                musicLoop_ = std::make_unique<audio::Music>(std::move(musicLoop));
            playDefaultMusic();
        } else {
            aout << "[audio] init FAILED (silent)" << std::endl;
        }
    }
}

Renderer::~Renderer() {
    {
        if (sceneManager_ && sceneManager_->current())
            sceneManager_->current()->exit(*this);
        sceneManager_.reset();
        if (frameUbo_) { glDeleteBuffers(1, &frameUbo_); frameUbo_ = 0; }
        particleShader_.reset();
        if (particleVbo_) { glDeleteBuffers(1, &particleVbo_); particleVbo_ = 0; }
        if (particleIbo_) { glDeleteBuffers(1, &particleIbo_); particleIbo_ = 0; }
        shadowShader_.reset();
        shadowSkinnedShader_.reset();
        if (shadowDepthTex_) { glDeleteTextures(1, &shadowDepthTex_); shadowDepthTex_ = 0; }
        if (shadowFbo_) { glDeleteFramebuffers(1, &shadowFbo_); shadowFbo_ = 0; }
        camera_.reset();
        if (audio_) { audio_->stopMusic(); audio_->shutdown(); audio_.reset(); }
    }
}

void Renderer::render() {
    if (sceneManager_) sceneManager_->flush(*this);

    updateRenderArea();

    const auto now = std::chrono::steady_clock::now();
    float dt = 0.0f;
    if (lastFrameTime_.time_since_epoch().count() != 0)
        dt = std::chrono::duration<float>(now - lastFrameTime_).count();
    lastFrameTime_ = now;
    if (dt > 0.1f) dt = 0.1f;

    if (sceneManager_) sceneManager_->update(dt, *this);

    lightVP_ = computeLightVP();

    FrameUBO data{};
    const auto& vp = camera_->viewProjection();
    for (int i = 0; i < 16; ++i) data.viewProj[i] = vp.m[i];
    data.lightDirAmbient[0] = lightDir_.x;
    data.lightDirAmbient[1] = lightDir_.y;
    data.lightDirAmbient[2] = lightDir_.z;
    data.lightDirAmbient[3] = ambient_;
    glBindBuffer(GL_UNIFORM_BUFFER, frameUbo_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(FrameUBO), &data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Shadow pass
    if (sceneManager_ && sceneManager_->current()) {
        Scene* scene = sceneManager_->current();
        SceneNode* sr = scene->shadowRoot();
        if (sr) {
            shadowPass(sr);
            shadowMapCleared_ = false;
        } else if (!shadowMapCleared_) {
            shadowStats_ = {};
            glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
            glClear(GL_DEPTH_BUFFER_BIT);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            shadowMapCleared_ = true;
        }
    }

    // Main pass
    glClearColor(clearColor_.x, clearColor_.y, clearColor_.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    currentShader_ = nullptr;

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTex_);
    glActiveTexture(GL_TEXTURE0);

    if (sceneManager_) sceneManager_->render(*this);

    platform_.swapBuffers();
}

void Renderer::renderSceneGraph(const SceneNode* root) {
    if (root) renderNode(root);
}

void Renderer::renderUI(ui::UIRoot* ui) {
    if (ui) ui->render();
}

void Renderer::renderParticles(const particle::Emitter& emitter) {
    int count = emitter.activeCount();
    if (count == 0) return;

    const auto& particles = emitter.particles();
    const math::Mat4& view = camera_->view();
    math::Vec3 right(view.m[0], view.m[4], view.m[8]);
    math::Vec3 up(view.m[1], view.m[5], view.m[9]);

    int needed = count * 4;
    if (needed > particleBufferMax_) {
        particleBufferMax_ = needed * 2;
        glBindBuffer(GL_ARRAY_BUFFER, particleVbo_);
        glBufferData(GL_ARRAY_BUFFER,
                     particleBufferMax_ * 7 * sizeof(float),
                     nullptr, GL_DYNAMIC_DRAW);

        const int maxQuads = particleBufferMax_ / 4;
        std::vector<unsigned short> indices(static_cast<size_t>(maxQuads) * 6);
        for (int i = 0; i < maxQuads; ++i) {
            int base = i * 4;
            indices[i * 6 + 0] = static_cast<unsigned short>(base + 0);
            indices[i * 6 + 1] = static_cast<unsigned short>(base + 1);
            indices[i * 6 + 2] = static_cast<unsigned short>(base + 2);
            indices[i * 6 + 3] = static_cast<unsigned short>(base + 0);
            indices[i * 6 + 4] = static_cast<unsigned short>(base + 2);
            indices[i * 6 + 5] = static_cast<unsigned short>(base + 3);
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, particleIbo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size() * sizeof(unsigned short),
                     indices.data(), GL_STATIC_DRAW);
    }

    struct PVertex { float x, y, z; float r, g, b, a; };
    std::vector<PVertex> verts(static_cast<size_t>(count) * 4);
    for (int i = 0; i < count; ++i) {
        const auto& p = particles[i];
        math::Vec3 rv = right * p.size * 0.5f;
        math::Vec3 uv = up * p.size * 0.5f;
        math::Vec3 c = p.position;
        int base = i * 4;
        verts[base + 0] = {c.x - rv.x - uv.x, c.y - rv.y - uv.y, c.z - rv.z - uv.z,
                           p.color.x, p.color.y, p.color.z, p.color.w};
        verts[base + 1] = {c.x + rv.x - uv.x, c.y + rv.y - uv.y, c.z + rv.z - uv.z,
                           p.color.x, p.color.y, p.color.z, p.color.w};
        verts[base + 2] = {c.x + rv.x + uv.x, c.y + rv.y + uv.y, c.z + rv.z + uv.z,
                           p.color.x, p.color.y, p.color.z, p.color.w};
        verts[base + 3] = {c.x - rv.x + uv.x, c.y - rv.y + uv.y, c.z - rv.z + uv.z,
                           p.color.x, p.color.y, p.color.z, p.color.w};
    }

    particleShader_->activate();
    currentShader_ = particleShader_.get();
    glBindBuffer(GL_ARRAY_BUFFER, particleVbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(PVertex), verts.data());

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PVertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(PVertex),
                          reinterpret_cast<const void*>(12));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, particleIbo_);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawElements(GL_TRIANGLES, count * 6, GL_UNSIGNED_SHORT, nullptr);
    glDisable(GL_BLEND);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Renderer::renderStars(const engine::stars::StarField& field, float intensity, float time) {
    if (intensity <= 0.001f || field.stars.empty() || !starsShader_) return;
    const int count = static_cast<int>(field.stars.size());
    struct SVertex { float dx, dy, dz, mag, r, g, b; };
    std::vector<SVertex> verts(count);
    for (int i = 0; i < count; i++) {
        const auto& s = field.stars[i];
        verts[i] = {s.dir.x, s.dir.y, s.dir.z, s.magnitude, s.color.x, s.color.y, s.color.z};
    }
    glBindBuffer(GL_ARRAY_BUFFER, starsVbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(SVertex),
                 verts.data(), GL_DYNAMIC_DRAW);
    starsShader_->activate();
    currentShader_ = starsShader_.get();
    starsShader_->setFloat("uIntensity", intensity);
    starsShader_->setFloat("uDayPhase", time);
    starsShader_->setFloat("uPointSize", starConfig_.pointSize);
    starsShader_->setFloat("uBrightnessScale", starConfig_.brightnessScale);
    starsShader_->setFloat("uBrightFloor", starConfig_.brightnessFloor);
    starsShader_->setFloat("uBrightCeil", starConfig_.brightnessCeil);
    starsShader_->setFloat3("uSkyAxis", starConfig_.skyAxis.x, starConfig_.skyAxis.y, starConfig_.skyAxis.z);
    glDepthMask(GL_FALSE);
    glBindVertexArray(starsVao_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SVertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(SVertex),
                          reinterpret_cast<const void*>(12));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(SVertex),
                          reinterpret_cast<const void*>(16));
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_POINTS, 0, count);
    glDisable(GL_BLEND);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDepthMask(GL_TRUE);
}

void Renderer::renderLightningFlash(const math::Vec3& color, float intensity) {
    if (intensity <= 0.001f || !flashShader_) return;
    glDepthMask(GL_FALSE);
    flashShader_->activate();
    currentShader_ = flashShader_.get();
    flashShader_->setFloat3("uColor", color.x, color.y, color.z);
    flashShader_->setFloat("uIntensity", intensity);
    glBindVertexArray(flashVao_);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
}

void Renderer::renderNode(const SceneNode* node) {
    const math::Mat4 world = node->world();
    for (const auto& d : node->drawables()) {
        if (!d.model || !d.material || !d.material->shader) continue;
        const Shader* sh = d.material->shader;
        if (sh != currentShader_) {
            sh->activate();
            sh->setLightVP(lightVP_.m);
            sh->setShadowMap(1);
            currentShader_ = sh;
        }
        if (node->hasBoneMatrices()) {
            const auto& bm = node->boneMatrices();
            sh->setBoneMatrices(&bm[0].m[0], static_cast<int>(bm.size()));
        }
        if (d.material->texture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, d.material->texture->getTextureID());
            sh->setTexture(0);
            sh->setHasTexture(true);
        } else {
            sh->setHasTexture(false);
        }
        sh->setModelMatrix(world.m);
        sh->setColor(d.color.x, d.color.y, d.color.z, 1.0f);
        glDepthMask(d.depthWrite ? GL_TRUE : GL_FALSE);
        d.model->draw();
        glDepthMask(GL_TRUE);
    }
    for (const auto& child : node->children()) renderNode(child.get());
}

void Renderer::initRenderer() {
    width_ = -1;
    height_ = -1;

    PRINT_GL_STRING(GL_VENDOR);
    PRINT_GL_STRING(GL_RENDERER);
    PRINT_GL_STRING(GL_VERSION);
    PRINT_GL_STRING_AS_LIST(GL_EXTENSIONS);

    glClearColor(CORNFLOWER_BLUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glGenBuffers(1, &frameUbo_);
    glBindBuffer(GL_UNIFORM_BUFFER, frameUbo_);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(FrameUBO), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, frameUbo_);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    {
        std::string vs = shader::loadSource(platform_, "shaders/particle.vert");
        std::string fs = shader::loadSource(platform_, "shaders/particle.frag");
        particleShader_.reset(Shader::loadShader(vs, fs, {}, false));
    }
    assert(particleShader_);
    particleShader_->bindUniformBlock("FrameData", 0);

    glGenBuffers(1, &particleVbo_);
    glGenBuffers(1, &particleIbo_);

    {
        std::string vs = shader::loadSource(platform_, "shaders/stars.vert");
        std::string fs = shader::loadSource(platform_, "shaders/stars.frag");
        starsShader_.reset(Shader::loadShader(vs, fs, {}, false));
    }
    assert(starsShader_);
    starsShader_->bindUniformBlock("FrameData", 0);
    glGenBuffers(1, &starsVbo_);
    glGenVertexArrays(1, &starsVao_);

    {
        std::string vs = shader::loadSource(platform_, "shaders/flash.vert");
        std::string fs = shader::loadSource(platform_, "shaders/flash.frag");
        flashShader_.reset(Shader::loadShader(vs, fs, {}, false));
    }
    assert(flashShader_);
    glGenBuffers(1, &flashVbo_);
    glGenVertexArrays(1, &flashVao_);
    {
        const float quad[8] = { -1.f, -1.f,  1.f, -1.f, -1.f,  1.f,  1.f,  1.f };
        glBindBuffer(GL_ARRAY_BUFFER, flashVbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glBindVertexArray(flashVao_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    initShadowMap();

    sceneManager_ = std::make_unique<SceneManager>();
    assert(initialSceneFactory_);
    sceneManager_->start(initialSceneFactory_(), *this);
}

void Renderer::updateRenderArea() {
    int width = 0, height = 0;
    platform_.framebufferSize(width, height);
    if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        glViewport(0, 0, width, height);
        camera_->setViewportSize(width, height);
        if (sceneManager_ && sceneManager_->current())
            sceneManager_->current()->onResize(width, height, *this);
    }
}

picking::Ray Renderer::pickingRay(float x, float y) const {
    const math::Mat4 invVp = camera_->viewProjection().inverted();
    return picking::fromScreen(x, y, camera_->width(), camera_->height(), invVp);
}

namespace {
struct PickedDrawable { const Mesh* mesh; math::Mat4 world; };
void collectDrawables(const SceneNode* node, std::vector<PickedDrawable>& out) {
    const math::Mat4 world = node->world();
    for (const auto& d : node->drawables()) out.push_back({d.mesh, world});
    for (const auto& child : node->children()) collectDrawables(child.get(), out);
}
} // namespace

picking::Hit Renderer::pickAt(float x, float y) const {
    std::vector<picking::HitShape> shapes;
    if (pickRoot_) {
        std::vector<PickedDrawable> drawables;
        collectDrawables(pickRoot_, drawables);
        shapes.reserve(drawables.size());
        for (const auto& pd : drawables)
            if (pd.mesh) shapes.push_back(picking::fromMesh(*pd.mesh, pd.world));
    }
    return picking::pickRay(pickingRay(x, y), shapes);
}

void Renderer::setFrameLight(const math::Vec3& dir, float ambient) {
    lightDir_ = dir;
    ambient_  = ambient;
}

void Renderer::finish() {
    platform_.requestQuit();
}

void Renderer::pointerDown(int32_t id, float x, float y) {
    pan_.active   = false;
    pan_.dragging = false;
    pan_.startX   = x;
    pan_.startY   = y;
    pan_.lastX    = x;
    pan_.lastY    = y;
    EventBus::emit<PointerDownEvent>(id, x, y);
    playClickSfx();
}

void Renderer::pointerMove(int32_t id, float x, float y) {
    if (!pan_.dragging) {
        const float ddx = x - pan_.startX;
        const float ddy = y - pan_.startY;
        if (ddx * ddx + ddy * ddy > kPanThresholdPx * kPanThresholdPx) {
            pan_.dragging = true;
            pan_.active   = true;
            pan_.lastX    = x;
            pan_.lastY    = y;
        }
    }
    if (pan_.active) {
        camera_->pan((x - pan_.lastX) * pan_.sensitivity,
                     (y - pan_.lastY) * pan_.sensitivity);
        pan_.lastX = x;
        pan_.lastY = y;
    }
    EventBus::emit<PointerMoveEvent>(id, x, y);
}

void Renderer::pointerUp(int32_t id, float x, float y) {
    pan_.active   = false;
    pan_.dragging = false;
    EventBus::emit<PointerUpEvent>(id, x, y);
}

void Renderer::scroll(float delta) {
    camera_->zoomBy(std::pow(1.1f, delta));
}

void Renderer::initShadowMap() {
    glGenTextures(1, &shadowDepthTex_);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, kShadowMapSize, kShadowMapSize,
                 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &shadowFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepthTex_, 0);
    GLenum drawBuf = GL_NONE;
    glDrawBuffers(1, &drawBuf);
    glReadBuffer(GL_NONE);
    GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    aout << "Shadow FBO status: " << (fboStatus == GL_FRAMEBUFFER_COMPLETE ? "OK" : "FAIL") << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    {
        std::string vs = shader::loadSource(platform_, "shaders/shadow.vert");
        std::string fs = shader::loadSource(platform_, "shaders/shadow.frag");
        shadowShader_.reset(Shader::loadShader(vs, fs, {}, false));
    }
    assert(shadowShader_);
    {
        std::string vs = shader::loadSource(platform_, "shaders/shadow_skinned.vert");
        std::string fs = shader::loadSource(platform_, "shaders/shadow.frag");
        shadowSkinnedShader_.reset(Shader::loadShader(vs, fs, {}, false));
    }
    assert(shadowSkinnedShader_);
}

math::Mat4 Renderer::computeLightVP() const {
    const math::Vec3 target = camera_ ? camera_->target() : math::Vec3(0.0f, 0.0f, 0.0f);
    math::Vec3 lightDirNorm = lightDir_.normalized();
    math::Vec3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(math::dot(lightDirNorm, up)) > 0.99f)
        up = math::Vec3(0.0f, 0.0f, 1.0f);
    math::Vec3 right = math::cross(up, lightDirNorm);
    up = math::cross(lightDirNorm, right);
    const float halfSize = shadowHalfSize_;
    const float dist = halfSize * 1.25f;
    math::Vec3 eye = target + lightDirNorm * dist;
    math::Mat4 view = math::Mat4::lookAt(eye, target, up);
    math::Mat4 proj = math::Mat4::orthographic(-halfSize, halfSize, -halfSize, halfSize,
                                               0.1f, dist + halfSize * 1.7f);
    return proj * view;
}

void Renderer::renderNodeShadow(const SceneNode* node) {
    const math::Mat4 world = node->world();
    const Shader* sh = node->hasBoneMatrices() ? shadowSkinnedShader_.get() : shadowShader_.get();
    for (const auto& d : node->drawables()) {
        if (!d.model || !d.material || !d.castShadow) continue;
        if (d.material->shader == nullptr) continue;
        if (sh != currentShader_) {
            sh->activate();
            sh->setLightVP(lightVP_.m);
            currentShader_ = sh;
        }
        if (node->hasBoneMatrices()) {
            const auto& bm = node->boneMatrices();
            sh->setBoneMatrices(&bm[0].m[0], static_cast<int>(bm.size()));
        }
        sh->setModelMatrix(world.m);
        d.model->draw();
        ++shadowStats_.draws;
        if (node->hasBoneMatrices()) ++shadowStats_.skinnedDraws;
    }
    for (const auto& child : node->children()) renderNodeShadow(child.get());
}

void Renderer::shadowPass(const SceneNode* root) {
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glViewport(0, 0, kShadowMapSize, kShadowMapSize);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(4.0f, 4.0f);
    currentShader_ = nullptr;
    shadowStats_ = {};
    renderNodeShadow(root);
    currentShader_ = nullptr;
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
}

void Renderer::playDefaultMusic() {
    if (!audio_) return;
    if (!musicTracks_.empty()) {
        std::vector<const audio::Music*> list;
        for (const auto& t : musicTracks_) list.push_back(t.get());
        audio_->playPlaylist(list, kMusicVolume);
    } else if (musicLoop_) {
        audio_->playMusic(*musicLoop_, true, kMusicVolume);
    }
}

void Renderer::playClickSfx() {
    if (audio_ && clickSfx_) audio_->playSound(*clickSfx_, 1.0f);
}
