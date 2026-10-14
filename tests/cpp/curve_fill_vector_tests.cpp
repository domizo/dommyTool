#include "common/utility/DCurveSampling.h"
#include "common/utility/DPointDeformMath.h"
#include "common/utility/DPointTransform.h"
#include "common/utility/DCurveBatchBudget.h"
#include "common/utility/DMayaCheckedArray.h"
#include <maya/MMatrix.h>
#include <ImathVec.h>

#include <array>
#include <atomic>
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Deterministic split ranges test worker status collection, not TBB scheduling.
namespace tbb {
template<typename T> struct blocked_range {
    T first, last;
    blocked_range(T a, T b, T = 1) : first(a), last(b) {}
    T begin() const { return first; }
    T end() const { return last; }
};
template<typename T, typename Body> void parallel_for(const blocked_range<T>& range, Body body) {
    const T middle = range.begin() + (range.end() - range.begin()) / 2;
    body(blocked_range<T>(range.begin(), middle)); body(blocked_range<T>(middle, range.end()));
}
}
using DPoint = Imath::V3f;
using DPointList = std::vector<DPoint>;
using float3 = std::array<float, 3>;

struct Failures {
    std::map<std::string, int> calls, at;
    bool fail(const std::string& name) { return ++calls[name] == at[name]; }
    void reset() { calls.clear(); at.clear(); }
} failures;
struct MPoint {
    std::array<double, 3> values;
    MPoint(double x = 0, double y = 0, double z = 0) : values{{x,y,z}} {}
    double& operator[](std::size_t i) { return values.at(i); }
    double operator[](std::size_t i) const { return values.at(i); }
};
using MVector = MPoint;
namespace MSpace { enum Space { kObject }; }
namespace dommy { namespace maya {
bool isFiniteVector(const MVector& v) {
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}
bool isFloatRepresentablePoint(const MPoint& v) {
    return numeric::canRepresentAsFloat(v[0]) && numeric::canRepresentAsFloat(v[1]) &&
        numeric::canRepresentAsFloat(v[2]);
}
} }
struct MPointArray {
    std::vector<MPoint> points;
    unsigned int length() const { return static_cast<unsigned int>(points.size()); }
    MStatus setLength(unsigned int count) {
        if (failures.fail("resize")) return MS::kFailure;
        points.resize(count); return MS::kSuccess;
    }
    MStatus copy(const MPointArray& value) {
        if (failures.fail("copy")) return MS::kFailure;
        points = value.points; return MS::kSuccess;
    }
    MPoint& operator[](unsigned int i) { return points.at(i); }
    const MPoint& operator[](unsigned int i) const { return points.at(i); }
};
struct MDoubleArray {};
void GenerateDCurveKnots(std::size_t, int, MDoubleArray&) {}
struct DCurveInternalData {
    MPointArray points;
    MMatrix matrix;
    DCurveInternalData() { points.points = {MPoint(0,0,0), MPoint(1,0,0)}; }
    DCurveInternalData(const MPointArray& value, int) : points(value) {
        if (failures.fail("construct")) throw std::bad_alloc();
    }
    DCurveInternalData(const MPointArray& value, const MDoubleArray&, int degree)
        : DCurveInternalData(value, degree) {}
    bool hasValidCurveTopology() const { return points.length() >= 2; }
    std::size_t numberOfPoints() const { return points.length(); }
    bool appendOperationCost(dommy::curve::CurveBatchBudget& budget) const {
        return hasValidCurveTopology() && budget.append(points.length(),points.length(),0U,0U);
    }
    int getDegree() const { return 1; }
    MMatrix getTransformMatrix() const { return matrix; }
};
using DCItem = std::shared_ptr<DCurveInternalData>;
struct DCurveData {
    using DCurveList = std::vector<DCItem>;
    DCurveList curves;
    std::size_t numberOfCurves() const { return curves.size(); }
    DCurveList::const_iterator beginCurves() const { return curves.begin(); }
    DCurveList::const_iterator endCurves() const { return curves.end(); }
    bool insert(const DCurveList& value) {
        if (failures.fail("insert")) return false;
        curves = value; return true;
    }
};
struct Mesh { MVector normal{0,1,0}; };
struct MObject { DCItem curve; std::shared_ptr<Mesh> mesh; };
MStatus generateMCurveFromDcurve(const DCItem& curve, MObject& object) {
    if (failures.fail("curve")) return MS::kFailure;
    object.curve = curve; return MS::kSuccess;
}
const double kMFnNurbsEpsilon = 1e-6;
struct MFnNurbsCurve {
    DCItem curve;
    MFnNurbsCurve(const MObject& object, MStatus* status) : curve(object.curve) {
        *status = failures.fail("bindCurve") ? MS::kFailure : MS::kSuccess;
    }
    double length(double, MStatus* status) {
        *status = failures.fail("length") ? MS::kFailure : MS::kSuccess; return 1.0;
    }
    double findParamFromLength(double value, MStatus* status) {
        *status = failures.fail("parameter") ? MS::kFailure : MS::kSuccess; return value;
    }
    MStatus getPointAtParam(double value, MPoint& point, MSpace::Space) {
        if (failures.fail("point")) return MS::kFailure;
        for (unsigned int axis = 0; axis < 3; ++axis)
            point[axis] = curve->points[0][axis] + value * (curve->points[1][axis] - curve->points[0][axis]);
        return MS::kSuccess;
    }
};
enum class DMeshPointSpace { Local };
MStatus generateMMeshFromDmesh(const Mesh& mesh, MObject& object, DMeshPointSpace) {
    if (failures.fail("mesh")) return MS::kFailure;
    object.mesh = std::make_shared<Mesh>(mesh); return MS::kSuccess;
}
struct MFnMesh {
    std::shared_ptr<Mesh> mesh;
    MFnMesh(const MObject& object, MStatus* status) : mesh(object.mesh) {
        *status = failures.fail("bindMesh") ? MS::kFailure : MS::kSuccess;
    }
    MStatus getVertexNormal(int, MVector& normal, MSpace::Space) {
        if (failures.fail("normal")) return MS::kFailure;
        normal = mesh->normal; return MS::kSuccess;
    }
};
struct MString { std::string text{"weight"}; const char* asChar() const { return text.c_str(); } };
struct Value { double number = 1; int integer = 0; bool flag = false; float3 vector{{0,1,0}}; MString string; };
struct MDataHandle {
    Value value;
    bool valid;
    int* reads;
    void check() const { assert(valid); ++*reads; }
    double asDouble() const { check(); return value.number; }
    float asFloat() const { check(); return static_cast<float>(value.number); }
    short asShort() const { check(); return static_cast<short>(value.integer); }
    int asInt() const { check(); return value.integer; }
    bool asBool() const { check(); return value.flag; }
    MVector asVector() const { check(); return MVector(value.vector[0], value.vector[1], value.vector[2]); }
    const float3& asFloat3() const { check(); return value.vector; }
    MString asString() const { check(); return value.string; }
};
struct MDataBlock {
    int calls = 0, failAt = 0, reads = 0;
    std::map<int, Value> values;
    MDataHandle inputValue(int attribute, MStatus* status) {
        const bool valid = ++calls != failAt;
        *status = valid ? MS::kSuccess : MS::kFailure;
        return MDataHandle{values[attribute], valid, &reads};
    }
};
enum { aVectorType, aBaseVector, aVectorAttributeName, aMagnitude,
       aWeightAttributeName, aWeight, aUseClamp, aMaxStrength, aCurveCount, aSampling };
#define CHECK_MSTATUS_AND_RETURN_IT(status) do { if ((status) != MS::kSuccess) return status; } while (false)
#include "fill_vector_methods.inc"

void testInputs()
{
    using Reader = MStatus (*)(MDataBlock&);
    const Reader readers[] = {read_fill, read_mesh, read_curve, read_point};
    const int counts[] = {2,6,6,8};
    for (int node = 0; node < 4; ++node) {
        for (int failure = 1; failure <= counts[node]; ++failure) {
            MDataBlock data; data.failAt = failure; data.values[aSampling].integer = 3;
            assert(readers[node](data) == MS::kFailure && data.reads == failure - 1);
        }
        MDataBlock data; data.values[aSampling].integer = 3;
        assert(readers[node](data) == MS::kSuccess);
    }
    MDataBlock data; data.values[aSampling].integer = 1;
    assert(read_fill(data) == MS::kInvalidParameter);
    data.values[aSampling].integer = 1000001;
    assert(read_fill(data) == MS::kInvalidParameter);
    data.values[aVectorType].integer = 3;
    assert(read_mesh(data) == MS::kInvalidParameter && read_point(data) == MS::kInvalidParameter);
    data.values[aVectorType].integer = 0; data.values[aMagnitude].number = std::numeric_limits<float>::infinity();
    assert(read_curve(data) == MS::kInvalidParameter);
    data.values[aMagnitude].number = 1; data.values[aMaxStrength].number = -1; data.values[aUseClamp].flag = true;
    assert(read_point(data) == MS::kInvalidParameter);
    DCurveData twoCurves; twoCurves.curves.resize(2);
    MDataBlock plan; plan.values[aCurveCount].integer = 1000000; plan.values[aSampling].integer = 10;
    assert(plan_fill(plan,&twoCurves) == MS::kSuccess);
    plan.values[aSampling].integer = 11;
    assert(plan_fill(plan,&twoCurves) == MS::kInvalidParameter);
    plan.values[aCurveCount].integer = -1;
    assert(plan_fill(plan,&twoCurves) == MS::kInvalidParameter);
}

void testGeneration()
{
    DCurveData input; input.curves = {std::make_shared<DCurveInternalData>(), std::make_shared<DCurveInternalData>()};
    input.curves[0]->matrix[3][1] = 100; input.curves[1]->matrix[3][1] = 200;
    dommy::curve::StitchSamplingPlan plan;
    assert(dommy::curve::checkedStitchSamplingPlan(2,3,plan));
    for (int mode = 0; mode < 2; ++mode) {
        const auto generate = [&](DCurveData& output) {
            return mode == 0 ? generate_fill(&input,&output,2,3,2) : generate_stitch(&input,&output,plan,1,0,0);
        };
        failures.reset(); DCurveData output;
        assert(generate(output) == MS::kSuccess);
        assert(output.curves.size() == (mode == 0 ? 2U : 3U));
        if (mode == 0) assert(std::fabs(output.curves[0]->points[0][1] - 400.0/3.0) < 1e-12);
        else assert(output.curves[0]->points[0][1] == 100 && output.curves[0]->points[1][1] == 200);
        assert(output.curves[0]->matrix[3][1] == 0 && input.curves[0]->points[0][1] == 0);
        const auto events = failures.calls;
        const DCItem sentinel = std::make_shared<DCurveInternalData>();
        for (const auto& event : events) {
            for (int ordinal = 1; ordinal <= event.second; ++ordinal) {
                failures.reset(); failures.at[event.first] = ordinal; output.curves = {sentinel};
                assert(generate(output) != MS::kSuccess);
                assert(output.curves.size() == 1 && output.curves[0] == sentinel);
            }
        }
        failures.reset(); input.curves[1]->matrix[0][3] = 1;
        assert(generate(output) == MS::kInvalidParameter && output.curves[0] == sentinel);
        input.curves[1]->matrix[0][3] = 0;
        input.curves[1]->matrix[3][1] = 1e100;
        assert(generate(output) == MS::kInvalidParameter && output.curves[0] == sentinel);
        input.curves[1]->matrix[3][1] = 200;
    }
    failures.reset(); DCurveData empty, output;
    assert(generate_fill(&empty,&output,0,3,0) == MS::kSuccess && failures.calls.empty());
    input.curves[0]->matrix[0][0] = 0; input.curves[0]->matrix[0][1] = 2;
    assert(generate_stitch(&input,&output,plan,1,0,0) == MS::kSuccess);
    assert(output.curves[2]->points[0][0] == 0 && output.curves[2]->points[0][1] == 102);
}

void testNormals()
{
    auto mesh = std::make_shared<Mesh>();
    std::vector<Imath::V3d> vectors(3);
    failures.reset(); assert(query_normals(mesh,3,vectors) == MS::kSuccess && vectors[2][1] == 1);
    for (const std::string& event : {"mesh", "bindMesh", "normal"}) {
        const int count = event == "normal" ? 3 : 1;
        for (int ordinal = 1; ordinal <= count; ++ordinal) {
            failures.reset(); failures.at[event] = ordinal;
            assert(query_normals(mesh,3,vectors) == MS::kFailure);
            if (event != "normal") assert(failures.calls["normal"] == 0);
        }
    }
    failures.reset(); mesh->normal[1] = std::numeric_limits<double>::quiet_NaN();
    assert(query_normals(mesh,3,vectors) == MS::kInvalidParameter);
}

void testVectorMath()
{
    using dommy::point::offsetAlongVector;
    using dommy::point::clampVectorMagnitude;
    const DPointList source(3,DPoint(1,2,3));
    std::vector<Imath::V3d> vectors(3,Imath::V3d(3e38,0,0));
    std::vector<double> weights(3,1);
    DPointList output;
    assert(deform_curve(source,vectors,weights,2,0,false,0,output) == MS::kSuccess);
    assert(output[0][0] == 1 && output[2][2] == 3);
    vectors.assign(3,Imath::V3d(1e20,0,0));
    assert(deform_point(source,vectors,weights,1,1,true,1,output) == MS::kSuccess && output[2][0] == 2);
    vectors[2][0] = std::numeric_limits<double>::infinity();
    const auto previous = output;
    assert(deform_curve(source,vectors,weights,1,1,false,0,output) == MS::kInvalidParameter);
    assert(output[0][0] == previous[0][0] && output[2][0] == previous[2][0]);
    assert(deform_point(source,vectors,weights,1,1,true,1,output) == MS::kInvalidParameter);
    DPoint value(7,8,9), sourcePoint(1,2,3);
    assert(offsetAlongVector(sourcePoint,Imath::V3d(1e300,0,0),1e300,1e-300,1e-300,value));
    assert(std::fabs(value[0]-2) < 1e-6);
    assert(offsetAlongVector(sourcePoint,Imath::V3d(1e-300,0,0),1e-300,1e300,1e300,value));
    assert(std::fabs(value[0]-2) < 1e-6);
    assert(offsetAlongVector(sourcePoint,Imath::V3d(1e-200,0,0),1e-120,1e200,1e120,value));
    assert(std::fabs(value[0]-2) < 1e-6);
    assert(offsetAlongVector(sourcePoint,Imath::V3d(2,0,0),2,-0.5,1,value) && value[0] == -1);
    value = DPoint(7,8,9);
    assert(!offsetAlongVector(sourcePoint,Imath::V3d(1e300,0,0),2,1,1,value) && value[0] == 7);
    assert(!offsetAlongVector(sourcePoint,Imath::V3d(0,0,0),std::numeric_limits<double>::infinity(),0,0,value));
    std::array<double,3> vector{{7,8,9}};
    const double large = std::numeric_limits<double>::max();
    assert(clampVectorMagnitude(Imath::V3d(large,large,large),1,vector));
    assert(std::fabs(vector[0]*vector[0]+vector[1]*vector[1]+vector[2]*vector[2]-1) < 1e-12);
    assert(clampVectorMagnitude(Imath::V3d(large,0,0),0,vector) && vector[0] == 0);
    assert(clampVectorMagnitude(Imath::V3d(0,0,0),1,vector) && vector[0] == 0);
    assert(clampVectorMagnitude(Imath::V3d(0.25,0,0),1,vector) && vector[0] == 0.25);
    assert(!clampVectorMagnitude(Imath::V3d(1,0,0),-1,vector) && vector[0] == 0.25);
    DPointList largeSource(4097,DPoint(0,0,0));
    vectors.assign(4097,Imath::V3d(1,0,0)); weights.assign(4097,1);
    assert(deform_curve(largeSource,vectors,weights,1,1,false,0,output) == MS::kSuccess && output.back()[0] == 1);
    vectors.back()[0] = 1e100;
    assert(deform_curve(largeSource,vectors,weights,1,1,false,0,output) == MS::kInvalidParameter && output.back()[0] == 1);
    assert(deform_point(largeSource,vectors,weights,1,1,true,1,output) == MS::kSuccess && output.back()[0] == 1);
}

int main()
{
    testInputs(); testGeneration(); testNormals(); testVectorMath();
    std::cout << "Fill/stitch, mesh query, and vector-deformation executable checks passed\n";
}
