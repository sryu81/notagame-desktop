#include "EventBus.h"

#include <algorithm>

#include "AndroidOut.h"

EventBus::SubscriptionId EventBus::addHandler(
        std::type_index type, std::function<void(const void *)> fn) {
    SubscriptionId id{nextId_++};
    handlers_[type].push_back(Handler{id, std::move(fn)});
    return id;
}

void EventBus::emitImpl(std::type_index type, const void *eventPtr) {
    auto it = handlers_.find(type);
    if (it == handlers_.end()) return;
    auto &handlers = it->second;
    // Iterate by index so the loop is safe across vector reallocations
    // triggered by handlers that register new handlers of the same type.
    for (size_t i = 0; i < handlers.size(); ++i) {
        if (dead_.count(handlers[i].id)) continue;
        handlers[i].fn(eventPtr);
    }
    handlers.erase(
            std::remove_if(
                    handlers.begin(), handlers.end(),
                    [](const Handler &h) {
                        if (dead_.count(h.id)) { dead_.erase(h.id); return true; }
                        return false;
                    }),
            handlers.end());
}

void EventBus::unsubscribe(SubscriptionId id) {
    if (id.isValid()) {
        dead_.insert(id);
    }
}

void EventBus::clear() {
    handlers_.clear();
    dead_.clear();
}

size_t EventBus::handlerCount() {
    size_t n = 0;
    for (const auto &kv : handlers_) n += kv.second.size();
    return n;
}

void EventBus::selfTest() {
    aout << "[eventbus] self-test starting" << std::endl;

    clear();

    struct EventA {
        int value;
        explicit EventA(int v) : value(v) {}
    };

    // Test 1: two handlers for the same event run in registration order.
    {
        int callCount = 0;
        int firstOrder = -1;
        int secondOrder = -1;
        int observedValue = -1;
        auto sub1 = on<EventA>([&](const EventA &e) {
            firstOrder = ++callCount;
            observedValue = e.value;
        });
        auto sub2 = on<EventA>([&](const EventA &e) {
            secondOrder = ++callCount;
            assert(e.value == 42);
        });
        emit<EventA>(42);
        assert(firstOrder == 1);
        assert(secondOrder == 2);
        assert(observedValue == 42);
        assert(handlerCount() == 2);
        unsubscribe(sub1);
        unsubscribe(sub2);
    }
    clear();

    // Test 2: self-unsubscribing handler runs to completion, others run,
    // the dead handler is removed by the end of emit.
    {
        bool secondRan = false;
        bool thirdRan = false;
        SubscriptionId selfUnsubId;
        selfUnsubId = on<EventA>([&](const EventA &) { unsubscribe(selfUnsubId); });
        on<EventA>([&](const EventA &) { secondRan = true; });
        on<EventA>([&](const EventA &) { thirdRan = true; });
        emit<EventA>(1);
        assert(secondRan);
        assert(thirdRan);
        assert(handlerCount() == 2);
    }
    clear();

    // Test 3: emit with no handlers is a no-op.
    emit<EventA>(1);
    assert(handlerCount() == 0);

    // Test 4: handler registered during emit is called in the same emit.
    {
        int x = 0;
        on<EventA>([&](const EventA &) {
            x++;
            on<EventA>([&](const EventA &) { x += 10; });
        });
        emit<EventA>(0);
        assert(x == 11);
    }
    clear();

    aout << "[eventbus] self-test PASS" << std::endl;
}
