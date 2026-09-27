#ifndef NOT_A_GAME_EQUIPSLOTS_H
#define NOT_A_GAME_EQUIPSLOTS_H

#include <cassert>
#include <initializer_list>
#include <map>
#include <optional>
#include <utility>

namespace engine {

template <typename T, typename SlotEnum>
class EquipSlots {
public:
    EquipSlots() = default;
    explicit EquipSlots(std::initializer_list<SlotEnum> slots) {
        for (auto s : slots) slots_[s] = std::nullopt;
    }

    bool equip(SlotEnum slot, T item) {
        auto it = slots_.find(slot);
        if (it == slots_.end()) return false;
        if (it->second.has_value()) return false;
        it->second = std::move(item);
        return true;
    }

    std::optional<T> unequip(SlotEnum slot) {
        auto it = slots_.find(slot);
        if (it == slots_.end() || !it->second.has_value()) return std::nullopt;
        std::optional<T> taken = std::move(*it->second);
        it->second.reset();
        return taken;
    }

    const T* get(SlotEnum slot) const {
        auto it = slots_.find(slot);
        if (it == slots_.end() || !it->second.has_value()) return nullptr;
        return &*it->second;
    }

    T* get(SlotEnum slot) {
        auto it = slots_.find(slot);
        if (it == slots_.end() || !it->second.has_value()) return nullptr;
        return &*it->second;
    }

    bool isEmpty(SlotEnum slot) const {
        auto it = slots_.find(slot);
        return it == slots_.end() || !it->second.has_value();
    }

    bool hasSlot(SlotEnum slot) const {
        return slots_.find(slot) != slots_.end();
    }

    size_t size() const { return slots_.size(); }

private:
    std::map<SlotEnum, std::optional<T>> slots_;
};

void equipSlotsSelfTest();

} // namespace engine

#endif
