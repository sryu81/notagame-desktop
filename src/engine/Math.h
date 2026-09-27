#ifndef NOT_A_GAME_MATH_H
#define NOT_A_GAME_MATH_H

#include <cmath>
#include <cstdint>

namespace math {

struct Vec2;
struct Vec3;
struct Vec4;
struct Mat4;
struct Quat;

inline constexpr float PI = 3.14159265358979323846f;
inline constexpr float TWO_PI = 2.0f * PI;
inline constexpr float HALF_PI = 0.5f * PI;

inline constexpr float radians(float d) { return d * (PI / 180.0f); }
inline constexpr float degrees(float r) { return r * (180.0f / PI); }
inline constexpr float clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
inline constexpr float saturate(float v) { return clamp(v, 0.0f, 1.0f); }
inline constexpr float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline constexpr float sign(float v) {
    return v < 0.0f ? -1.0f : (v > 0.0f ? 1.0f : 0.0f);
}
inline bool approxEqual(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

struct Vec2 {
    float x;
    float y;

    constexpr Vec2() : x(0.0f), y(0.0f) {}
    constexpr Vec2(float s) : x(s), y(s) {}
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}

    constexpr Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator-() const { return {-x, -y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vec2 operator/(float s) const { return {x / s, y / s}; }
    constexpr Vec2 operator*(const Vec2& o) const { return {x * o.x, y * o.y}; }

    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
    Vec2& operator/=(float s) { x /= s; y /= s; return *this; }
    Vec2& operator*=(const Vec2& o) { x *= o.x; y *= o.y; return *this; }

    constexpr float dot(const Vec2& o) const { return x * o.x + y * o.y; }
    constexpr float lengthSq() const { return x * x + y * y; }
    float length() const { return std::sqrt(lengthSq()); }
    Vec2 normalized() const { float l = length(); return l > 0.0f ? *this / l : *this; }
};

inline constexpr Vec2 operator*(float s, const Vec2& v) { return v * s; }
inline constexpr float dot(const Vec2& a, const Vec2& b) { return a.dot(b); }

struct Vec3 {
    float x;
    float y;
    float z;

    constexpr Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    constexpr Vec3(float s) : x(s), y(s), z(s) {}
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    constexpr Vec3(const Vec2& v, float z_) : x(v.x), y(v.y), z(z_) {}

    constexpr Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator-() const { return {-x, -y, -z}; }
    constexpr Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    constexpr Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    constexpr Vec3 operator/(const Vec3& o) const { return {x / o.x, y / o.y, z / o.z}; }

    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    Vec3& operator/=(float s) { x /= s; y /= s; z /= s; return *this; }
    Vec3& operator*=(const Vec3& o) { x *= o.x; y *= o.y; z *= o.z; return *this; }
    Vec3& operator/=(const Vec3& o) { x /= o.x; y /= o.y; z /= o.z; return *this; }

    constexpr float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    constexpr Vec3 cross(const Vec3& o) const {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }
    constexpr float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSq()); }
    Vec3 normalized() const { float l = length(); return l > 0.0f ? *this / l : *this; }
    constexpr Vec2 xy() const { return {x, y}; }
};

inline constexpr Vec3 operator*(float s, const Vec3& v) { return v * s; }
inline constexpr float dot(const Vec3& a, const Vec3& b) { return a.dot(b); }
inline constexpr Vec3 cross(const Vec3& a, const Vec3& b) { return a.cross(b); }
inline constexpr Vec3 mix(const Vec3& a, const Vec3& b, float t) {
    return a * (1.0f - t) + b * t;
}

struct Vec4 {
    float x;
    float y;
    float z;
    float w;

    constexpr Vec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    constexpr Vec4(float s) : x(s), y(s), z(s), w(s) {}
    constexpr Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    constexpr Vec4(const Vec3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
    constexpr Vec4(const Vec2& v, float z_, float w_) : x(v.x), y(v.y), z(z_), w(w_) {}

    constexpr Vec4 operator+(const Vec4& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    constexpr Vec4 operator-(const Vec4& o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    constexpr Vec4 operator-() const { return {-x, -y, -z, -w}; }
    constexpr Vec4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
    constexpr Vec4 operator/(float s) const { return {x / s, y / s, z / s, w / s}; }
    constexpr Vec4 operator*(const Vec4& o) const { return {x * o.x, y * o.y, z * o.z, w * o.w}; }

    Vec4& operator+=(const Vec4& o) { x += o.x; y += o.y; z += o.z; w += o.w; return *this; }
    Vec4& operator-=(const Vec4& o) { x -= o.x; y -= o.y; z -= o.z; w -= o.w; return *this; }
    Vec4& operator*=(float s) { x *= s; y *= s; z *= s; w *= s; return *this; }
    Vec4& operator/=(float s) { x /= s; y /= s; z /= s; w /= s; return *this; }
    Vec4& operator*=(const Vec4& o) { x *= o.x; y *= o.y; z *= o.z; w *= o.w; return *this; }

    constexpr float dot(const Vec4& o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
    constexpr Vec3 xyz() const { return {x, y, z}; }
};

inline constexpr Vec4 operator*(float s, const Vec4& v) { return v * s; }

inline bool approxEqual(const Vec2& a, const Vec2& b, float eps = 1e-5f) {
    return approxEqual(a.x, b.x, eps) && approxEqual(a.y, b.y, eps);
}

inline bool approxEqual(const Vec3& a, const Vec3& b, float eps = 1e-5f) {
    return approxEqual(a.x, b.x, eps) &&
           approxEqual(a.y, b.y, eps) &&
           approxEqual(a.z, b.z, eps);
}

inline bool approxEqual(const Vec4& a, const Vec4& b, float eps = 1e-5f) {
    return approxEqual(a.x, b.x, eps) &&
           approxEqual(a.y, b.y, eps) &&
           approxEqual(a.z, b.z, eps) &&
           approxEqual(a.w, b.w, eps);
}

struct Mat4 {
    float m[16];

    constexpr Mat4() : m{0.0f, 0.0f, 0.0f, 0.0f,
                         0.0f, 0.0f, 0.0f, 0.0f,
                         0.0f, 0.0f, 0.0f, 0.0f,
                         0.0f, 0.0f, 0.0f, 0.0f} {}
    explicit constexpr Mat4(float diag) : m{diag, 0.0f, 0.0f, 0.0f,
                                              0.0f, diag, 0.0f, 0.0f,
                                              0.0f, 0.0f, diag, 0.0f,
                                              0.0f, 0.0f, 0.0f, diag} {}

    static constexpr Mat4 identity() { return Mat4(1.0f); }

    static Mat4 translation(const Vec3& t) {
        Mat4 r = identity();
        r.m[12] = t.x;
        r.m[13] = t.y;
        r.m[14] = t.z;
        return r;
    }

    static Mat4 scale(const Vec3& s) {
        Mat4 r;
        r.m[0]  = s.x;
        r.m[5]  = s.y;
        r.m[10] = s.z;
        r.m[15] = 1.0f;
        return r;
    }

    static Mat4 rotationX(float rad);
    static Mat4 rotationY(float rad);
    static Mat4 rotationZ(float rad);
    static Mat4 rotation(const Quat& q);

    static Mat4 perspective(float fovYRadians, float aspect, float near, float far);
    static Mat4 orthographic(float halfHeight, float aspect, float near, float far);
    static Mat4 orthographic(float left, float right, float bottom, float top,
                              float near, float far);
    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up);
    static Mat4 planarShadowProjection(const Vec3& lightDir, float groundY = 0.0f);

    Mat4 operator*(const Mat4& o) const;
    Vec4 operator*(const Vec4& v) const;
    Mat4& operator*=(const Mat4& o) { *this = *this * o; return *this; }

    Vec3 transformPoint(const Vec3& v) const;
    Vec3 transformDirection(const Vec3& v) const;

    Mat4 transposed() const;
    Mat4 inverted() const;
};

struct Quat {
    float x;
    float y;
    float z;
    float w;

    constexpr Quat() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
    constexpr Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    static constexpr Quat identity() { return Quat(); }

    static Quat fromAxisAngle(const Vec3& axis, float rad);
    static Quat fromRotationMatrix(const Mat4& m);

    constexpr Quat operator+(const Quat& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    constexpr Quat operator-() const { return {-x, -y, -z, -w}; }
    constexpr Quat operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
    Quat operator*(const Quat& o) const;
    Vec3 operator*(const Vec3& v) const;

    constexpr float dot(const Quat& o) const { return x * o.x + y * o.y + z * o.z + w * o.w; }
    constexpr float lengthSq() const { return x * x + y * y + z * z + w * w; }
    float length() const { return std::sqrt(lengthSq()); }
    Quat normalized() const { float l = length(); return l > 0.0f ? *this * (1.0f / l) : *this; }
    Quat conjugate() const { return {-x, -y, -z, w}; }
    Quat inverse() const {
        float s = lengthSq();
        return s > 0.0f ? conjugate() * (1.0f / s) : *this;
    }

    Mat4 toMat4() const;

    static Quat slerp(const Quat& a, const Quat& b, float t);
    static Quat nlerp(const Quat& a, const Quat& b, float t);
};

inline Quat operator*(float s, const Quat& q) { return q * s; }

void selfTest();

}  // namespace math

#endif  // NOT_A_GAME_MATH_H
