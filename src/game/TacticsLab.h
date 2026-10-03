#ifndef NOT_A_GAME_TACTICS_LAB_H
#define NOT_A_GAME_TACTICS_LAB_H

#include <memory>
#include <string>
#include <vector>

#include "Action.h"
#include "BattleGrid.h"
#include "DebugPanel.h"
#include "EntityBuilder.h"
#include "EventBus.h"
#include "LabAssets.h"
#include "LabChecklist.h"
#include "Rules.h"
#include "Scene.h"
#include "SceneNode.h"
#include "UIRoot.h"
#include "UnitCatalog.h"

namespace game { class GameState; class Unit; }

class TacticsLab : public Scene {
public:
    explicit TacticsLab(game::GameState& gs);
    ~TacticsLab() override;
    void enter(Renderer& r) override;
    void exit(Renderer& r) override;
    void update(float dt, Renderer& r) override;
    void render(Renderer& r) override;
    void onResize(int w, int h, Renderer& r) override;
    SceneNode* shadowRoot() const override { return root_.get(); }

    enum Scenario { kReach, kOccupancy, kElevation, kSight, kCover, kRanged, kScenarioCount };

private:
    void buildCatalog();
    void loadScenario(int s);
    void clearScenario();
    game::Unit* spawn(const char* id, int x, int y);
    void finishGrid();
    void showCells(const std::vector<math::Vec2>& cells, const math::Vec3& color);
    void showOverlay();
    void buildUI(Renderer& r);
    void resetCamera(Renderer& r);
    void restartAutoDrive();
    void autoDrive(float dt);
    void observe();
    void checkDamageTable();
    void checkStun();

    game::GameState& gs_;
    std::unique_ptr<game::LabAssets> assets_;
    std::unique_ptr<SceneNode> root_;
    std::unique_ptr<SceneNode> overlay_;
    SceneNode* worldRoot_ = nullptr;
    SceneNode* unitsRoot_ = nullptr;
    SceneNode* fxRoot_ = nullptr;
    game::UnitCatalog catalog_;
    std::unique_ptr<grid::BattleGrid> grid_;
    std::unique_ptr<game::EntityBuilder> builder_;
    std::unique_ptr<action::Manager> actions_;
    std::vector<game::Unit*> units_;
    rules::Tunables tun_;
    int scenario_ = kReach;
    std::string info_;

    std::unique_ptr<ui::UIRoot> ui_;
    std::unique_ptr<game::DebugPanel> panel_;
    std::unique_ptr<game::LabChecklist> checks_;
    ui::UIText* infoText_ = nullptr;
    std::vector<EventBus::SubscriptionId> subs_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);

    bool  autoOn_ = true;
    int   step_ = 0;
    float stepT_ = 0.0f;
    bool  waiting_ = false;

    int   openHp0_ = 0;
    int   coverHp0_ = 0;
    int   nearHp0_ = 0;
    int   hpChanges_ = 0;
    int   lastHp_ = 0;
    bool  earlyHit_ = false;
    bool  sawProjectile_ = false;
    math::Vec3 lastProjectile_{0.0f};
};

#endif
