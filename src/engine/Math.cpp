#include "Math.h"

#include <cmath>
#include <cassert>

#include "AndroidOut.h"

namespace math {

Mat4 Mat4::rotationX(float rad) {
    Mat4 r = identity();
    float c = std::cos(rad);
    float s = std::sin(rad);
    r.m[5]  =  c;
    r.m[6]  =  s;
    r.m[9]  = -s;
    r.m[10] =  c;
    return r;
}

Mat4 Mat4::rotationY(float rad) {
    Mat4 r = identity();
    float c = std::cos(rad);
    float s = std::sin(rad);
    r.m[0]  =  c;
    r.m[2]  = -s;
    r.m[8]  =  s;
    r.m[10] =  c;
    return r;
}

Mat4 Mat4::rotationZ(float rad) {
    Mat4 r = identity();
    float c = std::cos(rad);
    float s = std::sin(rad);
    r.m[0] =  c;
    r.m[1] =  s;
    r.m[4] = -s;
    r.m[5] =  c;
    return r;
}

Mat4 Mat4::rotation(const Quat& q) { return q.toMat4(); }

Mat4 Mat4::perspective(float fovYRadians, float aspect, float near, float far) {
    Mat4 r;
    float f = 1.0f / std::tan(fovYRadians * 0.5f);
    r.m[0]  = f / aspect;
    r.m[5]  = f;
    r.m[10] = (far + near) / (near - far);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * far * near) / (near - far);
    return r;
}

Mat4 Mat4::orthographic(float halfHeight, float aspect, float near, float far) {
    float halfWidth = halfHeight * aspect;
    Mat4 r;
    r.m[0]  = 1.0f / halfWidth;
    r.m[5]  = 1.0f / halfHeight;
    r.m[10] = -2.0f / (far - near);
    r.m[14] = -(far + near) / (far - near);
    r.m[15] = 1.0f;
    return r;
}

Mat4 Mat4::orthographic(float left, float right, float bottom, float top,
                        float near, float far) {
    Mat4 r;
    r.m[0]  =  2.0f / (right - left);
    r.m[5]  =  2.0f / (top - bottom);
    r.m[10] = -2.0f / (far - near);
    r.m[12] = -(right + left) / (right - left);
    r.m[13] = -(top + bottom) / (top - bottom);
    r.m[14] = -(far + near) / (far - near);
    r.m[15] = 1.0f;
    return r;
}

Mat4 Mat4::lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
    Vec3 f = (target - eye).normalized();
    Vec3 s = cross(f, up).normalized();
    Vec3 u = cross(s, f);

    Mat4 r;
    r.m[0]  =  s.x;  r.m[1]  =  u.x;  r.m[2]  = -f.x;  r.m[3]  =  0;
    r.m[4]  =  s.y;  r.m[5]  =  u.y;  r.m[6]  = -f.y;  r.m[7]  =  0;
    r.m[8]  =  s.z;  r.m[9]  =  u.z;  r.m[10] = -f.z;  r.m[11] =  0;
    r.m[12] = -dot(s, eye);
    r.m[13] = -dot(u, eye);
    r.m[14] =  dot(f, eye);
    r.m[15] =  1.0f;
    return r;
}

Mat4 Mat4::planarShadowProjection(const Vec3& lightDir, float groundY) {
    // Projects onto y=groundY plane along -lightDir.
    float invLy = 1.0f / lightDir.y;
    float lxDivLy = lightDir.x * invLy;
    float lzDivLy = lightDir.z * invLy;
    Mat4 r = identity();
    r.m[4]  = -lxDivLy;
    r.m[6]  = -lzDivLy;
    r.m[12] = groundY * lxDivLy;
    r.m[13] = groundY;
    r.m[14] = groundY * lzDivLy;
    return r;
}

Mat4 Mat4::operator*(const Mat4& o) const {
    Mat4 r;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) {
                s += m[k * 4 + row] * o.m[col * 4 + k];
            }
            r.m[col * 4 + row] = s;
        }
    }
    return r;
}

Vec4 Mat4::operator*(const Vec4& v) const {
    return {
        m[0]  * v.x + m[4]  * v.y + m[8]  * v.z + m[12] * v.w,
        m[1]  * v.x + m[5]  * v.y + m[9]  * v.z + m[13] * v.w,
        m[2]  * v.x + m[6]  * v.y + m[10] * v.z + m[14] * v.w,
        m[3]  * v.x + m[7]  * v.y + m[11] * v.z + m[15] * v.w
    };
}

Vec3 Mat4::transformPoint(const Vec3& v) const {
    Vec4 r = *this * Vec4(v, 1.0f);
    float invW = r.w != 0.0f ? 1.0f / r.w : 1.0f;
    return r.xyz() * invW;
}

Vec3 Mat4::transformDirection(const Vec3& v) const {
    return (*this * Vec4(v, 0.0f)).xyz();
}

Mat4 Mat4::transposed() const {
    Mat4 r;
    r.m[0]  = m[0];  r.m[1]  = m[4];  r.m[2]  = m[8];  r.m[3]  = m[12];
    r.m[4]  = m[1];  r.m[5]  = m[5];  r.m[6]  = m[9];  r.m[7]  = m[13];
    r.m[8]  = m[2];  r.m[9]  = m[6];  r.m[10] = m[10]; r.m[11] = m[14];
    r.m[12] = m[3];  r.m[13] = m[7];  r.m[14] = m[11]; r.m[15] = m[15];
    return r;
}

Mat4 Mat4::inverted() const {
    const float* a = m;
    float inv[16];
    float det;

    inv[0]  =  a[5]*a[10]*a[15] - a[5]*a[11]*a[14] - a[9]*a[6]*a[15]
             + a[9]*a[7]*a[14]  + a[13]*a[6]*a[11] - a[13]*a[7]*a[10];
    inv[1]  = -a[1]*a[10]*a[15] + a[1]*a[11]*a[14] + a[9]*a[2]*a[15]
             - a[9]*a[3]*a[14]  - a[13]*a[2]*a[11] + a[13]*a[3]*a[10];
    inv[2]  =  a[1]*a[6]*a[15]  - a[1]*a[7]*a[14]  - a[5]*a[2]*a[15]
             + a[5]*a[3]*a[14]  + a[13]*a[2]*a[7]  - a[13]*a[3]*a[6];
    inv[3]  = -a[1]*a[6]*a[11]  + a[1]*a[7]*a[10]  + a[5]*a[2]*a[11]
             - a[5]*a[3]*a[10]  - a[9]*a[2]*a[7]   + a[9]*a[3]*a[6];

    inv[4]  = -a[4]*a[10]*a[15] + a[4]*a[11]*a[14] + a[8]*a[6]*a[15]
             - a[8]*a[7]*a[14]  - a[12]*a[6]*a[11] + a[12]*a[7]*a[10];
    inv[5]  =  a[0]*a[10]*a[15] - a[0]*a[11]*a[14] - a[8]*a[2]*a[15]
             + a[8]*a[3]*a[14]  + a[12]*a[2]*a[11] - a[12]*a[3]*a[10];
    inv[6]  = -a[0]*a[6]*a[15]  + a[0]*a[7]*a[14]  + a[4]*a[2]*a[15]
             - a[4]*a[3]*a[14]  - a[12]*a[2]*a[7]  + a[12]*a[3]*a[6];
    inv[7]  =  a[0]*a[6]*a[11]  - a[0]*a[7]*a[10]  - a[4]*a[2]*a[11]
             + a[4]*a[3]*a[10]  + a[8]*a[2]*a[7]   - a[8]*a[3]*a[6];

    inv[8]  =  a[4]*a[9]*a[15]  - a[4]*a[11]*a[13] - a[8]*a[5]*a[15]
             + a[8]*a[7]*a[13]  + a[12]*a[5]*a[11] - a[12]*a[7]*a[9];
    inv[9]  = -a[0]*a[9]*a[15]  + a[0]*a[11]*a[13] + a[8]*a[1]*a[15]
             - a[8]*a[3]*a[13]  - a[12]*a[1]*a[11] + a[12]*a[3]*a[9];
    inv[10] =  a[0]*a[5]*a[15]  - a[0]*a[7]*a[13]  - a[4]*a[1]*a[15]
             + a[4]*a[3]*a[13]  + a[12]*a[1]*a[7]  - a[12]*a[3]*a[5];
    inv[11] = -a[0]*a[5]*a[11]  + a[0]*a[7]*a[9]   + a[4]*a[1]*a[11]
             - a[4]*a[3]*a[9]   - a[8]*a[1]*a[7]   + a[8]*a[3]*a[5];

    inv[12] = -a[4]*a[9]*a[14]  + a[4]*a[10]*a[13] + a[8]*a[5]*a[14]
             - a[8]*a[6]*a[13]  - a[12]*a[5]*a[10] + a[12]*a[6]*a[9];
    inv[13] =  a[0]*a[9]*a[14]  - a[0]*a[10]*a[13] - a[8]*a[1]*a[14]
             + a[8]*a[2]*a[13]  + a[12]*a[1]*a[10] - a[12]*a[2]*a[9];
    inv[14] = -a[0]*a[5]*a[14]  + a[0]*a[6]*a[13]  + a[4]*a[1]*a[14]
             - a[4]*a[2]*a[13]  - a[12]*a[1]*a[6]  + a[12]*a[2]*a[5];
    inv[15] =  a[0]*a[5]*a[10]  - a[0]*a[6]*a[9]   - a[4]*a[1]*a[10]
             + a[4]*a[2]*a[9]   + a[8]*a[1]*a[6]   - a[8]*a[2]*a[5];

    det = a[0]*inv[0] + a[1]*inv[4] + a[2]*inv[8] + a[3]*inv[12];
    assert(det != 0.0f && "Mat4::inverted: singular matrix");
    det = 1.0f / det;

    Mat4 r;
    for (int i = 0; i < 16; ++i) r.m[i] = inv[i] * det;
    return r;
}

Quat Quat::fromAxisAngle(const Vec3& axis, float rad) {
    float half = rad * 0.5f;
    float s = std::sin(half);
    float c = std::cos(half);
    Vec3 n = axis.normalized();
    return {n.x * s, n.y * s, n.z * s, c};
}

Quat Quat::fromRotationMatrix(const Mat4& m) {
    float trace = m.m[0] + m.m[5] + m.m[10];
    Quat q;
    if (trace > 0.0f) {
        float s = std::sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m.m[6] - m.m[9])  / s;
        q.y = (m.m[8] - m.m[2])  / s;
        q.z = (m.m[1] - m.m[4])  / s;
    } else if (m.m[0] > m.m[5] && m.m[0] > m.m[10]) {
        float s = std::sqrt(1.0f + m.m[0] - m.m[5] - m.m[10]) * 2.0f;
        q.w = (m.m[6] - m.m[9])  / s;
        q.x = 0.25f * s;
        q.y = (m.m[4] + m.m[1])  / s;
        q.z = (m.m[8] + m.m[2])  / s;
    } else if (m.m[5] > m.m[10]) {
        float s = std::sqrt(1.0f + m.m[5] - m.m[0] - m.m[10]) * 2.0f;
        q.w = (m.m[8] - m.m[2])  / s;
        q.x = (m.m[4] + m.m[1])  / s;
        q.y = 0.25f * s;
        q.z = (m.m[9] + m.m[6])  / s;
    } else {
        float s = std::sqrt(1.0f + m.m[10] - m.m[0] - m.m[5]) * 2.0f;
        q.w = (m.m[1] - m.m[4])  / s;
        q.x = (m.m[8] + m.m[2])  / s;
        q.y = (m.m[9] + m.m[6])  / s;
        q.z = 0.25f * s;
    }
    return q.normalized();
}

Quat Quat::operator*(const Quat& o) const {
    return {
        w * o.x + x * o.w + y * o.z - z * o.y,
        w * o.y - x * o.z + y * o.w + z * o.x,
        w * o.z + x * o.y - y * o.x + z * o.w,
        w * o.w - x * o.x - y * o.y - z * o.z
    };
}

Vec3 Quat::operator*(const Vec3& v) const {
    Vec3 qv(x, y, z);
    Vec3 t = cross(qv, v) * 2.0f;
    return v + t * w + cross(qv, t);
}

Mat4 Quat::toMat4() const {
    float xx = x * x;
    float yy = y * y;
    float zz = z * z;
    float xy = x * y;
    float xz = x * z;
    float yz = y * z;
    float wx = w * x;
    float wy = w * y;
    float wz = w * z;

    Mat4 r = Mat4::identity();
    r.m[0]  = 1.0f - 2.0f * (yy + zz);
    r.m[1]  = 2.0f * (xy + wz);
    r.m[2]  = 2.0f * (xz - wy);

    r.m[4]  = 2.0f * (xy - wz);
    r.m[5]  = 1.0f - 2.0f * (xx + zz);
    r.m[6]  = 2.0f * (yz + wx);

    r.m[8]  = 2.0f * (xz + wy);
    r.m[9]  = 2.0f * (yz - wx);
    r.m[10] = 1.0f - 2.0f * (xx + yy);
    return r;
}

Quat Quat::slerp(const Quat& a, const Quat& b, float t) {
    Quat qb = b;
    float d = a.dot(b);
    if (d < -1e-4f) {
        qb = -b;
        d = -d;
    }
    d = clamp(d, -1.0f, 1.0f);
    if (d > 0.9995f) {
        return nlerp(a, qb, t);
    }
    float th = std::acos(d);
    float s  = std::sin(th);
    float fa = std::sin((1.0f - t) * th) / s;
    float fb = std::sin(t * th) / s;
    return a * fa + qb * fb;
}

Quat Quat::nlerp(const Quat& a, const Quat& b, float t) {
    return (a * (1.0f - t) + b * t).normalized();
}

void selfTest() {
    aout << "[math] self-test starting" << std::endl;

    Vec3 a(1.0f, 2.0f, 3.0f);
    Vec3 b(4.0f, 5.0f, 6.0f);
    assert(approxEqual(a.dot(b), 32.0f));
    assert(approxEqual(a.cross(b), Vec3(-3.0f, 6.0f, -3.0f)));
    assert(approxEqual(Vec3(3.0f, 0.0f, 4.0f).length(), 5.0f));
    assert(approxEqual(Vec3(3.0f, 0.0f, 4.0f).normalized(), Vec3(0.6f, 0.0f, 0.8f)));
    assert(approxEqual(cross(Vec3(1, 0, 0), Vec3(0, 1, 0)), Vec3(0, 0, 1)));

    Mat4 id = Mat4::identity();
    assert(approxEqual(id.m[0], 1.0f) && approxEqual(id.m[5], 1.0f) &&
           approxEqual(id.m[10], 1.0f) && approxEqual(id.m[15], 1.0f));

    Mat4 t = Mat4::translation(Vec3(1, 2, 3));
    Vec4 p(0, 0, 0, 1);
    assert(approxEqual(t * p, Vec4(1, 2, 3, 1)));
    assert(approxEqual(t.transformPoint(Vec3(0, 0, 0)), Vec3(1, 2, 3)));

    Mat4 s = Mat4::scale(Vec3(2, 3, 4));
    assert(approxEqual(s.transformPoint(Vec3(1, 1, 1)), Vec3(2, 3, 4)));

    Mat4 rx = Mat4::rotationX(HALF_PI);
    assert(approxEqual(rx.transformDirection(Vec3(0, 1, 0)), Vec3(0, 0, 1)));
    assert(approxEqual(rx.transformDirection(Vec3(0, 0, 1)), Vec3(0, -1, 0)));

    Mat4 ry = Mat4::rotationY(HALF_PI);
    assert(approxEqual(ry.transformDirection(Vec3(1, 0, 0)), Vec3(0, 0, -1)));
    assert(approxEqual(ry.transformDirection(Vec3(0, 0, 1)), Vec3(1, 0, 0)));

    Mat4 rz = Mat4::rotationZ(HALF_PI);
    assert(approxEqual(rz.transformDirection(Vec3(1, 0, 0)), Vec3(0, 1, 0)));

    Mat4 id2 = Mat4::identity();
    Mat4 tt  = Mat4::translation(Vec3(5, 0, 0));
    Mat4 composed = tt * id2;
    assert(approxEqual(composed.transformPoint(Vec3(1, 0, 0)), Vec3(6, 0, 0)));

    Mat4 eye = Mat4::identity();
    Mat4 invEye = eye.inverted();
    for (int i = 0; i < 16; ++i) {
        assert(approxEqual(eye.m[i], invEye.m[i]));
    }

    Mat4 tr = Mat4::translation(Vec3(7, 3, -2));
    Mat4 trInv = tr.inverted();
    Vec3 roundTrip = trInv.transformPoint(tr.transformPoint(Vec3(1, 1, 1)));
    assert(approxEqual(roundTrip, Vec3(1, 1, 1), 1e-4f));

    Mat4 trT = tr.transposed();
    assert(approxEqual(trT.m[12], 0.0f) && approxEqual(trT.m[13], 0.0f) &&
           approxEqual(trT.m[14], 0.0f));
    assert(approxEqual(trT.m[3], 7.0f) && approxEqual(trT.m[7], 3.0f) &&
           approxEqual(trT.m[11], -2.0f));

    Mat4 view = Mat4::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
    assert(approxEqual(view.transformPoint(Vec3(0, 0, 5)), Vec3(0, 0, 0), 1e-4f));
    assert(approxEqual(view.transformPoint(Vec3(1, 0, 0)), Vec3(1, 0, -5), 1e-4f));

    Mat4 proj = Mat4::perspective(radians(90.0f), 1.0f, 1.0f, 100.0f);
    Vec4 ndcCenter = proj * Vec4(0, 0, -1, 1);
    assert(approxEqual(ndcCenter.x, 0.0f, 1e-4f));
    assert(approxEqual(ndcCenter.y, 0.0f, 1e-4f));
    assert(approxEqual(ndcCenter.z / ndcCenter.w, -1.0f, 1e-4f));

    Mat4 idMat = Mat4::identity();
    Mat4 trans = Mat4::translation(Vec3(3, 4, 5));
    Mat4 idRight = trans * idMat;
    Mat4 idLeft  = idMat * trans;
    assert(approxEqual(idRight.transformPoint(Vec3(1, 1, 1)), Vec3(4, 5, 6), 1e-4f));
    assert(approxEqual(idLeft.transformPoint(Vec3(1, 1, 1)),  Vec3(4, 5, 6), 1e-4f));

    Mat4 perspAtCenter = Mat4::perspective(radians(90.0f), 1.0f, 1.0f, 100.0f);
    Vec4 ndcAtCenter = perspAtCenter * Vec4(0, 0, -1, 1);
    assert(approxEqual(ndcAtCenter.x, 0.0f, 1e-4f));
    assert(approxEqual(ndcAtCenter.y, 0.0f, 1e-4f));
    assert(approxEqual(ndcAtCenter.z / ndcAtCenter.w, -1.0f, 1e-4f));

    Mat4 lookAtView = Mat4::lookAt(Vec3(2, 3, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
    assert(approxEqual(lookAtView.transformPoint(Vec3(2, 3, 5)), Vec3(0, 0, 0), 1e-4f));

    Mat4 lookAtInv = lookAtView.inverted();
    Mat4 lookAtRoundTrip = lookAtView * lookAtInv;
    for (int i = 0; i < 16; ++i) {
        float expected = (i == 0 || i == 5 || i == 10 || i == 15) ? 1.0f : 0.0f;
        assert(approxEqual(lookAtRoundTrip.m[i], expected, 1e-4f));
    }

    Quat qid = Quat::identity();
    assert(approxEqual((qid * Vec3(1, 2, 3)).lengthSq(), 14.0f));
    assert(approxEqual((qid * Vec3(1, 2, 3)), Vec3(1, 2, 3)));

    Quat qy90 = Quat::fromAxisAngle(Vec3(0, 1, 0), HALF_PI);
    assert(approxEqual(qy90 * Vec3(1, 0, 0), Vec3(0, 0, -1), 1e-4f));
    assert(approxEqual(qy90 * Vec3(0, 0, 1), Vec3(1, 0, 0), 1e-4f));

    Quat qy180 = Quat::fromAxisAngle(Vec3(0, 1, 0), PI);
    assert(approxEqual(qy180 * Vec3(1, 0, 0), Vec3(-1, 0, 0), 1e-4f));

    Quat qx90 = Quat::fromAxisAngle(Vec3(1, 0, 0), HALF_PI);
    Quat qz90 = Quat::fromAxisAngle(Vec3(0, 0, 1), HALF_PI);
    Quat qCombined = qz90 * qx90;
    Vec3 v(1, 0, 0);
    Vec3 expectedFirst = qx90 * v;
    Vec3 expectedBoth  = qz90 * expectedFirst;
    assert(approxEqual(qCombined * v, expectedBoth, 1e-4f));

    Quat qa = Quat::identity();
    Quat qb = Quat::fromAxisAngle(Vec3(0, 1, 0), PI);
    Quat qMid = Quat::slerp(qa, qb, 0.5f);
    Vec3 mid = qMid * Vec3(1, 0, 0);
    assert(approxEqual(mid, Vec3(0, 0, -1), 1e-4f));

    Quat qStart = Quat::slerp(qa, qb, 0.0f);
    Quat qEnd   = Quat::slerp(qa, qb, 1.0f);
    assert(approxEqual(qStart * Vec3(1, 0, 0), Vec3(1, 0, 0), 1e-4f));
    assert(approxEqual(qEnd   * Vec3(1, 0, 0), Vec3(-1, 0, 0), 1e-4f));

    Quat qMat = Quat::fromRotationMatrix(qy90.toMat4());
    assert(approxEqual(qMat * Vec3(1, 0, 0), Vec3(0, 0, -1), 1e-3f));

    aout << "[math] self-test PASS" << std::endl;
}

}  // namespace math
