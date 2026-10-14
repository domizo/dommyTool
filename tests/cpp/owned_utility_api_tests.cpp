#include "DUtility.h"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <sstream>
#include <type_traits>

namespace {
using DTools::Vec2;
using DTools::Vec3;
using DTools::Vec4;

template<class Vector, int Width> void storageContract()
{
    static_assert(std::is_standard_layout<Vector>::value, "Public scalar-array layout");
    static_assert(sizeof(Vector) == Width * sizeof(DReal), "No hidden storage");
    static_assert(alignof(Vector) == alignof(DReal), "Public alignment");
    static_assert(offsetof(Vector, data) == 0, "Data begins at object address");
    static_assert(std::is_same<decltype(std::declval<Vector&>()[0]), DReal&>::value, "Writable lanes");
    static_assert(std::is_same<decltype(std::declval<const Vector&>()[0]), DReal>::value, "Const lanes are values");
    static_assert(!std::is_convertible<Vector, DReal*>::value, "Pointer conversions stay explicit");
    DReal lanes[4] = {-0.0f, 2.0f, -3.0f, 4.0f};
    Vector fromBuffer(lanes), copied(fromBuffer), assigned;
    assigned = copied;
    assert(std::memcmp(assigned.data, lanes, Width * sizeof(DReal)) == 0);
    assert(static_cast<DReal*>(assigned) == assigned.data);
    assert(static_cast<const DReal*>(copied) == copied.data);
    std::ostringstream output;
    output << copied;
    assert(output.str().front() == '(' && output.str().back() == ')');
    std::istringstream input("1 2 3 4");
    input >> assigned;
    for (int axis = 0; axis != Width; ++axis) assert(assigned[axis] == axis + 1);
}

void signatures()
{
    Vec3& (Vec3::*scale3)(const DReal&) = &Vec3::operator*=;
    Vec4& (Vec4::*scale4)(DReal) = &Vec4::operator*=;
    Vec3 (*multiplyFloat)(const Vec3&, const DReal&) = DTools::operator*;
    Vec3 (*multiplyInt)(const Vec3&, const int&) = DTools::operator*;
    Vec4 (*divideInt)(const Vec4&, int) = DTools::operator/;
    Vec2 (*multiply2)(DReal, const Vec2&) = DTools::operator*;
    Vec3 value(2.0f, 3.0f, 4.0f);
    assert(multiplyFloat(value, 2.0f)[0] == 4.0f && multiplyInt(value, 2)[1] == 6.0f);
    assert(divideInt(Vec4(8.0f), 2)[3] == 4.0f && multiply2(2.0f, Vec2(3.0f))[1] == 6.0f);
    (value.*scale3)(value[0]);
    assert(value == Vec3(4.0f, 12.0f, 16.0f));
    Vec4 four(2.0f, 3.0f, 4.0f, 5.0f);
    (four.*scale4)(four[0]);
    assert(four == Vec4(4.0f, 6.0f, 8.0f, 10.0f));
    Vec2 two(2.0f, 3.0f);
    two *= two[0];
    assert(two == Vec2(4.0f, 12.0f));
    value.SetValue(1.0f, 2.0f, 3.0f);
    value.SetValue(value[2], value[0], value[1]);
    assert(value == Vec3(3.0f)); // Serialized writes preserve reference aliasing.
    DReal data[3] = {};
    value.GetValue(data);
    assert(data[0] == 3.0f && data[2] == 3.0f);
    Vec4 converted(Vec3(1.0f, 2.0f, 3.0f));
    assert(converted[3] == 0.0f);
    converted = Vec3(4.0f, 5.0f, 6.0f);
    assert(converted[3] == 0.0f);
}

void arithmeticPolicies()
{
    volatile DReal supplied = 3.0f;
    const DReal divisor = supplied;
    const DReal reciprocal = 1.0 / divisor;
    Vec2 two(7.0f); two /= divisor;
    Vec3 three(7.0f); three /= divisor;
    Vec4 four(7.0f); four /= divisor;
    assert(two[0] == 7.0f * reciprocal && three[0] == 7.0f * reciprocal);
    assert(four[0] == 7.0f / divisor);
    assert((Vec4(7.0f) / DReal(divisor))[0] == 7.0f * reciprocal);
    assert(two[0] != four[0]);
    const Vec3 negativeZero(-0.0f), positive(1.0f);
    assert(std::signbit(DTools::dot(negativeZero, positive)));
    assert(std::signbit(DTools::dot(Vec4(-0.0f), Vec4(1.0f))));
    assert(Vec2(3.0f, 4.0f) < Vec2(0.0f, 6.0f));
    assert(Vec3(0.0f, 100.0f, 0.0f) < Vec3(1.0f, 0.0f, 0.0f));
    const DReal nan = std::numeric_limits<DReal>::quiet_NaN();
    assert(Vec3(1.0f) < Vec3(nan) && !(Vec3(nan) < Vec3(1.0f)));
    assert(Vec3(nan) != Vec3(nan));
    assert(DTools::proj(Vec4(2.0f, 4.0f, 6.0f, 2.0f)) == Vec3(1.0f, 2.0f, 3.0f));
    assert(DTools::proj(Vec4(2.0f, 4.0f, 6.0f, 0.0f)) == Vec3(2.0f, 4.0f, 6.0f));
    const DReal smallW = VEC_RX_FEQ_EPS / 2.0;
    assert(DTools::proj(Vec4(1.0f, 1.0f, 1.0f, smallW)) == Vec3(1.0f));
}

void facadePolicies()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    assert(std::isnan(D_MAX(1.0, nan)) && D_MAX(nan, 1.0) == 1.0);
    assert(std::signbit(D_MAX(0.0, -0.0)) && std::signbit(D_MIN(0.0, -0.0)));
    assert(std::isnan(D_CLAMP(nan, 0.0, 1.0)));
    assert(D_CLAMP(0.5, 1.0, 0.0) == 1.0);
    assert(D_SIGN(nan) == 0.0 && !std::signbit(D_SIGN(-0.0)));
    int flag = 0; D_TOGGLE(flag); assert(flag == 1); D_TOGGLE(flag); assert(flag == 0);
    D_TOGGLE(flag, 23); assert(flag == 1); D_TOGGLE(flag, 0); assert(flag == 0);
    int left = 1, right = 2; D_SWAP(left, right); assert(left == 2 && right == 1);
    assert(DFunc::GenSpace(-1).empty() && DFunc::GenSpace(3) == "   ");
    assert(DFunc::Min3(Vec3(1.0f, -2.0f, 3.0f)) == -2.0);
    assert(DFunc::Max3(Vec3(1.0f, -2.0f, 3.0f)) == 3.0);
    assert(DFunc::Sqrt(nan) == 0.0 && DFunc::Sqrt(-1.0) == 0.0);
    int count = 0;
    DFOR3(0, 2, 0, 3, 0, 4) { count += (i >= 0 && j >= 0 && k >= 0); }
    assert(count == 24);
    count = 0;
    DFOR2E(1, 2, 1, 3) { count += (i >= 1 && j >= 1); }
    assert(count == 6);
}
} // namespace

int main()
{
    storageContract<Vec2, 2>(); storageContract<Vec3, 3>(); storageContract<Vec4, 4>();
    signatures(); arithmeticPolicies(); facadePolicies();
    std::cout << "Owned utility API: storage, exact overloads, aliasing, division, signed zero, NaNs and facade contracts passed\n";
}
