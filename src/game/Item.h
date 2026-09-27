#ifndef NOT_A_GAME_ITEM_H
#define NOT_A_GAME_ITEM_H

#include <cstdint>
#include <string>

namespace game {

enum class EquipSlot : uint8_t {
    Head = 0,
    Body = 1,
    Hand = 2,
    Feet = 3,
    Accessory = 4,
    Count = 5,
};

struct ItemDef {
    std::string name;
    int         value = 0;
    int         atk   = 0;
    int         def   = 0;
    int         heal  = 0;
};

} // namespace game

#endif
