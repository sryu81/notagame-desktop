#ifndef NOT_A_GAME_EVENTBUS_H
#define NOT_A_GAME_EVENTBUS_H

#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <typeindex>
#include <utility>
#include <vector>

// Synchronous pub/sub for engine-wide events. Main thread only; no locks.
// Handlers are stored per-event-type and invoked in registration order.
// A handler may call EventBus::unsubscribe() on its own (or any other)
// subscription id; the unsubscribe is deferred until the current emit()
// returns, so the unsubscribing handler still runs to completion and
// other handlers in the same emit are unaffected.
class EventBus {
public:
    struct SubscriptionId {
        uint64_t value = 0;
        bool isValid() const { return value != 0; }
        bool operator==(SubscriptionId o) const { return value == o.value; }
        bool operator<(SubscriptionId o) const { return value < o.value; }
    };

    template <typename T>
    static SubscriptionId on(std::function<void(const T &)> handler) {
        assert(handler);
        auto wrapper = [h = std::move(handler)](const void *p) {
            h(*static_cast<const T *>(p));
        };
        return addHandler(typeid(T), std::move(wrapper));
    }

    template <typename T, typename... Args>
    static void emit(Args &&...args) {
        T event(std::forward<Args>(args)...);
        emitImpl(typeid(T), &event);
    }

    static void unsubscribe(SubscriptionId id);
    static void clear();
    static size_t handlerCount();

    static void selfTest();

private:
    struct Handler {
        SubscriptionId id;
        std::function<void(const void *)> fn;
    };

    static SubscriptionId addHandler(
            std::type_index type, std::function<void(const void *)> fn);
    static void emitImpl(std::type_index type, const void *eventPtr);

    inline static std::map<std::type_index, std::vector<Handler>> handlers_ = {};
    inline static std::set<SubscriptionId> dead_ = {};
    inline static uint64_t nextId_ = 1;
};

#endif
