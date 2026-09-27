#ifndef NOT_A_GAME_UNITCATALOG_H
#define NOT_A_GAME_UNITCATALOG_H

#include <string>

#include "Catalog.h"
#include "UnitDef.h"

namespace game {

class UnitCatalog {
public:
    void registerDef(const std::string& id, const unit::UnitDef& def);
    const unit::UnitDef* find(const std::string& id) const;
    bool has(const std::string& id) const;
    size_t size() const;
    void clear();

    static void selfTest();

private:
    engine::Catalog<unit::UnitDef> catalog_;
};

} // namespace game

#endif
