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

#include "axmol/math/Vec2.h"
#include "axmol/base/Macros.h"

#include <Magnum/Math/Complex.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Math/Vector2.h>

NS_AX_MATH_BEGIN

#if defined(AX_DLLEXPORT) || defined(AX_DLLIMPORT)
const Vec2 Vec2::zero(0.0f, 0.0f);
const Vec2 Vec2::one(1.0f, 1.0f);
const Vec2 Vec2::xAxis(1.0f, 0.0f);
const Vec2 Vec2::yAxis(0.0f, 1.0f);
#endif

// returns true if segment A-B intersects with segment C-D. S->E is the overlap part
bool isOneDimensionSegmentOverlap(float A, float B, float C, float D, float* S, float* E)
{
    float ABmin = std::min(A, B);
    float ABmax = std::max(A, B);
    float CDmin = std::min(C, D);
    float CDmax = std::max(C, D);

    if (ABmax < CDmin || CDmax < ABmin)
    {
        // ABmin->ABmax->CDmin->CDmax or CDmin->CDmax->ABmin->ABmax
        return false;
    }
    else
    {
        if (ABmin >= CDmin && ABmin <= CDmax)
        {
            // CDmin->ABmin->CDmax->ABmax or CDmin->ABmin->ABmax->CDmax
            if (S != nullptr)
                *S = ABmin;
            if (E != nullptr)
                *E = CDmax < ABmax ? CDmax : ABmax;
        }
        else if (ABmax >= CDmin && ABmax <= CDmax)
        {
            // ABmin->CDmin->ABmax->CDmax
            if (S != nullptr)
                *S = CDmin;
            if (E != nullptr)
                *E = ABmax;
        }
        else
        {
            // ABmin->CDmin->CDmax->ABmax
            if (S != nullptr)
                *S = CDmin;
            if (E != nullptr)
                *E = CDmax;
        }
        return true;
    }
}

// cross product of 2 vector. A->B X C->D
float crossProduct2Vector(const Vec2& A, const Vec2& B, const Vec2& C, const Vec2& D)
{
    return (D.y - C.y) * (B.x - A.x) - (D.x - C.x) * (B.y - A.y);
}

float Vec2::angle(const Vec2& v1, const Vec2& v2)
{
    const Magnum::Vector2 a{v1.x, v1.y};
    const Magnum::Vector2 b{v2.x, v2.y};
    return std::atan2(std::abs(Magnum::Math::cross(a, b)) + MATH_FLOAT_SMALL, Magnum::Math::dot(a, b));
}

void Vec2::add(const Vec2& v1, const Vec2& v2, Vec2* dst)
{
    AX_ASSERT(dst);

    dst->x = v1.x + v2.x;
    dst->y = v1.y + v2.y;
}

void Vec2::clamp(const Vec2& min, const Vec2& max)
{
    AX_ASSERT(!(min.x > max.x || min.y > max.y));

    const Magnum::Vector2 result =
        Magnum::Math::clamp(Magnum::Vector2{x, y}, Magnum::Vector2{min.x, min.y}, Magnum::Vector2{max.x, max.y});
    x = result.x();
    y = result.y();
}

void Vec2::clamp(const Vec2& val, const Vec2& min, const Vec2& max, Vec2* dst)
{
    AX_ASSERT(dst);
    AX_ASSERT(!(min.x > max.x || min.y > max.y));

    const Magnum::Vector2 result = Magnum::Math::clamp(Magnum::Vector2{val.x, val.y}, Magnum::Vector2{min.x, min.y},
                                                       Magnum::Vector2{max.x, max.y});
    dst->x                       = result.x();
    dst->y                       = result.y();
}

float Vec2::distance(const Vec2& val) const
{
    return (Magnum::Vector2{val.x, val.y} - Magnum::Vector2{x, y}).length();
}

float Vec2::dot(const Vec2& v1, const Vec2& v2)
{
    return v1.x * v2.x + v1.y * v2.y;
}

float Vec2::length() const
{
    return Magnum::Vector2{x, y}.length();
}

void Vec2::normalize()
{
    const Magnum::Vector2 value{x, y};
    float n = value.dot();
    // Already normalized.
    if (n == 1.0f)
        return;

    n = std::sqrt(n);
    // Too close to zero.
    if (n < MATH_TOLERANCE)
        return;

    const Magnum::Vector2 result = value * (1.0f / n);
    x                            = result.x();
    y                            = result.y();
}

Vec2 Vec2::getNormalized() const
{
    Vec2 val(*this);
    val.normalize();
    return val;
}

void Vec2::rotate(const Vec2& point, float angle)
{
    const Magnum::Vector2 pivot{point.x, point.y};
    const Magnum::Vector2 result =
        Magnum::Complex::rotation(Magnum::Rad{angle}).transformVector(Magnum::Vector2{x, y} - pivot) + pivot;
    x = result.x();
    y = result.y();
}

void Vec2::subtract(const Vec2& v1, const Vec2& v2, Vec2* dst)
{
    AX_ASSERT(dst);

    dst->x = v1.x - v2.x;
    dst->y = v1.y - v2.y;
}

bool Vec2::fuzzyEquals(const Vec2& b, float var) const
{
    if (x - var <= b.x && b.x <= x + var)
        if (y - var <= b.y && b.y <= y + var)
            return true;
    return false;
}

float Vec2::getAngle(const Vec2& other) const
{
    Vec2 a2     = getNormalized();
    Vec2 b2     = other.getNormalized();
    float angle = atan2f(a2.cross(b2), a2.dot(b2));
    if (std::abs(angle) < FLT_EPSILON)
        return 0.f;
    return angle;
}

Vec2 Vec2::rotateByAngle(const Vec2& pivot, float angle) const
{
    return pivot + (*this - pivot).rotate(Vec2::forAngle(angle));
}

bool Vec2::isLineIntersect(const Vec2& A, const Vec2& B, const Vec2& C, const Vec2& D, float* S, float* T)
{
    // FAIL: Line undefined
    if ((A.x == B.x && A.y == B.y) || (C.x == D.x && C.y == D.y))
    {
        return false;
    }

    const float denom = crossProduct2Vector(A, B, C, D);

    if (denom == 0)
    {
        // Lines parallel or overlap
        return false;
    }

    if (S != nullptr)
        *S = crossProduct2Vector(C, D, C, A) / denom;
    if (T != nullptr)
        *T = crossProduct2Vector(A, B, C, A) / denom;

    return true;
}

bool Vec2::isLineParallel(const Vec2& A, const Vec2& B, const Vec2& C, const Vec2& D)
{
    // FAIL: Line undefined
    if ((A.x == B.x && A.y == B.y) || (C.x == D.x && C.y == D.y))
    {
        return false;
    }

    if (crossProduct2Vector(A, B, C, D) == 0)
    {
        // line overlap
        if (crossProduct2Vector(C, D, C, A) == 0 || crossProduct2Vector(A, B, C, A) == 0)
        {
            return false;
        }

        return true;
    }

    return false;
}

bool Vec2::isLineOverlap(const Vec2& A, const Vec2& B, const Vec2& C, const Vec2& D)
{
    // FAIL: Line undefined
    if ((A.x == B.x && A.y == B.y) || (C.x == D.x && C.y == D.y))
    {
        return false;
    }

    if (crossProduct2Vector(A, B, C, D) == 0 &&
        (crossProduct2Vector(C, D, C, A) == 0 || crossProduct2Vector(A, B, C, A) == 0))
    {
        return true;
    }

    return false;
}

bool Vec2::isSegmentOverlap(const Vec2& A, const Vec2& B, const Vec2& C, const Vec2& D, Vec2* S, Vec2* E)
{

    if (isLineOverlap(A, B, C, D))
    {
        return isOneDimensionSegmentOverlap(A.x, B.x, C.x, D.x, &S->x, &E->x) &&
               isOneDimensionSegmentOverlap(A.y, B.y, C.y, D.y, &S->y, &E->y);
    }

    return false;
}

bool Vec2::isSegmentIntersect(const Vec2& A, const Vec2& B, const Vec2& C, const Vec2& D)
{
    float S, T;

    if (isLineIntersect(A, B, C, D, &S, &T) && (S >= 0.0f && S <= 1.0f && T >= 0.0f && T <= 1.0f))
    {
        return true;
    }

    return false;
}

Vec2 Vec2::getIntersectPoint(const Vec2& A, const Vec2& B, const Vec2& C, const Vec2& D)
{
    float S, T;

    if (isLineIntersect(A, B, C, D, &S, &T))
    {
        // Vec2 of intersection
        Vec2 P;
        P.x = A.x + S * (B.x - A.x);
        P.y = A.y + S * (B.y - A.y);
        return P;
    }

    return Vec2::zero;
}

NS_AX_MATH_END
