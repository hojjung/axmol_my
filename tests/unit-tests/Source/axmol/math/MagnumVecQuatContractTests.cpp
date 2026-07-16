#include <cstddef>
#include <type_traits>
#include <utility>

#include <doctest.h>

#include "axmol/math/Quat.h"
#include "axmol/math/Vec2.h"
#include "axmol/math/Vec3.h"
#include "axmol/math/Vec4.h"

using namespace ax;

namespace
{
constexpr float kTolerance = 0.00001f;

void checkVec2(const Vec2& actual, float x, float y)
{
    CHECK(actual.x == doctest::Approx(x).epsilon(kTolerance));
    CHECK(actual.y == doctest::Approx(y).epsilon(kTolerance));
}

void checkVec3(const Vec3& actual, float x, float y, float z)
{
    CHECK(actual.x == doctest::Approx(x).epsilon(kTolerance));
    CHECK(actual.y == doctest::Approx(y).epsilon(kTolerance));
    CHECK(actual.z == doctest::Approx(z).epsilon(kTolerance));
}

void checkVec4(const Vec4& actual, float x, float y, float z, float w)
{
    CHECK(actual.x == doctest::Approx(x).epsilon(kTolerance));
    CHECK(actual.y == doctest::Approx(y).epsilon(kTolerance));
    CHECK(actual.z == doctest::Approx(z).epsilon(kTolerance));
    CHECK(actual.w == doctest::Approx(w).epsilon(kTolerance));
}

void checkQuat(const Quat& actual, float x, float y, float z, float w)
{
    CHECK(actual.x == doctest::Approx(x).epsilon(kTolerance));
    CHECK(actual.y == doctest::Approx(y).epsilon(kTolerance));
    CHECK(actual.z == doctest::Approx(z).epsilon(kTolerance));
    CHECK(actual.w == doctest::Approx(w).epsilon(kTolerance));
}
}  // namespace

static_assert(sizeof(Vec2) == sizeof(float) * 2);
static_assert(sizeof(Vec3) == sizeof(float) * 3);
static_assert(sizeof(Vec4) == sizeof(float) * 4);
static_assert(sizeof(Quat) == sizeof(float) * 4);
static_assert(alignof(Vec2) == alignof(float));
static_assert(alignof(Vec3) == alignof(float));
static_assert(alignof(Vec4) == alignof(float));
static_assert(alignof(Quat) == alignof(float));
static_assert(std::is_standard_layout_v<Vec2>);
static_assert(std::is_standard_layout_v<Vec3>);
static_assert(std::is_standard_layout_v<Vec4>);
static_assert(std::is_standard_layout_v<Quat>);
static_assert(std::is_trivially_copyable_v<Vec2>);
static_assert(std::is_trivially_copyable_v<Vec3>);
static_assert(std::is_trivially_copyable_v<Vec4>);
static_assert(std::is_trivially_copyable_v<Quat>);
static_assert(offsetof(Vec2, x) == 0);
static_assert(offsetof(Vec2, y) == sizeof(float));
static_assert(offsetof(Vec3, x) == 0);
static_assert(offsetof(Vec3, y) == sizeof(float));
static_assert(offsetof(Vec3, z) == sizeof(float) * 2);
static_assert(offsetof(Vec4, x) == 0);
static_assert(offsetof(Vec4, y) == sizeof(float));
static_assert(offsetof(Vec4, z) == sizeof(float) * 2);
static_assert(offsetof(Vec4, w) == sizeof(float) * 3);
static_assert(offsetof(Quat, x) == 0);
static_assert(offsetof(Quat, y) == sizeof(float));
static_assert(offsetof(Quat, z) == sizeof(float) * 2);
static_assert(offsetof(Quat, w) == sizeof(float) * 3);
static_assert(std::is_same_v<decltype(-std::declval<const Vec4&>()), Vec4>);

TEST_SUITE("math/MagnumVecQuatContract")
{
    TEST_CASE("Vec2 preserves Axmol operations")
    {
        Vec2 value{3.0f, 4.0f};
        CHECK(value.length() == doctest::Approx(5.0f));
        CHECK(value.dot(Vec2{2.0f, -1.0f}) == doctest::Approx(2.0f));
        CHECK(value.cross(Vec2{2.0f, -1.0f}) == doctest::Approx(-11.0f));

        value.normalize();
        checkVec2(value, 0.6f, 0.8f);

        Vec2 rotated{2.0f, 1.0f};
        rotated.rotate(Vec2{1.0f, 1.0f}, 1.57079632679489661923f);
        checkVec2(rotated, 1.0f, 2.0f);

        checkVec2(Vec2{1.0f, 2.0f}.lerp(Vec2{5.0f, 10.0f}, 0.25f), 2.0f, 4.0f);

        Vec2 aliasedAdd{1.0f, 2.0f};
        Vec2::add(aliasedAdd, Vec2{3.0f, 4.0f}, &aliasedAdd);
        checkVec2(aliasedAdd, 4.0f, 6.0f);

        Vec2 aliasedSubtract{5.0f, 7.0f};
        Vec2::subtract(Vec2{9.0f, 11.0f}, aliasedSubtract, &aliasedSubtract);
        checkVec2(aliasedSubtract, 4.0f, 4.0f);
    }

    TEST_CASE("Vec3 cross product remains alias safe")
    {
        Vec3 value{1.0f, 2.0f, 3.0f};
        value.cross(Vec3{4.0f, 5.0f, 6.0f});
        checkVec3(value, -3.0f, 6.0f, -3.0f);

        CHECK(Vec3::dot(Vec3{1.0f, 2.0f, 3.0f}, Vec3{4.0f, 5.0f, 6.0f}) == doctest::Approx(32.0f));
        checkVec3(Vec3{0.0f, 0.0f, 0.0f}.getNormalized(), 0.0f, 0.0f, 0.0f);
        checkVec3(Vec3{1.0f, 2.0f, 3.0f}.lerp(Vec3{5.0f, 10.0f, 15.0f}, 0.25f), 2.0f, 4.0f, 6.0f);

        Vec3 aliasedDestination{4.0f, 5.0f, 6.0f};
        Vec3::cross(Vec3{1.0f, 2.0f, 3.0f}, aliasedDestination, &aliasedDestination);
        checkVec3(aliasedDestination, -3.0f, 6.0f, -3.0f);
    }

    TEST_CASE("Vec4 adapter keeps component and value semantics")
    {
        const Vec4 value{1.0f, -2.0f, 3.0f, -4.0f};
        checkVec4(-value, -1.0f, 2.0f, -3.0f, 4.0f);
        checkVec4(value + Vec4{3.0f, 4.0f, 5.0f, 6.0f}, 4.0f, 2.0f, 8.0f, 2.0f);
        checkVec4(value * 2.0f, 2.0f, -4.0f, 6.0f, -8.0f);
        CHECK(value.lengthSquared() == doctest::Approx(30.0f));
        CHECK(Vec4::angle(Vec4{0.0f, 0.0f, 0.0f, 1.0f}, Vec4{1.0f, 0.0f, 0.0f, 0.0f}) ==
              doctest::Approx(1.57079632679489661923f));

        Vec4 aliasedAdd{1.0f, 2.0f, 3.0f, 4.0f};
        Vec4::add(aliasedAdd, Vec4{5.0f, 6.0f, 7.0f, 8.0f}, &aliasedAdd);
        checkVec4(aliasedAdd, 6.0f, 8.0f, 10.0f, 12.0f);

        Vec4 aliasedSubtract{5.0f, 6.0f, 7.0f, 8.0f};
        Vec4::subtract(Vec4{9.0f, 10.0f, 11.0f, 12.0f}, aliasedSubtract, &aliasedSubtract);
        checkVec4(aliasedSubtract, 4.0f, 4.0f, 4.0f, 4.0f);
    }

    TEST_CASE("Quat uses Magnum rotation and composition")
    {
        Quat rotation;
        Quat::createFromAxisAngle(Vec3::zAxis, 1.57079632679489661923f, &rotation);
        checkVec3(rotation * Vec3::xAxis, 0.0f, 1.0f, 0.0f);

        const Quat inverse = rotation.getInversed();
        checkQuat(rotation * inverse, 0.0f, 0.0f, 0.0f, 1.0f);

        Quat aliasedProduct = rotation;
        Quat::multiply(aliasedProduct, inverse, &aliasedProduct);
        checkQuat(aliasedProduct, 0.0f, 0.0f, 0.0f, 1.0f);

        checkVec3(Quat{0.0f, 0.0f, 1.0f, 1.0f} * Vec3::xAxis, -1.0f, 2.0f, 0.0f);
        checkVec3(Quat::zero * Vec3{2.0f, 3.0f, 4.0f}, 2.0f, 3.0f, 4.0f);

        Quat zeroAxis;
        Quat::createFromAxisAngle(Vec3::zero, 1.0f, &zeroAxis);
        checkQuat(zeroAxis, 0.0f, 0.0f, 0.0f, 1.0f);

        Vec3 identityAxis{1.0f, 1.0f, 1.0f};
        CHECK(Quat::identity.toAxisAngle(&identityAxis) == doctest::Approx(0.0f));
        checkVec3(identityAxis, 0.0f, 0.0f, 0.0f);
    }

    TEST_CASE("Quat interpolation preserves Axmol endpoint and lerp contracts")
    {
        const Quat first = Quat::identity;
        const Quat second{0.0f, 1.0f, 0.0f, 0.0f};

        Quat result;
        Quat::lerp(first, second, 0.5f, &result);
        checkQuat(result, 0.0f, 0.5f, 0.0f, 0.5f);

        Quat::slerp(first, second, 0.5f, &result);
        checkQuat(result, 0.0f, 0.70710677f, 0.0f, 0.70710677f);

        Quat::slerp(Quat{0.0f, 0.0f, 0.0f, 1.001f}, Quat{0.0f, 0.999f, 0.0f, 0.0f}, 0.5f, &result);
        checkQuat(result, 0.0f, 0.70710677f, 0.0f, 0.70710677f);
        CHECK(result.x * result.x + result.y * result.y + result.z * result.z + result.w * result.w ==
              doctest::Approx(1.0f).epsilon(kTolerance));

        const Quat nearFirst{0.0f, 0.0f, 0.0f, 1.001f};
        const Quat nearSecond{0.0f, 0.999f, 0.0f, 0.0f};
        Quat::squad(nearFirst, nearSecond, nearFirst, nearSecond, 0.5f, &result);
        CHECK(result.x * result.x + result.y * result.y + result.z * result.z + result.w * result.w ==
              doctest::Approx(1.0f).epsilon(kTolerance));

        Quat::slerp(first, Quat{-second.x, -second.y, -second.z, -second.w}, 0.0f, &result);
        checkQuat(result, first.x, first.y, first.z, first.w);

        result = first;
        Quat::lerp(result, second, 1.0f, &result);
        checkQuat(result, second.x, second.y, second.z, second.w);

        result = second;
        Quat::slerp(first, result, 0.0f, &result);
        checkQuat(result, first.x, first.y, first.z, first.w);
    }
}
