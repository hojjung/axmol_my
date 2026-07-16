/**
 Copyright 2013 BlackBerry Inc.
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
 Copyright (c) 2019-present Axmol Engine contributors (see AUTHORS.md).

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

 This file was modified to fit the axmol project
 */

#include "axmol/math/Vec4.h"

#include <cmath>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Quaternion.h>
#include <Magnum/Math/Vector4.h>

#include "axmol/base/Macros.h"

NS_AX_MATH_BEGIN

namespace
{
Magnum::Vector4 toMagnum(const Vec4Base& value)
{
    return {value.x, value.y, value.z, value.w};
}

void assign(Vec4Base& destination, const Magnum::Vector4& value)
{
    destination.x = value.x();
    destination.y = value.y();
    destination.z = value.z();
    destination.w = value.w();
}
}  // namespace

#if defined(AX_DLLEXPORT) || defined(AX_DLLIMPORT)
const Vec4 Vec4::zero(0.0f, 0.0f, 0.0f, 0.0f);
const Vec4 Vec4::one(1.0f, 1.0f, 1.0f, 1.0f);
#endif

void Vec4Base::clamp(const Vec4Base& min, const Vec4Base& max)
{
    AX_ASSERT(!(min.x > max.x || min.y > max.y || min.z > max.z || min.w > max.w));

    assign(*this, Magnum::Math::clamp(toMagnum(*this), toMagnum(min), toMagnum(max)));
}

void Vec4Base::clamp(const Vec4Base& v, const Vec4Base& min, const Vec4Base& max, Vec4Base* dst)
{
    AX_ASSERT(dst);
    AX_ASSERT(!(min.x > max.x || min.y > max.y || min.z > max.z || min.w > max.w));

    assign(*dst, Magnum::Math::clamp(toMagnum(v), toMagnum(min), toMagnum(max)));
}

bool Vec4::isZero() const
{
    return x == 0.0f && y == 0.0f && z == 0.0f && w == 0.0f;
}

bool Vec4::isOne() const
{
    return x == 1.0f && y == 1.0f && z == 1.0f && w == 1.0f;
}

float Vec4::angle(const Vec4& v1, const Vec4& v2)
{
    const Magnum::Quaternion a{{v1.x, v1.y, v1.z}, v1.w};
    const Magnum::Quaternion b{{v2.x, v2.y, v2.z}, v2.w};
    const Magnum::Vector3 relative =
        a.scalar() * b.vector() - b.scalar() * a.vector() - Magnum::Math::cross(a.vector(), b.vector());
    return std::atan2(relative.length() + MATH_FLOAT_SMALL, Magnum::Math::dot(a, b));
}

void Vec4::add(const Vec4& v1, const Vec4& v2, Vec4* dst)
{
    AX_ASSERT(dst);

    dst->x = v1.x + v2.x;
    dst->y = v1.y + v2.y;
    dst->z = v1.z + v2.z;
    dst->w = v1.w + v2.w;
}

float Vec4::distance(const Vec4& val) const
{
    return (toMagnum(val) - toMagnum(*this)).length();
}

float Vec4::distanceSquared(const Vec4& val) const
{
    const float dx = val.x - x;
    const float dy = val.y - y;
    const float dz = val.z - z;
    const float dw = val.w - w;
    return dx * dx + dy * dy + dz * dz + dw * dw;
}

float Vec4::dot(const Vec4& val) const
{
    return x * val.x + y * val.y + z * val.z + w * val.w;
}

float Vec4::dot(const Vec4& v1, const Vec4& v2)
{
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
}

float Vec4::length() const
{
    return toMagnum(*this).length();
}

float Vec4::lengthSquared() const
{
    return x * x + y * y + z * z + w * w;
}

void Vec4::normalize()
{
    const Magnum::Vector4 value = toMagnum(*this);
    float n                     = value.dot();
    // Already normalized.
    if (n == 1.0f)
        return;

    n = std::sqrt(n);
    // Too close to zero.
    if (n < MATH_TOLERANCE)
        return;

    assign(*this, value * (1.0f / n));
}

Vec4 Vec4::getNormalized() const
{
    Vec4 val(*this);
    val.normalize();
    return val;
}

void Vec4::subtract(const Vec4& v1, const Vec4& v2, Vec4* dst)
{
    AX_ASSERT(dst);

    dst->x = v1.x - v2.x;
    dst->y = v1.y - v2.y;
    dst->z = v1.z - v2.z;
    dst->w = v1.w - v2.w;
}

NS_AX_MATH_END
