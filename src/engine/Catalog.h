#ifndef NOT_A_GAME_CATALOG_H
#define NOT_A_GAME_CATALOG_H

#include <string>
#include <unordered_map>
#include <vector>

namespace engine {

template <typename T>
class Catalog {
public:
    using Id = std::string;

    void add(const Id& id, T value) {
        items_[id] = std::move(value);
    }

    bool has(const Id& id) const {
        return items_.find(id) != items_.end();
    }

    const T* find(const Id& id) const {
        auto it = items_.find(id);
        return it != items_.end() ? &it->second : nullptr;
    }

    T* find(const Id& id) {
        auto it = items_.find(id);
        return it != items_.end() ? &it->second : nullptr;
    }

    bool remove(const Id& id) {
        return items_.erase(id) > 0;
    }

    std::vector<T> all() const {
        std::vector<T> v;
        v.reserve(items_.size());
        for (const auto& kv : items_) v.push_back(kv.second);
        return v;
    }

    std::vector<T> drain() {
        std::vector<T> v;
        v.reserve(items_.size());
        for (auto& kv : items_) v.push_back(std::move(kv.second));
        items_.clear();
        return v;
    }

    size_t size() const { return items_.size(); }

    void clear() { items_.clear(); }

private:
    std::unordered_map<Id, T> items_;
};

void catalogSelfTest();

} // namespace engine

#endif
