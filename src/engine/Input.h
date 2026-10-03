#ifndef NOT_A_GAME_INPUT_H
#define NOT_A_GAME_INPUT_H

struct PointerDownEvent {
    int pointerId;
    float x;
    float y;
    PointerDownEvent(int id, float x_, float y_) : pointerId(id), x(x_), y(y_) {}
};

struct PointerUpEvent {
    int pointerId;
    float x;
    float y;
    PointerUpEvent(int id, float x_, float y_) : pointerId(id), x(x_), y(y_) {}
};

struct PointerMoveEvent {
    int pointerId;
    float x;
    float y;
    PointerMoveEvent(int id, float x_, float y_) : pointerId(id), x(x_), y(y_) {}
};

struct TapEvent {
    int pointerId;
    float x;
    float y;
    TapEvent(int id, float x_, float y_) : pointerId(id), x(x_), y(y_) {}
};

struct HoverEvent {
    float x;
    float y;
    HoverEvent(float x_, float y_) : x(x_), y(y_) {}
};

struct KeyDownEvent {
    int keyCode;
    explicit KeyDownEvent(int code) : keyCode(code) {}
};

struct KeyUpEvent {
    int keyCode;
    explicit KeyUpEvent(int code) : keyCode(code) {}
};

struct GamepadButtonEvent {
    int buttonCode;
    bool down;
    GamepadButtonEvent(int code, bool d) : buttonCode(code), down(d) {}
};

namespace input {
void selfTest();
}

#endif
