#include "UnitCatalog.h"

#include "Engine.h"

#include <cassert>

namespace game {

void UnitCatalog::registerDef(const std::string& id, const unit::UnitDef& def) {
    catalog_.add(id, def);
}

const unit::UnitDef* UnitCatalog::find(const std::string& id) const {
    return catalog_.find(id);
}

bool UnitCatalog::has(const std::string& id) const {
    return catalog_.has(id);
}

size_t UnitCatalog::size() const {
    return catalog_.size();
}

void UnitCatalog::clear() {
    catalog_.clear();
}

void UnitCatalog::selfTest() {
    aout << "[unit-catalog] self-test starting" << std::endl;

    UnitCatalog cat;
    assert(cat.size() == 0);
    assert(cat.find("knight") == nullptr);
    assert(!cat.has("knight"));

    unit::UnitDef knight;
    knight.name = "knight";
    knight.baseStats = {20, 8, 5, 4, 1};
    knight.faction = unit::Faction::Player;
    knight.baseColor = {0.9f, 0.5f, 0.2f};
    knight.selectedColor = {1.0f, 0.9f, 0.2f};
    cat.registerDef("knight", knight);

    unit::UnitDef mage;
    mage.name = "mage";
    mage.baseStats = {12, 12, 3, 3, 2};
    mage.faction = unit::Faction::Player;
    mage.baseColor = {0.5f, 0.3f, 0.8f};
    mage.selectedColor = {0.8f, 0.5f, 1.0f};
    cat.registerDef("mage", mage);

    unit::UnitDef goblin;
    goblin.name = "goblin";
    goblin.baseStats = {8, 4, 2, 3, 1};
    goblin.faction = unit::Faction::Enemy;
    goblin.baseColor = {0.2f, 0.6f, 0.2f};
    goblin.selectedColor = {0.8f, 0.2f, 0.2f};
    cat.registerDef("goblin", goblin);

    assert(cat.size() == 3);
    assert(cat.has("knight"));
    assert(cat.has("mage"));
    assert(cat.has("goblin"));
    assert(!cat.has("archer"));

    const unit::UnitDef* k = cat.find("knight");
    assert(k != nullptr);
    assert(k->name == "knight");
    assert(k->baseStats.maxHp == 20);
    assert(k->baseStats.atk == 8);
    assert(k->faction == unit::Faction::Player);

    const unit::UnitDef* m = cat.find("mage");
    assert(m != nullptr);
    assert(m->name == "mage");
    assert(m->baseStats.range == 2);

    const unit::UnitDef* e = cat.find("goblin");
    assert(e != nullptr);
    assert(e->faction == unit::Faction::Enemy);
    assert(e->baseStats.def == 2);

    assert(cat.find("archer") == nullptr);

    cat.clear();
    assert(cat.size() == 0);
    assert(cat.find("knight") == nullptr);

    aout << "[unit-catalog] self-test PASS" << std::endl;
}

} // namespace game
