#include "Input.h"

#include <cassert>

#include "AndroidOut.h"
#include "EventBus.h"

namespace input {

void selfTest() {
    aout << "[input] self-test starting" << std::endl;

    EventBus::clear();

    {
        int count = 0;
        int lastId = -1;
        float lastX = 0.0f;
        float lastY = 0.0f;
        EventBus::on<PointerDownEvent>([&](const PointerDownEvent &e) {
            count++;
            lastId = e.pointerId;
            lastX = e.x;
            lastY = e.y;
        });
        EventBus::emit<PointerDownEvent>(7, 123.5f, 456.75f);
        assert(count == 1);
        assert(lastId == 7);
        assert(lastX == 123.5f);
        assert(lastY == 456.75f);
    }
    EventBus::clear();

    {
        int upCount = 0;
        int moveCount = 0;
        EventBus::on<PointerUpEvent>([&](const PointerUpEvent &) { upCount++; });
        EventBus::on<PointerMoveEvent>([&](const PointerMoveEvent &) { moveCount++; });
        EventBus::emit<PointerUpEvent>(1, 50.0f, 75.0f);
        EventBus::emit<PointerMoveEvent>(1, 60.0f, 80.0f);
        EventBus::emit<PointerMoveEvent>(1, 70.0f, 85.0f);
        assert(upCount == 1);
        assert(moveCount == 2);
    }
    EventBus::clear();

    {
        int downKeyCount = 0;
        int upKeyCount = 0;
        int lastDownCode = -1;
        int lastUpCode = -1;
        EventBus::on<KeyDownEvent>([&](const KeyDownEvent &e) {
            downKeyCount++;
            lastDownCode = e.keyCode;
        });
        EventBus::on<KeyUpEvent>([&](const KeyUpEvent &e) {
            upKeyCount++;
            lastUpCode = e.keyCode;
        });
        EventBus::emit<KeyDownEvent>(66);
        EventBus::emit<KeyUpEvent>(66);
        assert(downKeyCount == 1);
        assert(upKeyCount == 1);
        assert(lastDownCode == 66);
        assert(lastUpCode == 66);
    }
    EventBus::clear();

    {
        int count = 0;
        int lastCode = -1;
        bool lastDown = false;
        EventBus::on<GamepadButtonEvent>([&](const GamepadButtonEvent &e) {
            count++;
            lastCode = e.buttonCode;
            lastDown = e.down;
        });
        EventBus::emit<GamepadButtonEvent>(96, true);
        assert(count == 1);
        assert(lastCode == 96);
        assert(lastDown);
        EventBus::emit<GamepadButtonEvent>(96, false);
        assert(count == 2);
        assert(!lastDown);
    }
    EventBus::clear();

    aout << "[input] self-test PASS" << std::endl;
}

}
