#ifndef NOT_A_GAME_BATTLE_DATA_H
#define NOT_A_GAME_BATTLE_DATA_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "AI.h"
#include "Action.h"
#include "BattleFlow.h"
#include "BattleFx.h"
#include "BattleGrid.h"
#include "Climate.h"
#include "DebugPanel.h"
#include "Engine.h"
#include "EntityRegistry.h"
#include "GameUI.h"
#include "LabAssets.h"
#include "LabChecklist.h"
#include "LevelLoader.h"
#include "Picker.h"
#include "Turn.h"
#include "UnitCatalog.h"
#include "UnitModels.h"
#include "UnitSprites.h"

namespace game { class GameState; class Unit; }

namespace battle {

inline constexpr float kCameraFovYRadians = 30.0f * math::PI / 180.0f;
inline constexpr float kCameraNearPlane   = 0.1f;
inline constexpr float kCameraFarPlane    = 120.0f;
inline constexpr int   kLevelCount        = 3;

const char* levelName(int levelId);

enum class Mode { Menu, ChooseMove, ChooseTarget, Busy };

struct Tally {
    int moves = 0;
    int badMoves = 0;
    int attacks = 0;
    int badAttacks = 0;
    int stunnedActed = 0;
    size_t skips = 0;
};

struct Data {
    int  levelId = 1;
    bool autoBattle = false;
    level::Level level;

    std::unique_ptr<game::LabAssets> assets;
    const Material* litMat   = nullptr;
    const Material* unlitMat = nullptr;
    game::UnitModels models;
    game::UnitSprites sprites;
    bool spritesOk = true;
    asset::Handle terrainMesh = asset::kNull;
    game::UnitCatalog catalog;

    std::unique_ptr<SceneNode> root;
    std::unique_ptr<SceneNode> overlay;
    std::unique_ptr<SceneNode> ring;
    std::unique_ptr<SceneNode> hoverRing;
    SceneNode* projRoot = nullptr;

    std::unique_ptr<grid::BattleGrid> grid;
    std::unique_ptr<game::EntityRegistry> registry;
    std::vector<game::Unit*> units;
    std::vector<std::string> names;
    size_t propCount = 0;
    std::unique_ptr<turn::Manager> turns;
    std::unique_ptr<action::Manager> actions;
    std::unique_ptr<flow::BattleFlow> flow;
    rules::Tunables tun;
    std::unique_ptr<ai::Rng> rng;

    game::Unit* turnUnit = nullptr;
    gui::TurnState turnState;
    Mode mode = Mode::Menu;
    int aiPhase = 0;
    ai::Plan aiPlan;
    std::vector<math::Vec2> highlight;
    const game::Unit* selected = nullptr;
    const game::Unit* hovered = nullptr;
    pick::Picker picker;
    flow::Outcome outcome = flow::Outcome::Ongoing;
    float resultTimer = 0.0f;
    bool framing = false;
    bool resultChecked = false;
    bool spawnedOk = false;
    Tally tally;
    std::string savePath;
    float speed = 1.0f;
    int speedIdx = 1;

    particle::Emitter particles;
    particle::Emitter rain{1024};
    particle::Emitter trail{256};
    float rainAccum = 0.0f;
    fx::FloatingTexts numbers;
    fx::StatusIcons icons;

    std::unique_ptr<ui::UIRoot> ui;
    ui::UIText* banner  = nullptr;
    ui::UIText* fpsText = nullptr;
    gui::ActionMenu menu;
    gui::UnitCard card;
    gui::TurnOrderBar bar;
    std::unique_ptr<game::DebugPanel> hud;
    std::unique_ptr<game::LabChecklist> checks;
    float fpsAccum  = 0.0f;
    int   fpsFrames = 0;

    std::vector<EventBus::SubscriptionId> subs;

    engine::stars::StarField stars;
    engine::clock::Clock     clock;
    game::ClimateState       climate;
    game::ClimatePalettes    palettes;
    engine::sky::SunState    currentSun;
};

Data& d();

void createContent(Renderer& r, game::GameState& gs);

void buildHUD(Renderer& r, game::GameState& gs);
void updateHUD(float dt, Renderer& r);

void handleTap(float x, float y, Renderer& r);
void handleHover(float x, float y, Renderer& r);

void updateTurns(float dt, Renderer& r, game::GameState& gs);
void addChecklistRows(game::LabChecklist& c);
void onMenuAction(gui::Action a);
bool playerControls(const game::Unit* u);
void playerMove(const math::Vec2& cell);
void playerAttack(game::Unit* target, Renderer& r);
void showHighlight(const std::vector<math::Vec2>& cells, const math::Vec3& color);
void saveGame();
void loadGame();

} // namespace battle

#endif
