// Real DPAttributeData and conversion code; only host arrays/storage are doubles.
#include "common/utility/DMayaAttributeArray.h"
#include <maya/MVector.h>
#include <cassert>
#include <iostream>

static int calls = 0, failAt = -1, shortAt = -1;
template <class T> struct HostArray {
    std::vector<T> values;
    unsigned length() const {
        return values.size();
    }
    T& operator[](unsigned i) {
        return values.at(i);
    }
    const T& operator[](unsigned i) const {
        return values.at(i);
    }
    MStatus setLength(unsigned count) {
        const int index = calls++;
        if (index == failAt)
            return MS::kFailure;
        values.resize(count && index == shortAt ? count - 1 : count);
        return MS::kSuccess;
    }
    MStatus copy(const HostArray& other) {
        const int index = calls++;
        if (index == failAt)
            return MS::kFailure;
        values = other.values;
        if (index == shortAt && !values.empty())
            values.pop_back();
        return MS::kSuccess;
    }
};
using MDoubleArray = HostArray<double>;
using MVectorArray = HostArray<MVector>;
using DScalarDataList = std::vector<float>;
using DVec2DataList = std::vector<Imath::V2f>;
using DVec3DataList = std::vector<Imath::V3f>;
struct DMeshData {
    DPAttributeData::ConstPtr attribute;
    std::size_t count = 0;
    DPAttributeData::ConstPtr getAttr(const std::string&) const {
        return attribute;
    }
    std::size_t numberOfMesh() const {
        return count;
    }
    std::size_t numberOfPoints() const {
        return count;
    }
};
using DMItem = std::shared_ptr<DMeshData>;
#include "mesh_attribute.inc"

template <class T> DMItem mesh(const std::vector<T>& values) {
    auto result = std::make_shared<DMeshData>();
    result->attribute = std::make_shared<TypedDPAttributeData<T>>(values, T());
    result->count = values.size();
    return result;
}
template <class Action> bool rejected(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}
template <class Action> void arrayFailures(Action action) {
    calls = 0;
    failAt = shortAt = -1;
    action();
    const int count = calls;
    assert(count > 0);
    for (int i = 0; i < count; ++i) {
        calls = 0;
        failAt = i;
        assert(rejected(action));
        assert(calls == i + 1);
        failAt = -1;
        calls = 0;
        shortAt = i;
        assert(rejected(action));
        assert(calls == i + 1);
        shortAt = -1;
    }
}
template <class T> void scalars() {
    auto source = mesh<T>({T(0), T(1)});
    MDoubleArray maya;
    DScalarDataList native;
    for (bool point : {false, true}) {
        auto readMaya = [&] {
            if (point)
                getDMPSingleValueToFloat<T>(source, "a", maya);
            else
                getDMSingleValueToFloat<T>(*source, "a", maya);
        };
        auto readNative = [&] {
            if (point)
                getDMPSingleValueToFloat<T>(source, "a", native);
            else
                getDMSingleValueToFloat<T>(*source, "a", native);
        };
        arrayFailures(readMaya);
        readMaya();
        readNative();
        assert(maya.length() == 2 && maya[1] == 1 && native.size() == 2 && native[1] == 1);
        source->count = 1;
        assert(rejected(readMaya) && rejected(readNative));
        source->count = 2;
        auto saved = source->attribute;
        source->attribute.reset();
        assert(rejected(readMaya) && rejected(readNative));
        source->attribute = saved;
        source->count = dommy::generation::kMaxGeneratedPointCount + 1;
        assert(rejected(readMaya) && rejected(readNative));
        source->count = 2;
    }
}
template <class T> void vec2() {
    auto source = mesh<T>({T(2, 3)});
    MVectorArray maya;
    DVec2DataList native;
    MDoubleArray u, v;
    for (bool point : {false, true}) {
        auto vector = [&] {
            if (point)
                getDMPVec2ToVectorArray<T>(source, "a", maya);
            else
                getDMVec2ToVectorArray<T>(*source, "a", maya);
        };
        auto uv = [&] {
            if (point)
                getDMPTwoArray<T>(source, "a", u, v);
            else
                getDMTwoArray<T>(*source, "a", u, v);
        };
        auto readNative = [&] {
            if (point)
                getDMPVec2ToVectorArray<T>(source, "a", native);
            else
                getDMVec2ToVectorArray<T>(*source, "a", native);
        };
        arrayFailures(vector);
        arrayFailures(uv);
        vector();
        uv();
        readNative();
        assert(maya[0].x == 2 && maya[0].y == 3 && maya[0].z == 0 && u[0] == 2 && v[0] == 3);
        assert(native[0][0] == 2 && native[0][1] == 3);
        source->count = 0;
        assert(rejected(vector) && rejected(uv) && rejected(readNative));
        source->count = 1;
    }
}
template <class T> void vec3() {
    auto source = mesh<T>({T(2, 3, 4)});
    MVectorArray maya;
    DVec3DataList native;
    MDoubleArray u, v;
    for (bool point : {false, true}) {
        auto vector = [&] {
            if (point)
                getDMPVec3ToVectorArray<T>(source, "a", maya);
            else
                getDMVec3ToVectorArray<T>(*source, "a", maya);
        };
        auto uv = [&] {
            if (point)
                getDMPTwoArray<T>(source, "a", u, v);
            else
                getDMTwoArray<T>(*source, "a", u, v);
        };
        auto readNative = [&] {
            if (point)
                getDMPVec3ToVectorArray<T>(source, "a", native);
            else
                getDMVec3ToVectorArray<T>(*source, "a", native);
        };
        arrayFailures(vector);
        arrayFailures(uv);
        vector();
        uv();
        readNative();
        assert(maya[0].x == 2 && maya[0].y == 3 && maya[0].z == 4 && u[0] == 2 && v[0] == 3);
        assert(native[0][2] == 4);
        source->count = 0;
        assert(rejected(vector) && rejected(uv) && rejected(readNative));
        source->count = 1;
    }
}
int main() {
    scalars<bool>();
    scalars<int32_t>();
    scalars<uint32_t>();
    scalars<int64_t>();
    scalars<float>();
    scalars<double>();
    vec2<Imath::V2i>();
    vec2<Imath::V2f>();
    vec2<Imath::V2d>();
    vec3<Imath::V3i>();
    vec3<Imath::V3f>();
    vec3<Imath::V3d>();
    MDoubleArray u, v;
    MVectorArray vectors;
    DVec3DataList native3;
    DVec2DataList native2;
    DScalarDataList scalar;
    for (double bad : {std::numeric_limits<double>::max(), std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()}) {
        auto s = mesh<double>({bad});
        auto v2 = mesh<Imath::V2d>({{2, bad}});
        auto v3 = mesh<Imath::V3d>({{2, 3, bad}});
        assert(rejected([&] { getDMSingleValueToFloat<double>(*s, "a", scalar); }));
        assert(rejected([&] { getDMPSingleValueToFloat<double>(s, "a", scalar); }));
        assert(rejected([&] { getDMVec2ToVectorArray<Imath::V2d>(*v2, "a", native2); }));
        assert(rejected([&] { getDMPVec2ToVectorArray<Imath::V2d>(v2, "a", native2); }));
        assert(rejected([&] { getDMPVec3ToVectorArray<Imath::V3d>(v3, "a", native3); }));
        if (!std::isfinite(bad)) {
            assert(rejected([&] { getDMPVec3ToVectorArray<Imath::V3d>(v3, "a", vectors); }));
            assert(rejected([&] { getDMPVec2ToVectorArray<Imath::V2d>(v2, "a", vectors); }));
            assert(rejected([&] { getDMPTwoArray<Imath::V3d>(v3, "a", u, v); }));
            assert(rejected([&] { getDMTwoArray<Imath::V3d>(*v3, "a", u, v); }));
            assert(rejected([&] { getDMPSingleValueToFloat<double>(s, "a", u); }));
        }
    }
    DMItem null;
    assert(rejected([&] { getDMPSingleValueToFloat<double>(null, "a", u); }));
    assert(rejected([&] { getDMPSingleValueToFloat<double>(null, "a", scalar); }));
    assert(rejected([&] { getDMPTwoArray<Imath::V2d>(null, "a", u, v); }));
    assert(rejected([&] { getDMPVec2ToVectorArray<Imath::V2d>(null, "a", vectors); }));
    assert(rejected([&] { getDMPVec2ToVectorArray<Imath::V2d>(null, "a", native2); }));
    assert(rejected([&] { getDMPVec3ToVectorArray<Imath::V3d>(null, "a", vectors); }));
    assert(rejected([&] { getDMPVec3ToVectorArray<Imath::V3d>(null, "a", native3); }));
    auto empty = mesh<double>({});
    getDMPSingleValueToFloat<double>(empty, "a", u);
    assert(u.length() == 0);
    auto wrong = mesh<float>({1});
    assert(rejected([&] { getDMPSingleValueToFloat<double>(wrong, "a", u); }));
    std::cout << "Mesh attributes: 14 production templates, typed data and host allocation/copy "
                 "failures passed\n";
}
