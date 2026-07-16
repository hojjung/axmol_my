/**
 Copyright 2013 BlackBerry Inc.
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 Original file from GamePlay3D: http://gameplay3d.org

 This file was modified to fit the cocos2d-x project
 */

#include "axmol/math/Quat.h"

#include <cmath>

#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Quaternion.h>

#include "axmol/base/Macros.h"

NS_AX_MATH_BEGIN

namespace
{
Magnum::Quaternion toMagnum(const Quat& value)
{
    return {{value.x, value.y, value.z}, value.w};
}

void assign(Quat& destination, const Magnum::Quaternion& value)
{
    destination.x = value.vector().x();
    destination.y = value.vector().y();
    destination.z = value.vector().z();
    destination.w = value.scalar();
}

Magnum::Quaternion normalizedForInterpolation(const Magnum::Quaternion& value)
{
    const float lengthSquared = value.dot();
    if (lengthSquared < 0.000001f)
        return {{0.0f, 0.0f, 0.0f}, 1.0f};
    if (lengthSquared == 1.0f)
        return value;
    return value * (1.0f / std::sqrt(lengthSquared));
}
}  // namespace

#if defined(AX_DLLEXPORT) || defined(AX_DLLIMPORT)
const Quat Quat::zero(0.0f, 0.0f, 0.0f, 0.0f);
const Quat Quat::identity(0.0f, 0.0f, 0.0f, 1.0f);
#endif

Quat::Quat(const Mat4& m)
{
    set(m);
}

bool Quat::isIdentity() const
{
    return x == 0.0f && y == 0.0f && z == 0.0f && w == 1.0f;
}

bool Quat::isZero() const
{
    return x == 0.0f && y == 0.0f && z == 0.0f && w == 0.0f;
}

void Quat::createFromRotationMatrix(const Mat4& m, Quat* dst)
{
    m.getRotation(dst);
}

void Quat::conjugate()
{
    x = -x;
    y = -y;
    z = -z;
}

Quat Quat::getConjugated() const
{
    Quat q(*this);
    q.conjugate();
    return q;
}

bool Quat::inverse()
{
    const Magnum::Quaternion value = toMagnum(*this);
    const float n                  = value.dot();
    if (n == 1.0f)
    {
        assign(*this, value.conjugated());
        return true;
    }

    // Too close to zero.
    if (n < 0.000001f)
        return false;

    assign(*this, value.conjugated() * (1.0f / n));

    return true;
}

Quat Quat::getInversed() const
{
    Quat q(*this);
    q.inverse();
    return q;
}

void Quat::multiply(const Quat& q)
{
    multiply(*this, q, this);
}

void Quat::multiply(const Quat& q1, const Quat& q2, Quat* dst)
{
    AX_ASSERT(dst);
    assign(*dst, toMagnum(q1) * toMagnum(q2));
}

void Quat::normalize()
{
    const Magnum::Quaternion value = toMagnum(*this);
    float n                        = value.dot();

    // Already normalized.
    if (n == 1.0f)
        return;

    n = std::sqrt(n);
    // Too close to zero.
    if (n < 0.000001f)
        return;

    assign(*this, value * (1.0f / n));
}

Quat Quat::getNormalized() const
{
    Quat q(*this);
    q.normalize();
    return q;
}

void Quat::set(const Mat4& m)
{
    Quat::createFromRotationMatrix(m, this);
}

float Quat::toAxisAngle(Vec3* axis) const
{
    AX_ASSERT(axis);

    Magnum::Quaternion value  = toMagnum(*this);
    const float squaredLength = value.dot();
    const float length        = std::sqrt(squaredLength);
    if (squaredLength != 1.0f && length >= 0.000001f)
        value = value * (1.0f / length);

    const Magnum::Vector3 vector = value.vector();
    const float axisLength       = vector.length();
    if (axisLength < MATH_TOLERANCE)
    {
        axis->setZero();
    }
    else
    {
        const Magnum::Vector3 normalizedAxis = vector / axisLength;
        axis->set(normalizedAxis.x(), normalizedAxis.y(), normalizedAxis.z());
    }

    return 2.0f * std::acos(value.scalar());
}

void Quat::lerp(const Quat& q1, const Quat& q2, float t, Quat* dst)
{
    AX_ASSERT(dst);
    AX_ASSERT(!(t < 0.0f || t > 1.0f));

    if (t == 0.0f)
    {
        *dst = q1;
        return;
    }
    if (t == 1.0f)
    {
        *dst = q2;
        return;
    }

    const float inverseT = 1.0f - t;
    const Quat result{inverseT * q1.x + t * q2.x, inverseT * q1.y + t * q2.y, inverseT * q1.z + t * q2.z,
                      inverseT * q1.w + t * q2.w};
    *dst = result;
}

void Quat::slerp(const Quat& q1, const Quat& q2, float t, Quat* dst)
{
    AX_ASSERT(dst);
    slerp(q1.x, q1.y, q1.z, q1.w, q2.x, q2.y, q2.z, q2.w, t, &dst->x, &dst->y, &dst->z, &dst->w);
}

void Quat::squad(const Quat& q1, const Quat& q2, const Quat& s1, const Quat& s2, float t, Quat* dst)
{
    AX_ASSERT(!(t < 0.0f || t > 1.0f));

    Quat dstQ(0.0f, 0.0f, 0.0f, 1.0f);
    Quat dstS(0.0f, 0.0f, 0.0f, 1.0f);

    slerpForSquad(q1, q2, t, &dstQ);
    slerpForSquad(s1, s2, t, &dstS);
    slerpForSquad(dstQ, dstS, 2.0f * t * (1.0f - t), dst);
}

void Quat::slerp(float q1x,
                 float q1y,
                 float q1z,
                 float q1w,
                 float q2x,
                 float q2y,
                 float q2z,
                 float q2w,
                 float t,
                 float* dstx,
                 float* dsty,
                 float* dstz,
                 float* dstw)
{
    AX_ASSERT(dstx && dsty && dstz && dstw);
    AX_ASSERT(!(t < 0.0f || t > 1.0f));

    if (t == 0.0f)
    {
        *dstx = q1x;
        *dsty = q1y;
        *dstz = q1z;
        *dstw = q1w;
        return;
    }
    else if (t == 1.0f)
    {
        *dstx = q2x;
        *dsty = q2y;
        *dstz = q2z;
        *dstw = q2w;
        return;
    }

    if (q1x == q2x && q1y == q2y && q1z == q2z && q1w == q2w)
    {
        *dstx = q1x;
        *dsty = q1y;
        *dstz = q1z;
        *dstw = q1w;
        return;
    }

    const Magnum::Quaternion first = normalizedForInterpolation({{q1x, q1y, q1z}, q1w});
    const Magnum::Quaternion second = normalizedForInterpolation({{q2x, q2y, q2z}, q2w});
    const Magnum::Quaternion result = Magnum::Math::slerpShortestPath(first, second, t);
    *dstx                           = result.vector().x();
    *dsty                           = result.vector().y();
    *dstz                           = result.vector().z();
    *dstw                           = result.scalar();
}

void Quat::slerpForSquad(const Quat& q1, const Quat& q2, float t, Quat* dst)
{
    AX_ASSERT(dst);

    const Magnum::Quaternion first  = normalizedForInterpolation(toMagnum(q1));
    const Magnum::Quaternion second = normalizedForInterpolation(toMagnum(q2));
    const float c                   = Magnum::Math::dot(first, second);

    if (std::abs(c) >= 1.0f)
    {
        *dst = q1;
        return;
    }

    if (1.0f - c * c <= 1.0e-10f)
    {
        *dst = q1;
        return;
    }

    assign(*dst, Magnum::Math::slerp(first, second, t));
}

NS_AX_MATH_END
