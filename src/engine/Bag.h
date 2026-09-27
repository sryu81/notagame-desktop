#ifndef NOT_A_GAME_BAG_H
#define NOT_A_GAME_BAG_H

#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace engine {

template <typename T, typename Id = std::string>
class Bag {
public:
    struct Entry {
        Id id;
        T item;
        int count;
    };

    void add(const Id& id, T item, int count = 1) {
        assert(count > 0);
        for (auto& e : entries_) {
            if (e.id == id) {
                e.count += count;
                return;
            }
        }
        entries_.push_back({id, std::move(item), count});
    }

    bool remove(const Id& id, int count = 1) {
        assert(count > 0);
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->id == id) {
                if (it->count <= count) {
                    entries_.erase(it);
                } else {
                    it->count -= count;
                }
                return true;
            }
        }
        return false;
    }

    int countOf(const Id& id) const {
        for (const auto& e : entries_) {
            if (e.id == id) return e.count;
        }
        return 0;
    }

    const Entry* find(const Id& id) const {
        for (const auto& e : entries_) {
            if (e.id == id) return &e;
        }
        return nullptr;
    }

    int totalItems() const {
        int total = 0;
        for (const auto& e : entries_) total += e.count;
        return total;
    }

    const std::vector<Entry>& entries() const { return entries_; }
    size_t size() const { return entries_.size(); }
    bool empty() const { return entries_.empty(); }

    void clear() { entries_.clear(); }

private:
    std::vector<Entry> entries_;
};

void bagSelfTest();

} // namespace engine

#endif
