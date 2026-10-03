#include "SystemsLab.h"

#include <algorithm>
#include <cstdio>

#include <GLES3/gl3.h>

#include "AI.h"
#include "Action.h"
#include "BattleFlow.h"
#include "BattleGrid.h"
#include "Engine.h"
#include "EntityRegistry.h"
#include "HubScene.h"
#include "ResultScene.h"
#include "Turn.h"
#include "Unit.h"

namespace {

const char* kSaveSlots   = "SAVE SLOTS";
const char* kIsolation   = "SLOT ISOLATION";
const char* kRegistry    = "REGISTRY SAVEABLE";
const char* kBagRow      = "BAG";
const char* kEquipRow    = "EQUIP";
const char* kCatalogRow  = "CATALOG";
const char* kCalendar    = "CUSTOM CALENDAR";
const char* kSfxVoices   = "SFX VOICES";
const char* kMusicSwitch = "MUSIC SWITCH";
const char* kVolumeBus   = "VOLUME BUSES";
const char* kTurnIdle    = "TURN ENDS ON IDLE";
const char* kWinLose     = "WIN/LOSE EACH CONDITION";
const char* kResultRow   = "RESULT SCENE";
const char* kSaveRestore = "SAVE MID-BATTLE == RESTORE";
const char* kRosterById  = "ROSTER BY ID";

const char* kSfxFiles[] = {"select.wav", "move.wav", "hit.wav", "miss.wav", "turn.wav"};

game::ItemDef item(const char* name, int value, int atk = 0, int def = 0, int heal = 0) {
    game::ItemDef d;
    d.name = name; d.value = value; d.atk = atk; d.def = def; d.heal = heal;
    return d;
}

void setRoster(game::GameState& gs, const std::vector<int>& hps) {
    gs.initDefaultRoster(hps.size());
    size_t i = 0;
    for (int hp : hps) gs.entry(i++).hp = hp;
}

std::string playTurns(ai::Battle& b, turn::Manager& tm, ai::Rng& rng, int n) {
    std::vector<game::Unit*> ptrs;
    for (auto& u : b.units) ptrs.push_back(u.get());
    std::string log;
    for (int k = 0; k < n && ai::winnerOf(ptrs) < 0; ++k) {
        game::Unit* u = tm.currentUnit();
        int idx = 0, target = -1, dmg = 0;
        for (size_t i = 0; i < ptrs.size(); ++i) if (ptrs[i] == u) idx = static_cast<int>(i);
        const ai::Plan plan = ai::planTurn(*u, *b.grid, ptrs, {}, rng);
        if (!plan.path.empty()) u->setCell(plan.path.back());
        if (plan.target) {
            dmg = rules::damage(u->stats(), plan.target->stats(),
                                rules::contextFor(*b.grid, *u, *plan.target, rng.next()));
            plan.target->takeDamage(dmg);
            for (size_t i = 0; i < ptrs.size(); ++i) if (ptrs[i] == plan.target) target = static_cast<int>(i);
        }
        log += ai::turnLog(tm.round(), idx, *u, target, dmg);
        tm.endTurn();
    }
    return log;
}

bool rosterIs(const game::GameState& gs, const std::vector<int>& hps) {
    if (gs.rosterSize() != hps.size()) return false;
    size_t i = 0;
    for (int hp : hps) if (gs.entry(i++).hp != hp) return false;
    return true;
}

}

std::string SystemsLab::slotPath(Renderer& r, int slot) const {
    return r.dataPath() + "/lab_slot" + std::to_string(slot) + ".sav";
}

void SystemsLab::enter(Renderer& r) {
    r.setPickRoot(nullptr);
    r.setClearColor(math::Vec3(0.08f, 0.09f, 0.12f));
    if (Audio* a = r.audio()) {
        savedMaster_ = a->masterVolume();
        for (const char* f : kSfxFiles) {
            audio::Sound s;
            if (a->loadSound(f, s)) {
                sfx_.push_back(std::move(s));
                sfxNames_.push_back(f);
            } else {
                aout << "[systemslab] failed to load " << f << std::endl;
            }
        }
        music2Loaded_ = a->loadMusic("music2.wav", music2_);
    }

    ui_ = std::make_unique<ui::UIRoot>();
    ui_->init();
    checks_ = std::make_unique<game::LabChecklist>(math::Vec2(16, 16));
    for (const char* row : {kSaveSlots, kIsolation, kRegistry, kBagRow, kEquipRow, kCatalogRow,
                            kCalendar, kSfxVoices, kMusicSwitch, kVolumeBus, kTurnIdle, kWinLose,
                            kResultRow, kSaveRestore, kRosterById})
        checks_->add(row);
    buildUI(r);
    restartAutoDrive();

    Renderer* rptr = &r;
    auto alive = alive_;
    SystemsLab* self = this;
    subs_.push_back(EventBus::on<PointerDownEvent>([alive, self, rptr](const PointerDownEvent& e) {
        if (!*alive || !self->ui_) return;
        if (ui::UIButton* btn = self->ui_->hitTestButton(math::Vec2(e.x, e.y))) {
            btn->handleClick();
            rptr->cancelPan();
        }
    }));
}

void SystemsLab::exit(Renderer& r) {
    *alive_ = false;
    for (auto id : subs_) EventBus::unsubscribe(id);
    subs_.clear();
    if (Audio* a = r.audio()) {
        a->setMasterVolume(savedMaster_);
        if (onMusic2_) r.playDefaultMusic();
    }
    ui_.reset();
    panel_.reset();
    checks_.reset();
    info_.clear();
}

void SystemsLab::restartAutoDrive() {
    autoT_ = 0.0f;
    step_ = 0;
    peakVoices_ = 0;
    if (checks_) checks_->reset();
}

void SystemsLab::buildUI(Renderer& r) {
    const int w = r.width() > 0 ? r.width() : 1280;
    const int h = r.height() > 0 ? r.height() : 720;
    ui_->setScreenSize(w, h);
    const float rowH = std::clamp(static_cast<float>(h) / 18.0f, 36.0f, 72.0f);
    const float rowW = rowH * 5.5f;
    const int scale = rowH >= 56.0f ? 3 : 2;

    auto root = std::make_unique<ui::UIPanel>(math::Vec2(0, 0),
        math::Vec2(static_cast<float>(w), static_cast<float>(h)), math::Vec4(0, 0, 0, 0));

    Renderer* rptr = &r;
    game::GameState* gs = &gs_;
    panel_ = std::make_unique<game::DebugPanel>(
        math::Vec2(static_cast<float>(w) - 24.0f - rowW, 24.0f),
        math::Vec2(rowW, rowH), rowH * 0.15f, scale);
    panel_->addRow("AUTO", [this]() { return std::string(autoOn_ ? "ON" : "OFF"); },
                   [this]() { autoOn_ = !autoOn_; });
    panel_->addAction("RESTART CHECKS", [this]() { autoOn_ = true; restartAutoDrive(); });
    panel_->addRow("SLOT", [this]() { return std::to_string(slot_); },
                   [this]() { slot_ = slot_ % 3 + 1; });
    panel_->addAction("SAVE SLOT", [this, rptr]() {
        save::writeAll(slotPath(*rptr, slot_).c_str(), labState_);
    });
    panel_->addAction("LOAD SLOT", [this, rptr]() {
        save::loadAll(slotPath(*rptr, slot_).c_str(), labState_);
    });
    panel_->addAction("DAMAGE UNIT 0", [this]() {
        if (labState_.rosterSize() == 0) labState_.initDefaultRoster(3);
        labState_.entry(0).hp = std::max(0, labState_.entry(0).hp - 1);
    });
    panel_->addAction("ADD POTION", [this]() { bag_.add("potion", item("POTION", 5, 0, 0, 3)); });
    panel_->addAction("USE POTION", [this]() { bag_.remove("potion"); });
    panel_->addRow("HELM", [this]() { return std::string(equip_.isEmpty(game::EquipSlot::Head) ? "OFF" : "ON"); },
                   [this]() {
                       if (equip_.isEmpty(game::EquipSlot::Head))
                           equip_.equip(game::EquipSlot::Head, item("HELM", 12, 0, 2));
                       else
                           equip_.unequip(game::EquipSlot::Head);
                   });
    panel_->addRow("PLAY SFX", [this]() {
                       return sfxNames_.empty() ? std::string("NONE") : sfxNames_[sfxIdx_].substr(0, sfxNames_[sfxIdx_].size() - 4);
                   },
                   [this, rptr]() {
                       if (sfx_.empty() || !rptr->audio()) return;
                       rptr->audio()->playSound(sfx_[sfxIdx_]);
                       sfxIdx_ = (sfxIdx_ + 1) % static_cast<int>(sfx_.size());
                   });
    panel_->addRow("MUSIC", [this]() { return std::string(onMusic2_ ? "2" : "1"); }, [this, rptr]() {
        if (!rptr->audio()) return;
        onMusic2_ = !onMusic2_ && music2Loaded_;
        if (onMusic2_) rptr->audio()->playMusic(music2_, true, 0.5f);
        else           rptr->playDefaultMusic();
    });
    panel_->addAction("RESULT DEMO", [rptr, gs]() {
        rptr->sceneManager().replace(std::make_unique<ResultScene>(*gs, flow::Outcome::Victory,
                                                                   "SYSTEMS LAB DEMO"));
    });
    panel_->addAction("BACK", [rptr, gs]() {
        rptr->sceneManager().replace(std::make_unique<HubScene>(*gs));
    });
    root->addChild(panel_->build());
    root->addChild(checks_->build());

    info_.clear();
    const float infoY = static_cast<float>(h) * 0.5f;
    for (int i = 0; i < 5; ++i) {
        auto t = std::make_unique<ui::UIText>(math::Vec2(16.0f, infoY + static_cast<float>(i) * 30.0f),
                                              "", math::Vec4(0.9f, 0.9f, 0.75f, 1.0f), 2);
        info_.push_back(t.get());
        root->addChild(std::move(t));
    }
    ui_->setRoot(std::move(root));
    refreshInfo();
}

void SystemsLab::onResize(int w, int h, Renderer& r) {
    if (w < 1 || h < 1 || !ui_) return;
    buildUI(r);
}

void SystemsLab::refreshInfo() {
    if (info_.size() < 5) return;
    std::string roster = "ROSTER HP:";
    for (size_t i = 0; i < labState_.rosterSize(); ++i) roster += " " + std::to_string(labState_.entry(i).hp);
    info_[0]->setText(roster + "  (SLOT " + std::to_string(slot_) + ")");
    std::string bag = "BAG:";
    for (const auto& e : bag_.entries()) bag += " " + e.item.name + "X" + std::to_string(e.count);
    info_[1]->setText(bag_.size() == 0 ? "BAG: EMPTY" : bag);
    const game::ItemDef* head = equip_.get(game::EquipSlot::Head);
    info_[2]->setText(std::string("HEAD: ") + (head ? head->name : "-") +
                      "  CATALOG: " + std::to_string(catalog_.size()));
    const auto& t = calendar_.now();
    char buf[64];
    snprintf(buf, sizeof(buf), "CALENDAR 4X10: Y%d M%d D%d %02d:%02d", t.year, t.month, t.day, t.hour, t.minute);
    info_[3]->setText(buf);
}

void SystemsLab::checkSaves(Renderer& r) {
    bool slotsOk = true;
    const std::vector<std::vector<int>> hp = {{10, 9, 8}, {5, 4, 3}, {1, 2, 3}};
    for (int s = 0; s < 3; ++s) {
        game::GameState out;
        setRoster(out, hp[s]);
        slotsOk = slotsOk && save::writeAll(slotPath(r, s + 1).c_str(), out);
    }
    for (int s = 0; s < 3; ++s) {
        game::GameState in;
        slotsOk = slotsOk && save::loadAll(slotPath(r, s + 1).c_str(), in) && rosterIs(in, hp[s]);
    }
    checks_->check(kSaveSlots, slotsOk);

    game::GameState edit;
    setRoster(edit, {7, 7, 7});
    const bool wrote = save::writeAll(slotPath(r, 2).c_str(), edit);
    game::GameState s1, s2;
    const bool isolated = wrote &&
        save::loadAll(slotPath(r, 1).c_str(), s1) && rosterIs(s1, hp[0]) &&
        save::loadAll(slotPath(r, 2).c_str(), s2) && rosterIs(s2, {7, 7, 7});
    checks_->check(kIsolation, isolated);

    SceneNode parent;
    game::EntityRegistry reg;
    reg.add(std::make_unique<game::Unit>(&parent, unit::Faction::Player, unit::Stats{12, 5, 3, 3, 1}));
    reg.add(std::make_unique<game::Unit>(&parent, unit::Faction::Enemy, unit::Stats{9, 4, 2, 3, 1}));
    reg.unitAt(0)->takeDamage(4);
    reg.unitAt(1)->takeDamage(2);
    const engine::Saveable& asSaveable = reg;
    const std::string regPath = r.dataPath() + "/lab_registry.sav";
    game::EntityRegistry back;
    engine::Saveable& backSaveable = back;
    const bool regOk = save::writeAll(regPath.c_str(), asSaveable) &&
                       save::loadAll(regPath.c_str(), backSaveable) &&
                       back.size() == 2 && back.unitAt(0) && back.unitAt(1) &&
                       back.unitAt(0)->hp() == 8 && back.unitAt(1)->hp() == 7;
    checks_->check(kRegistry, regOk);

    save::loadAll(slotPath(r, slot_).c_str(), labState_);
}

void SystemsLab::checkInventory() {
    engine::Bag<game::ItemDef> bag;
    bag.add("potion", item("POTION", 5, 0, 0, 3), 3);
    const bool removed = bag.remove("potion");
    const int afterOne = bag.countOf("potion");
    bag_ = bag;
    const bool overRemove = bag.remove("potion", 5);
    const bool missing = bag.remove("elixir");
    checks_->check(kBagRow, removed && afterOne == 2 && overRemove &&
                            bag.countOf("potion") == 0 && bag.size() == 0 && !missing);

    engine::EquipSlots<game::ItemDef, game::EquipSlot> eq{game::EquipSlot::Head, game::EquipSlot::Hand};
    const bool first = eq.equip(game::EquipSlot::Head, item("HELM", 12, 0, 2));
    const bool second = eq.equip(game::EquipSlot::Head, item("CAP", 3, 0, 1));
    const game::ItemDef* got = eq.get(game::EquipSlot::Head);
    const bool gotHelm = got && got->name == "HELM";
    const bool noSlot = eq.equip(game::EquipSlot::Feet, item("BOOTS", 4));
    auto taken = eq.unequip(game::EquipSlot::Head);
    checks_->check(kEquipRow, first && !second && gotHelm && !noSlot &&
                              taken && taken->name == "HELM" && eq.isEmpty(game::EquipSlot::Head));

    engine::Catalog<game::ItemDef> cat;
    cat.add("potion", item("POTION", 5));
    cat.add("helm", item("HELM", 12));
    cat.add("sword", item("SWORD", 30, 4));
    const game::ItemDef* sword = cat.find("sword");
    const bool rm = cat.remove("helm");
    checks_->check(kCatalogRow, sword && sword->atk == 4 && rm && !cat.remove("helm") &&
                                cat.size() == 2 && cat.all().size() == 2 && !cat.find("helm"));
    catalog_ = cat;
}

void SystemsLab::checkCalendar() {
    engine::clock::Clock c(4, 10);
    c.setDateTime(1, 1, 1, 0, 0, 0);
    c.addSeconds(10.0 * 86400.0);
    const bool month2 = c.year() == 1 && c.month() == 2 && c.day() == 1;
    c.setDateTime(1, 1, 1, 0, 0, 0);
    c.addSeconds(40.0 * 86400.0);
    const bool year2 = c.year() == 2 && c.month() == 1 && c.day() == 1;
    checks_->check(kCalendar, c.daysPerYear() == 40 && month2 && year2);
}

void SystemsLab::checkFlow(Renderer& r) {
    {
        grid::BattleGrid g(6, 1, 1.0f);
        SceneNode na, nb;
        game::Unit a(&na, unit::Faction::Player, unit::Stats{10, 5, 3, 4, 1});
        game::Unit b(&nb, unit::Faction::Enemy, unit::Stats{10, 5, 3, 4, 1});
        a.setCell(math::Vec2(0, 0));
        b.setCell(math::Vec2(5, 0));
        a.setGrid(&g);
        b.setGrid(&g);
        turn::Manager tm;
        tm.setUnits({&a, &b});
        action::Manager am;
        flow::BattleFlow bf(tm, am);
        bool held = true;
        for (int i = 0; i < 10; ++i) {
            tm.update(5.0f);
            held = held && !bf.update() && tm.currentUnit() == &a;
        }
        am.enqueue(std::make_unique<action::MoveAction>(&a, &g, std::vector<math::Vec2>{{0, 0}, {1, 0}, {2, 0}}, 0.2f));
        bf.acted();
        bool exact = true;
        int ends = 0;
        for (int i = 0; i < 40; ++i) {
            am.update(0.05f);
            const bool idle = am.isIdle();
            const bool ended = bf.update();
            if (ended) ++ends;
            if (ended && !idle) exact = false;
            if (idle && ends == 0) exact = false;
        }
        checks_->check(kTurnIdle, held && exact && ends == 1 && tm.currentUnit() == &b && a.cell().x == 2.0f);
    }

    {
        int ok = 0;
        for (int c = 0; c < 3; ++c) {
            game::Unit p(nullptr, unit::Faction::Player, unit::Stats{5, 3, 1, 3, 1});
            game::Unit e(nullptr, unit::Faction::Enemy, unit::Stats{5, 3, 1, 3, 1});
            p.setCell(math::Vec2(0, 0));
            e.setCell(math::Vec2(4, 0));
            const std::vector<game::Unit*> all{&p, &e};
            level::WinCondition w;
            w.type = static_cast<level::WinType>(c);
            w.a = c == 1 ? 3 : 2;
            int round = 1;
            if (flow::evaluate(w, all, round) == flow::Outcome::Ongoing) ++ok;
            if (c == 0) e.takeDamage(99);
            if (c == 1) round = 4;
            if (c == 2) p.setCell(math::Vec2(2, 0));
            if (flow::evaluate(w, all, round) == flow::Outcome::Victory) ++ok;
            game::Unit q(nullptr, unit::Faction::Player, unit::Stats{5, 3, 1, 3, 1});
            game::Unit f(nullptr, unit::Faction::Enemy, unit::Stats{5, 3, 1, 3, 1});
            q.takeDamage(99);
            if (flow::evaluate(w, {&q, &f}, 1) == flow::Outcome::Defeat) ++ok;
        }
        checks_->check(kWinLose, ok == 9);
    }

    {
        ResultScene rs(gs_, flow::Outcome::Defeat, "LAB");
        const auto next = rs.continueScene();
        checks_->check(kResultRow, rs.outcome() == flow::Outcome::Defeat &&
                                   dynamic_cast<HubScene*>(next.get()) != nullptr);
    }

    {
        ai::SimConfig cfg;
        cfg.seed = 4;
        auto setup = [&cfg](ai::Battle& b, turn::Manager& tm) {
            b = ai::setupBattle(cfg, ai::defaultRoster());
            std::vector<game::Unit*> ptrs;
            for (size_t i = 0; i < b.units.size(); ++i) {
                b.units[i]->setUnitId(static_cast<uint16_t>(i + 1));
                ptrs.push_back(b.units[i].get());
            }
            tm.setUnits(ptrs);
            tm.setTimed(false);
            return ptrs;
        };
        ai::Battle a;
        turn::Manager tmA;
        auto ptrsA = setup(a, tmA);
        ai::Rng rngA(cfg.seed);
        playTurns(a, tmA, rngA, 6);
        uint32_t stateA = rngA.state();
        const std::string path = r.dataPath() + "/lab_battle.sav";
        flow::BattleSnapshot snapA(ptrsA, &tmA, &stateA);
        const bool wrote = save::writeAll(path.c_str(), snapA);
        const std::string logA = playTurns(a, tmA, rngA, 10);

        ai::Battle b;
        turn::Manager tmB;
        auto ptrsB = setup(b, tmB);
        std::reverse(ptrsB.begin(), ptrsB.end());
        uint32_t stateB = 0;
        flow::BattleSnapshot snapB(ptrsB, &tmB, &stateB);
        const bool read = save::loadAll(path.c_str(), snapB) && snapB.loaded();
        ai::Rng rngB(999);
        rngB.setState(stateB);
        const std::string logB = playTurns(b, tmB, rngB, 10);
        if (logA != logB) aout << "[systemslab] resume differs:\n  " << logA << "\n  " << logB << std::endl;
        checks_->check(kSaveRestore, wrote && read && !logA.empty() && logA == logB);
    }

    {
        SceneNode parent;
        game::EntityRegistry reg;
        const uint16_t ids[3] = {5, 9, 2};
        const int dmg[3] = {1, 4, 6};
        for (int i = 0; i < 3; ++i) {
            auto u = std::make_unique<game::Unit>(&parent, unit::Faction::Player, unit::Stats{12, 5, 3, 3, 1});
            u->setUnitId(ids[i]);
            u->takeDamage(dmg[i]);
            reg.add(std::move(u));
        }
        game::GameState out;
        out.captureFrom(reg);
        const std::string path = r.dataPath() + "/lab_roster.sav";
        game::GameState in;
        const bool io = save::writeAll(path.c_str(), out) && save::loadAll(path.c_str(), in);
        game::EntityRegistry shuffled;
        const uint16_t order[3] = {2, 5, 9};
        for (uint16_t id : order) {
            auto u = std::make_unique<game::Unit>(&parent, unit::Faction::Player, unit::Stats{12, 5, 3, 3, 1});
            u->setUnitId(id);
            shuffled.add(std::move(u));
        }
        in.restoreTo(shuffled);
        checks_->check(kRosterById, io && shuffled.unitAt(0)->hp() == 6 && shuffled.unitAt(1)->hp() == 11 &&
                                    shuffled.unitAt(2)->hp() == 8);
    }
}

void SystemsLab::autoDrive(float dt, Renderer& r) {
    autoT_ += dt;
    Audio* a = r.audio();
    switch (step_) {
        case 0:
            checkSaves(r);
            checkInventory();
            checkCalendar();
            checkFlow(r);
            calendar_.setDateTime(1, 4, 9, 22, 0, 0);
            calendar_.setTimeScale(3600.0f);
            if (!a || !a->isOpen()) aout << "[systemslab] no audio device; audio rows stay red" << std::endl;
            step_ = 1;
            break;
        case 1:
            if (autoT_ < 0.5f) break;
            if (a && a->isOpen()) {
                for (size_t i = 0; i < sfx_.size(); ++i) a->playSound(sfx_[i], 0.6f);
                peakVoices_ = a->activeVoiceCount();
            }
            step_ = 2;
            break;
        case 2:
            if (autoT_ < 0.6f) break;
            checks_->check(kSfxVoices, sfx_.size() == 5 && peakVoices_ >= 2);
            if (a && a->isOpen() && music2Loaded_) {
                a->playMusic(music2_, true, 0.5f);
                onMusic2_ = true;
                checks_->check(kMusicSwitch, a->currentMusic() == &music2_);
                a->setMasterVolume(0.0f);
            }
            step_ = 3;
            break;
        case 3: {
            if (autoT_ < 1.0f) break;
            const bool silent = a && a->isOpen() && a->outputPeak() == 0.0f;
            if (a) a->setMasterVolume(1.0f);
            step_ = silent ? 4 : 6;
            if (!silent) aout << "[systemslab] master 0 but peak=" << (a ? a->outputPeak() : -1.0f) << std::endl;
            break;
        }
        case 4:
            if (autoT_ < 1.4f) break;
            checks_->check(kVolumeBus, a && a->outputPeak() > 0.0f);
            if (a) a->setMasterVolume(savedMaster_);
            step_ = 5;
            break;
        case 5:
        case 6:
            if (a) a->setMasterVolume(savedMaster_);
            aout << "[systemslab] auto-drive done: " << checks_->passedCount() << "/"
                 << checks_->size() << " checks green" << std::endl;
            step_ = 7;
            break;
        default:
            break;
    }
}

void SystemsLab::update(float dt, Renderer& r) {
    calendar_.tick(dt);
    if (autoOn_) autoDrive(dt, r);
    refreshInfo();
    if (panel_) panel_->refresh();
}

void SystemsLab::render(Renderer& r) {
    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r.renderUI(ui_.get());
}
