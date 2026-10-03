#ifndef NOT_A_GAME_RENDERER_H
#define NOT_A_GAME_RENDERER_H

#include <GLES3/gl3.h>
#include <chrono>
#include <memory>
#include <vector>

#include "Audio.h"
#include "Camera.h"
#include "Platform.h"
#include "Math.h"
#include "ParticleSystem.h"
#include "Picking.h"
#include "SceneManager.h"
#include "Shader.h"
#include "Sky.h"
#include "Stars.h"

class SceneNode;
namespace ui { class UIRoot; }

class Renderer {
public:
    Renderer(Platform& platform, SceneFactory initialSceneFactory);
    virtual ~Renderer();

    void pointerDown(int32_t id, float x, float y);
    void pointerMove(int32_t id, float x, float y);
    void pointerUp(int32_t id, float x, float y);
    void hover(float x, float y);
    void scroll(float delta);
    void render();

    Camera& camera() { return *camera_; }
    int width()  const { return static_cast<int>(width_); }
    int height() const { return static_cast<int>(height_); }

    void renderSceneGraph(const SceneNode* root);
    void renderUI(ui::UIRoot* ui);
    void renderParticles(const particle::Emitter& emitter);
    void renderStars(const engine::stars::StarField& field, float intensity, float time);
    void renderLightningFlash(const math::Vec3& color, float intensity);

    void setClearColor(const math::Vec3& c) { clearColor_ = c; }
    const math::Vec3& clearColor() const { return clearColor_; }

    struct StarRenderConfig {
        float pointSize        = 5.0f;
        float brightnessScale  = 1.0f;
        float brightnessFloor  = 0.05f;
        float brightnessCeil   = 1.0f;
        math::Vec3 skyAxis     = math::Vec3(1.0f, 0.0f, 0.0f);
    };
    void setStarRenderConfig(const StarRenderConfig& c) { starConfig_ = c; }
    const StarRenderConfig& starRenderConfig() const { return starConfig_; }

    picking::Ray pickingRay(float x, float y) const;
    picking::Hit pickAt(float x, float y) const;
    void setPickRoot(const SceneNode* root) { pickRoot_ = root; }

    SceneManager& sceneManager() { return *sceneManager_; }

    void zoomBy(float factor)  { if (camera_) camera_->zoomBy(factor); }
    void resetView()           { if (camera_) camera_->resetDistance(); }
    void cancelPan()           { pan_.active = false; }
    void setPanSensitivity(float s) { pan_.sensitivity = s; }
    float panSensitivity() const { return pan_.sensitivity; }
    void setOrbitSnap(float pitchDeg, float yawDeg) { if (camera_) camera_->setOrbitSnap(pitchDeg, yawDeg); }
    void setFovYRadians(float fovYRadians)          { if (camera_) camera_->setFovYRadians(fovYRadians); }

    void setMasterVolume(float v) { if (audio_) audio_->setMasterVolume(v); }
    void setSfxVolume(float v)    { if (audio_) audio_->setSfxVolume(v); }
    void setMusicVolume(float v)  { if (audio_) audio_->setMusicVolume(v); }
    void playClickSfx();
    Audio* audio() { return audio_.get(); }
    void playDefaultMusic();
    float masterVolume() const { return audio_ ? audio_->masterVolume() : 0.0f; }
    float sfxVolume()    const { return audio_ ? audio_->sfxVolume()    : 0.0f; }
    float musicVolume()  const { return audio_ ? audio_->musicVolume()  : 0.0f; }

    const engine::sky::SkyConfig& skyConfig() const { return skyConfig_; }
    void setSkyNightBlendShift(float v) { skyConfig_.nightBlendShift = v; }
    void setSkyNightBlendScale(float v) { skyConfig_.nightBlendScale = v; }
    void setSkyDawnStart(float v)       { skyConfig_.dawnStart       = v; }
    void setSkyDawnWidth(float v)       { skyConfig_.dawnWidth       = v; }
    void setSkyNoonScale(float v)       { skyConfig_.noonScale       = v; }
    void setSkyAmbientDawnMix(float v)  { skyConfig_.ambientDawnMix  = v; }
    void setSkyAmbientSunScale(float v) { skyConfig_.ambientSunScale = v; }
    void setSkyAmbientBaseline(float v) { skyConfig_.ambientBaseline = v; }
    void setSkyConfig(const engine::sky::SkyConfig& c) { skyConfig_ = c; }

    void setFrameLight(const math::Vec3& dir, float ambient);
    void finish();

    void setShadowExtent(float halfSize) { shadowHalfSize_ = halfSize; }
    float shadowExtent() const { return shadowHalfSize_; }
    const math::Mat4& lightViewProjection() const { return lightVP_; }
    struct ShadowStats { int draws = 0; int skinnedDraws = 0; };
    const ShadowStats& shadowStats() const { return shadowStats_; }

    const Platform& platform() const { return platform_; }
    std::string     dataPath() const { return platform_.dataPath(); }

private:
    void initRenderer();
    void updateRenderArea();

    Platform& platform_;
    int width_;
    int height_;

    std::unique_ptr<Camera> camera_;
    std::unique_ptr<Audio> audio_;
    std::unique_ptr<audio::Sound> clickSfx_;
    std::unique_ptr<audio::Music> musicLoop_;
    std::vector<std::unique_ptr<audio::Music>> musicTracks_;
    GLuint frameUbo_ = 0;
    const Shader* currentShader_ = nullptr;

    math::Vec3 lightDir_{0.4f, 0.8f, 0.5f};
    float ambient_ = 0.2f;
    math::Vec3 clearColor_{100 / 255.f, 149 / 255.f, 237 / 255.f};
    StarRenderConfig starConfig_;
    engine::sky::SkyConfig skyConfig_;

    const SceneNode* pickRoot_ = nullptr;
    std::unique_ptr<SceneManager> sceneManager_;
    SceneFactory initialSceneFactory_;
    std::chrono::steady_clock::time_point lastFrameTime_;

    struct PanState {
        bool  active    = false;
        bool  dragging  = false;
        float startX    = 0.0f;
        float startY    = 0.0f;
        float lastX     = 0.0f;
        float lastY     = 0.0f;
        float sensitivity = 1.0f;
    } pan_;
    static constexpr float kPanThresholdPx = 12.0f;

    std::unique_ptr<Shader> particleShader_;
    GLuint particleVbo_ = 0;
    GLuint particleIbo_ = 0;
    int particleBufferMax_ = 0;

    std::unique_ptr<Shader> starsShader_;
    GLuint starsVbo_ = 0;
    GLuint starsVao_ = 0;
    int    starsCount_ = 0;

    std::unique_ptr<Shader> flashShader_;
    GLuint flashVbo_ = 0;
    GLuint flashVao_ = 0;

    GLuint shadowFbo_ = 0;
    GLuint shadowDepthTex_ = 0;
    static constexpr int kShadowMapSize = 1024;
    std::unique_ptr<Shader> shadowShader_;
    std::unique_ptr<Shader> shadowSkinnedShader_;
    bool shadowMapCleared_ = false;
    float shadowHalfSize_ = 12.0f;
    ShadowStats shadowStats_;
    math::Mat4 lightVP_;

    void renderNode(const SceneNode* node);
    void renderNodeShadow(const SceneNode* node);
    void initShadowMap();
    void shadowPass(const SceneNode* root);
    math::Mat4 computeLightVP() const;
};

#endif //NOT_A_GAME_RENDERER_H
