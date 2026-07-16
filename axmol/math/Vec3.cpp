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

#include "axmol/math/Vec3.h"
#include "axmol/base/Macros.h"

#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Vector3.h>

NS_AX_MATH_BEGIN

#if defined(AX_DLLEXPORT) || defined(AX_DLLIMPORT)
const Vec3 Vec3::zero(0.0f, 0.0f, 0.0f);
const Vec3 Vec3::one(1.0f, 1.0f, 1.0f);
const Vec3 Vec3::xAxis(1.0f, 0.0f, 0.0f);
const Vec3 Vec3::yAxis(0.0f, 1.0f, 0.0f);
const Vec3 Vec3::zAxis(0.0f, 0.0f, 1.0f);
#endif

float Vec3::angle(const Vec3& v1, const Vec3& v2)
{
    const Magnum::Vector3 a{v1.x, v1.y, v1.z};
    const Magnum::Vector3 b{v2.x, v2.y, v2.z};
    return std::atan2(Magnum::Math::cross(a, b).length() + MATH_FLOAT_SMALL, Magnum::Math::dot(a, b));
}

void Vec3::add(const Vec3& v1, const Vec3& v2, Vec3* dst)
{
    AX_ASSERT(dst);

    dst->x = v1.x + v2.x;
    dst->y = v1.y + v2.y;
    dst->z = v1.z + v2.z;
}

void Vec3::clamp(const Vec3& min, const Vec3& max)
{
    AX_ASSERT(!(min.x > max.x || min.y > max.y || min.z > max.z));

    const Magnum::Vector3 result = Magnum::Math::clamp(Magnum::Vector3{x, y, z}, Magnum::Vector3{min.x, min.y, min.z},
                                                       Magnum::Vector3{max.x, max.y, max.z});
    x                            = result.x();
    y                            = result.y();
    z                            = result.z();
}

void Vec3::clamp(const Vec3& v, const Vec3& min, const Vec3& max, Vec3* dst)
{
    AX_ASSERT(dst);
    AX_ASSERT(!(min.x > max.x || min.y > max.y || min.z > max.z));

    const Magnum::Vector3 result = Magnum::Math::clamp(
        Magnum::Vector3{v.x, v.y, v.z}, Magnum::Vector3{min.x, min.y, min.z}, Magnum::Vector3{max.x, max.y, max.z});
    dst->x = result.x();
    dst->y = result.y();
    dst->z = result.z();
}

void Vec3::cross(const Vec3& v)
{
    cross(*this, v, this);
}

void Vec3::cross(const Vec3& v1, const Vec3& v2, Vec3* dst)
{
    AX_ASSERT(dst);

    const Magnum::Vector3 result =
        Magnum::Math::cross(Magnum::Vector3{v1.x, v1.y, v1.z}, Magnum::Vector3{v2.x, v2.y, v2.z});
    dst->x = result.x();
    dst->y = result.y();
    dst->z = result.z();
}

float Vec3::distance(const Vec3& v) const
{
    return (Magnum::Vector3{v.x, v.y, v.z} - Magnum::Vector3{x, y, z}).length();
}

float Vec3::distanceSquared(const Vec3& v) const
{
    const float dx = v.x - x;
    const float dy = v.y - y;
    const float dz = v.z - z;
    return dx * dx + dy * dy + dz * dz;
}

float Vec3::dot(const Vec3& v) const
{
    return x * v.x + y * v.y + z * v.z;
}

float Vec3::dot(const Vec3& v1, const Vec3& v2)
{
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

void Vec3::normalize()
{
    const Magnum::Vector3 value{x, y, z};
    float n = value.dot();
    // Already normalized.
    if (n == 1.0f)
        return;

    n = std::sqrt(n);
    // Too close to zero.
    if (n < MATH_TOLERANCE)
        return;

    const Magnum::Vector3 result = value * (1.0f / n);
    x                            = result.x();
    y                            = result.y();
    z                            = result.z();
}

Vec3 Vec3::getNormalized() const
{
    Vec3 v(*this);
    v.normalize();
    return v;
}

void Vec3::subtract(const Vec3& v1, const Vec3& v2, Vec3* dst)
{
    AX_ASSERT(dst);

    dst->x = v1.x - v2.x;
    dst->y = v1.y - v2.y;
    dst->z = v1.z - v2.z;
}

void Vec3::smooth(const Vec3& target, float elapsedTime, float responseTime)
{
    if (elapsedTime > 0)
    {
        const Magnum::Vector3 result =
            Magnum::Math::lerp(Magnum::Vector3{x, y, z}, Magnum::Vector3{target.x, target.y, target.z},
                               elapsedTime / (elapsedTime + responseTime));
        x = result.x();
        y = result.y();
        z = result.z();
    }
}

NS_AX_MATH_END
