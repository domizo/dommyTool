// Stand-in arrays expose status, short-copy, exception, and construction faults.
// The methods under test are extracted verbatim from the production .cpp files.
#include "common/utility/DMeshUVTransfer.h"
#include "common/utility/DCurveBendPlan.h"
#include "DPoint/math/DVectorMath.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

struct Faults {
    int copies = 0, copyAt = 0, kind = 0;
    int constructions = 0, constructAt = 0, live = 0;
} faults;

template <typename T> struct Array {
    std::vector<T> values;
    Array() {
        if (++faults.constructions == faults.constructAt) throw std::bad_alloc();
        ++faults.live;
    }
    ~Array() { --faults.live; }
    Array(const Array&) = delete;
    Array& operator=(const Array&) = delete;
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
    T& operator[](unsigned int index) { return values.at(index); }
    T operator[](unsigned int index) const { return values.at(index); }
    MStatus copy(const Array& other) {
        const bool fail = ++faults.copies == faults.copyAt;
        if (fail && faults.kind == 1) return MS::kFailure;
        if (fail && faults.kind == 3) throw std::bad_alloc();
        values = other.values;
        if (fail && faults.kind == 2 && !values.empty()) values.pop_back();
        return MS::kSuccess;
    }
};
using MDoubleArray = Array<double>;
using MFloatArray = Array<float>;
using MIntArray = Array<int>;
using DPointList = std::vector<int>;
using Metadata = std::vector<int>;
void CopyAttributeToAttribute(Metadata& target, const Metadata& source) { target = source; }
void CopyGroupToGroup(Metadata& target, const Metadata& source) { target = source; }

struct DPrimBaseData {
    DPointList pointArray;
    Metadata mAttributeList, mGrooupList;
    int m_mat = 0;
    int getTransformMatrix() const { return m_mat; }
    void getBBox() const {}
    std::size_t numberOfPoints() const { return pointArray.size(); }
};
struct DCurveInternalData : DPrimBaseData {
    struct ArrayStorage { MDoubleArray knots; };
    std::unique_ptr<ArrayStorage> arrays_{new ArrayStorage()};
    int degree = 1;
    bool mIsOpen = true;
    DCurveInternalData() = default;
    DCurveInternalData(const DCurveInternalData&);
    DCurveInternalData& operator=(const DCurveInternalData&);
    void copy(const DCurveInternalData&);
    void swapState(DCurveInternalData&) noexcept;
    bool replaceKnots(const MDoubleArray&);
    bool replacePositions(const DPointList&);
    bool hasValidCurveTopology() const {
        if (degree < 1 || pointArray.size() <= static_cast<std::size_t>(degree) ||
            arrays_->knots.length() != pointArray.size() + degree - 1U) return false;
        for (unsigned int i = 0; i < arrays_->knots.length(); ++i)
            if (!std::isfinite(arrays_->knots[i]) || (i && arrays_->knots[i] < arrays_->knots[i-1U]))
                return false;
        return true;
    }
};
struct DMeshInternalData : DPrimBaseData {
    struct ArrayStorage {
        MIntArray mFaceList, mConnectList;
        MFloatArray m_u_vals, m_v_vals;
        MIntArray m_uv_count_list, m_uv_connect_list;
    };
    std::unique_ptr<ArrayStorage> arrays_{new ArrayStorage()};
    Metadata mFaceAttributeList, mPVtxAttributeList, mFaceGroupList, mEdgeGroupList;
    DMeshInternalData() = default;
    DMeshInternalData(const DMeshInternalData&);
    DMeshInternalData& operator=(const DMeshInternalData&);
    void copy(const DMeshInternalData&);
    void swapState(DMeshInternalData&) noexcept;
    bool setUvData(const MFloatArray&, const MFloatArray&, const MIntArray&, const MIntArray&);
    void uvInitialize();
    void clearUVs();
    bool hasValidMeshData() const {
        std::size_t corners = 0;
        for (int count : arrays_->mFaceList.values) {
            if (count < 3) return false;
            corners += static_cast<std::size_t>(count);
        }
        if (!corners || corners != arrays_->mConnectList.length()) return false;
        for (int index : arrays_->mConnectList.values)
            if (index < 0 || static_cast<std::size_t>(index) >= pointArray.size()) return false;
        return dommy::mesh::validFullMeshUVArrays(arrays_->mFaceList, arrays_->m_u_vals,
            arrays_->m_v_vals, arrays_->m_uv_count_list, arrays_->m_uv_connect_list);
    }
};

#include "geometry_state_methods.inc"

using Snapshot = std::vector<std::vector<double>>;
template <typename T> void append(Snapshot& snapshot, const std::vector<T>& values) {
    snapshot.emplace_back(values.begin(), values.end());
}
Snapshot snapshot(const DPrimBaseData& object) {
    Snapshot result;
    append(result, object.pointArray); append(result, object.mAttributeList);
    append(result, object.mGrooupList); result.push_back({double(object.m_mat)});
    return result;
}
Snapshot snapshot(const DCurveInternalData& object) {
    Snapshot result = snapshot(static_cast<const DPrimBaseData&>(object));
    append(result, object.arrays_->knots.values);
    result.push_back({double(object.degree), double(object.mIsOpen)});
    return result;
}
Snapshot snapshot(const DMeshInternalData& object) {
    Snapshot result = snapshot(static_cast<const DPrimBaseData&>(object));
    append(result, object.mFaceAttributeList); append(result, object.mPVtxAttributeList);
    append(result, object.mFaceGroupList); append(result, object.mEdgeGroupList);
    append(result, object.arrays_->mFaceList.values); append(result, object.arrays_->mConnectList.values);
    append(result, object.arrays_->m_u_vals.values); append(result, object.arrays_->m_v_vals.values);
    append(result, object.arrays_->m_uv_count_list.values); append(result, object.arrays_->m_uv_connect_list.values);
    return result;
}
void resetFaults() {
    const int live = faults.live;
    faults = Faults(); faults.live = live;
}
template <typename Object, typename Action>
void verifyFailures(Object& target, Action action, int copyCount, int constructionCount) {
    const Snapshot before = snapshot(target);
    const int live = faults.live;
    for (int kind = 1; kind <= 3; ++kind) {
        for (int index = 1; index <= copyCount; ++index) {
            resetFaults(); faults.copyAt = index; faults.kind = kind;
            bool failed = false;
            try { failed = !action(); } catch (const std::exception&) { failed = true; }
            assert(failed && snapshot(target) == before && faults.live == live);
        }
    }
    for (int index = 1; index <= constructionCount; ++index) {
        resetFaults(); faults.constructAt = index;
        bool failed = false;
        try { failed = !action(); } catch (const std::exception&) { failed = true; }
        assert(failed && snapshot(target) == before && faults.live == live);
    }
    resetFaults();
}
void testStateTransactions() {
    DCurveInternalData original, replacement;
    original.pointArray = {1,2}; original.arrays_->knots.values = {0,1};
    original.mAttributeList = {10,20}; original.mGrooupList = {1}; original.m_mat = 7;
    replacement.pointArray = {3,4,5}; replacement.arrays_->knots.values = {0,1,2};
    replacement.mAttributeList = {30,40,50}; replacement.mGrooupList = {2}; replacement.m_mat = 9;
    verifyFailures(original, [&] { original.copy(replacement); return true; }, 1, 1);
    original = replacement;
    assert(snapshot(original) == snapshot(replacement));
    assert(faults.copies == 1); // Commit must not copy a Maya array again.
    original.pointArray[0] = 99;
    assert(replacement.pointArray[0] == 3);
    verifyFailures(original, [&] { return original.replaceKnots(replacement.arrays_->knots); }, 2, 1);
    verifyFailures(original, [&] { return original.replacePositions(DPointList{6,7,8}); }, 1, 1);
    assert(original.replacePositions(DPointList{6,7,8}));
    assert(original.pointArray == DPointList({6,7,8}) && original.m_mat == 9);
    const Snapshot curveBefore = snapshot(original);
    original.copy(original);
    assert(original.replaceKnots(original.arrays_->knots));
    assert(snapshot(original) == curveBefore);
    MDoubleArray invalidKnots;
    for (const auto& values : std::vector<std::vector<double>>{
            {}, {0,1}, {0,2,1}, {0,1,std::numeric_limits<double>::quiet_NaN()}}) {
        invalidKnots.values = values;
        assert(!original.replaceKnots(invalidKnots) && snapshot(original) == curveBefore);
    }

    DMeshInternalData mesh, other;
    for (DMeshInternalData* value : {&mesh, &other}) {
        value->pointArray = {1,2,3}; value->arrays_->mFaceList.values = {3};
        value->arrays_->mConnectList.values = {0,1,2};
        value->arrays_->m_u_vals.values = {0,1,0}; value->arrays_->m_v_vals.values = {0,0,1};
        value->arrays_->m_uv_count_list.values = {3}; value->arrays_->m_uv_connect_list.values = {0,1,2};
        value->mFaceAttributeList = {11}; value->mPVtxAttributeList = {12,13,14};
        value->mFaceGroupList = {0}; value->mEdgeGroupList = {1};
    }
    other.pointArray = {4,5,6,7}; other.m_mat = 42;
    other.mAttributeList = {1,2,3,4}; other.mGrooupList = {3};
    verifyFailures(mesh, [&] { mesh.copy(other); return true; }, 6, 6);
    mesh = other;
    assert(faults.copies == 6 && snapshot(mesh) == snapshot(other));
    const auto& uv = *other.arrays_;
    verifyFailures(mesh, [&] { return mesh.setUvData(uv.m_u_vals, uv.m_v_vals,
        uv.m_uv_count_list, uv.m_uv_connect_list); }, 6, 6);
    const Snapshot meshBefore = snapshot(mesh);
    auto& own = *mesh.arrays_;
    assert(mesh.setUvData(own.m_u_vals, own.m_v_vals, own.m_uv_count_list, own.m_uv_connect_list));
    assert(snapshot(mesh) == meshBefore);
    mesh.copy(mesh);
    assert(snapshot(mesh) == meshBefore);
    verifyFailures(mesh, [&] { mesh.clearUVs(); return true; }, 2, 8);
    for (int kind : {1,3}) {
        for (int index = 3; index <= 6; ++index) {
            resetFaults(); faults.kind = kind; faults.copyAt = index;
            bool failed = false;
            try { mesh.clearUVs(); } catch (const std::exception&) { failed = true; }
            assert(failed && snapshot(mesh) == meshBefore);
        }
    }
    resetFaults();
    mesh.clearUVs();
    assert(mesh.arrays_->m_u_vals.length() == 0 && mesh.hasValidMeshData());
}
void testBendPlan() {
    using dommy::curve::appendBendWork;
    std::size_t work = 0;
    assert(appendBendWork(2, false, work) && work == 1);
    assert(appendBendWork(3, true, work) && work == 10);
    for (std::size_t count : {0U, 1U, 100000U, 10000001U}) {
        assert(!appendBendWork(count, false, work) && work == 10);
    }
    assert(!appendBendWork(std::numeric_limits<std::size_t>::max(), false, work));
    work = 0;
    assert(appendBendWork(14142, false, work));
    work = 0;
    assert(!appendBendWork(14143, false, work) && work == 0);
    assert(!appendBendWork(10000, true, work) && work == 0);
    assert(appendBendWork(10000, false, work));
    assert(appendBendWork(10000, false, work));
    const std::size_t previous = work;
    assert(!appendBendWork(200, false, work) && work == previous);
    double x, y, z;
    assert(dommy::math::normalized3OrZero(std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(), 0.0, x, y, z, 1e-12));
    assert(std::isfinite(x) && std::fabs(x*x+y*y-1.0) < 1e-12);
    assert(!dommy::math::normalized3OrZero(0.0,0.0,0.0,x,y,z,1e-12));
}
int main() {
    testStateTransactions();
    assert(faults.live == 0);
    testBendPlan();
    std::cout << "Production geometry state transactions and bend budgets: OK (host fixtures)\n";
}
