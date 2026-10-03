#ifndef NOT_A_GAME_LEVEL_LAB_H
#define NOT_A_GAME_LEVEL_LAB_H

#include <memory>
#include <string>
#include <vector>

#include "BattleGrid.h"
#include "DebugPanel.h"
#include "EntityBuilder.h"
#include "EventBus.h"
#include "LabAssets.h"
#include "LabChecklist.h"
#include "LevelLoader.h"
#include "LevelSpawner.h"
#include "Scene.h"
#include "SceneNode.h"
#include "UIRoot.h"
#include "UnitCatalog.h"

namespace game { class GameState; class Prop; class Unit; }

class LevelLab : public Scene {
public:
    explicit LevelLab(game::GameState& gs);
    ~LevelLab() override;
    void enter(Renderer& r) override;
    void exit(Renderer& r) override;
    void update(float dt, Renderer& r) override;
    void render(Renderer& r) override;
    void onResize(int w, int h, Renderer& r) override;
    SceneNode* shadowRoot() const override { return root_.get(); }

private:
    struct Entry {
        std::string file;
        std::vector<uint8_t> bytes;
        level::Level level;
        bool loaded = false;
        asset::Handle terrain = asset::kNull;
    };

    void buildCatalog();
    void loadFiles(Renderer& r);
    void showLevel(int i);
    void buildUI(Renderer& r);
    void resetCamera(Renderer& r);
    void restartAutoDrive();
    void autoDrive(float dt, Renderer& r);
    bool checkUnits(const Entry& e) const;
    bool checkProps(const Entry& e, Renderer& r) const;
    bool checkTerrain() const;
    int  shadowDrawables() const;

    game::GameState& gs_;
    std::unique_ptr<game::LabAssets> assets_;
    std::unique_ptr<SceneNode> root_;
    std::vector<Entry> entries_;
    int current_ = 0;
    std::unique_ptr<grid::BattleGrid> grid_;
    std::unique_ptr<game::EntityBuilder> builder_;
    std::vector<game::Unit*> units_;
    std::vector<game::Prop*> props_;
    const Mesh* terrainMesh_ = nullptr;
    game::UnitCatalog catalog_;
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
    int   framesShown_ = 0;
    bool  unitsOk_ = true;
    bool  propsOk_ = true;
    bool  terrainOk_ = true;
};

#endif
