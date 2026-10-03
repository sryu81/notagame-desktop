#ifndef NOT_A_GAME_SYSTEMS_LAB_H
#define NOT_A_GAME_SYSTEMS_LAB_H

#include <memory>
#include <string>
#include <vector>

#include "Audio.h"
#include "Bag.h"
#include "Catalog.h"
#include "Clock.h"
#include "DebugPanel.h"
#include "EquipSlots.h"
#include "EventBus.h"
#include "GameState.h"
#include "Item.h"
#include "LabChecklist.h"
#include "Scene.h"
#include "UIRoot.h"

class SystemsLab : public Scene {
public:
    explicit SystemsLab(game::GameState& gs) : gs_(gs) {}
    void enter(Renderer& r) override;
    void exit(Renderer& r) override;
    void update(float dt, Renderer& r) override;
    void render(Renderer& r) override;
    void onResize(int w, int h, Renderer& r) override;

private:
    void buildUI(Renderer& r);
    void restartAutoDrive();
    void autoDrive(float dt, Renderer& r);
    void checkSaves(Renderer& r);
    void checkInventory();
    void checkCalendar();
    void checkFlow(Renderer& r);
    void refreshInfo();
    std::string slotPath(Renderer& r, int slot) const;

    game::GameState& gs_;
    game::GameState labState_;
    engine::Bag<game::ItemDef> bag_;
    engine::EquipSlots<game::ItemDef, game::EquipSlot> equip_{
        game::EquipSlot::Head, game::EquipSlot::Body, game::EquipSlot::Hand,
        game::EquipSlot::Feet, game::EquipSlot::Accessory};
    engine::Catalog<game::ItemDef> catalog_;
    engine::clock::Clock calendar_{4, 10};

    std::vector<audio::Sound> sfx_;
    std::vector<std::string> sfxNames_;
    audio::Music music2_;
    bool music2Loaded_ = false;
    bool onMusic2_ = false;
    float savedMaster_ = 1.0f;
    int sfxIdx_ = 0;
    int slot_ = 1;

    std::unique_ptr<ui::UIRoot> ui_;
    std::unique_ptr<game::DebugPanel> panel_;
    std::unique_ptr<game::LabChecklist> checks_;
    std::vector<ui::UIText*> info_;
    std::vector<EventBus::SubscriptionId> subs_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);

    bool  autoOn_ = true;
    float autoT_ = 0.0f;
    int   step_ = 0;
    int   peakVoices_ = 0;
};

#endif
