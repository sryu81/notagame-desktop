#ifndef NOT_A_GAME_BATTLE_DATA_H
#define NOT_A_GAME_BATTLE_DATA_H

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "Action.h"
#include "BattleGrid.h"
#include "Climate.h"
#include "DebugPanel.h"
#include "Engine.h"
#include "EntityRegistry.h"
#include "LabAssets.h"
#include "Turn.h"
#include "UnitCatalog.h"

namespace game { class GameState; class Unit; }

namespace battle {

inline constexpr float kCameraFovYRadians = 30.0f * math::PI / 180.0f;
inline constexpr float kCameraNearPlane   = 0.1f;
inline constexpr float kCameraFarPlane    = 100.0f;
inline constexpr int   kLandscapeCount    = 5;

enum HudPanel { kPanelTime, kPanelCamera, kPanelAudio, kPanelParticles,
                kPanelSky, kPanelWeather, kPanelScene, kPanelCount };

struct Data {
    std::unique_ptr<game::LabAssets> assets;
    const Material* litMat   = nullptr;
    const Material* unlitMat = nullptr;

    std::unique_ptr<SceneNode> root;
    std::unique_ptr<SceneNode> shadowRoot;
    std::unique_ptr<SceneNode> pathOverlay;
    std::unique_ptr<SceneNode> selectionIndicator;

    std::unique_ptr<game::EntityRegistry> registry;
    std::unique_ptr<grid::BattleGrid> grid;
    std::unique_ptr<turn::Manager> turnManager;
    std::unique_ptr<action::Manager> actionManager;
    game::UnitCatalog unitCatalog;
    std::deque<anim::Clip> unitClips;
    anim::Clip attackClip;
    anim::Clip hitReactionClip;
    anim::SkeletalClip skinnedAttackClip;
    anim::SkeletalClip skinnedHitReaction;
    anim::Skeleton skinnedSkeleton;
    std::string savePath;

    particle::Emitter particleEmitter;
    particle::Emitter rainEmitter{1024};
    float rainAccum  = 0.0f;
    float burstTimer = 0.0f;

    game::Unit* lastTurnUnit = nullptr;
    game::Unit* movingUnit   = nullptr;
    bool  wasActionIdle   = true;
    bool  camReturnActive = false;
    float camReturnTimer  = 0.0f;
    math::Vec3 preMoveCamTarget{0.0f};

    int landscapeIdx = 0;
    std::vector<asset::Handle> terrainHandles;
    std::vector<SceneNode*> tileNodes;

    std::unique_ptr<ui::UIRoot> ui;
    ui::UIText* turnBanner = nullptr;
    ui::UIText* fpsText    = nullptr;
    ui::UIText* debugText  = nullptr;
    ui::UIText* timeText   = nullptr;
    std::unique_ptr<game::DebugPanel> hudMenu;
    std::unique_ptr<game::DebugPanel> hudPanels[kPanelCount];
    int   hudOpen = kPanelTime;
    float fpsAccum  = 0.0f;
    int   fpsFrames = 0;
    int   boneCount = 0;

    int speedPreset       = 2;
    int wxPreset          = 0;
    int camOrbitPreset    = 0;
    int camFovPreset      = 1;
    int camSensPreset     = 1;
    int masterPreset      = 4;
    int musicPreset       = 2;
    int sfxPreset         = 4;
    int partGravityPreset = 2;
    int partUpVelPreset   = 2;
    int partShrinkPreset  = 1;
    int skyNightPreset    = 2;
    int skyDawnPreset     = 2;
    int skyNoonPreset     = 2;
    int skyAmbientPreset  = 2;
    int wxFlashPreset     = 2;
    int wxDecayPreset     = 2;
    int wxIntervalPreset  = 2;
    int wxJitterPreset    = 2;

    std::vector<EventBus::SubscriptionId> subs;

    engine::stars::StarField stars;
    engine::clock::Clock     clock;
    game::ClimateState       climate;
    game::ClimatePalettes    palettes;
    engine::sky::SunState    currentSun;
};

Data& d();
int& pendingLandscapeIdx();

void createContent(Renderer& r, game::GameState& gs);
void setLandscape(int idx);

void buildHUD(Renderer& r, game::GameState& gs);
void updateHUD(float dt);
void refreshTimeText();

void handlePointerDown(float x, float y, Renderer& r);

void setPathOverlay(const std::vector<math::Vec2>& path);
void refreshSelectionOverlay();
void enqueueMove(game::Unit* u, std::vector<math::Vec2> path);
void enqueueAttack(game::Unit* attacker, game::Unit* target);
game::Unit* pickUnit(float x, float y, Renderer& r);
math::Vec2 cellAtScreen(float x, float y, Renderer& r);
bool isPlayerTurn();
std::vector<game::Unit*> charactersAll();
size_t charactersAliveCount();
game::Unit* selectedUnit();
void placeUnitAtCell(game::Unit* u);
void tickTurnStart();
void saveGame();
void loadGame();

} // namespace battle

#endif
