// Production geometry/control flow, with float-backed storage and host seams.
#include <maya/MStatus.h>
#include <maya/MMatrix.h>
#include <maya/MVector.h>
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DParallelWork.h"
#include "common/utility/DUvParameter.h"
#include "modules/point/DPoint/mesh/DMeshDuplicationPlan.h"
#include "common/utility/DPointPayloadBudget.h"
#include <maya/MString.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <memory>
#include <map>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace host {
std::vector<std::string> trace;
int failAt = -1, liveMeshes = 0;
bool throwFailure = false;
MS::MStatusCode failure = MS::kFailure;
void reset() {trace.clear(); failAt = -1; throwFailure = false; failure = MS::kFailure;}
MStatus step(const char* name) {
    const int ordinal = static_cast<int>(trace.size());
    trace.push_back(name);
    if (ordinal != failAt) return MS::kSuccess;
    if (throwFailure) throw std::bad_alloc();
    return failure;
}
std::size_t calls(const char* name) {return std::count(trace.begin(), trace.end(), name);}
}

using DPoint = std::array<float, 3>;
using DPointList = std::vector<DPoint>;
struct MStringArray : std::vector<MString> {
    using std::vector<MString>::vector;
    unsigned int length() const { return static_cast<unsigned int>(size()); }
};
struct DPData {
    DPAttrList fields;
    std::size_t count = 2U;
    std::size_t numberOfPoints() const { return count; }
    bool isExistAttr(const std::string& name) const { return fields.count(name) != 0U; }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        const auto found = fields.find(name);
        return found == fields.end() ? DPAttributeData::ConstPtr() : found->second;
    }
    template <typename T> const std::vector<T>& attrListValue(const std::string& name) const {
        return std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(fields.at(name))->value();
    }
};
inline void EchoWarning(const char*, const char*) { host::trace.push_back("transfer.warning"); }
struct TestNodeName { const char* asChar() const { return "DPMeshDuplicator"; } };
inline TestNodeName name() { return {}; }
struct MFloatPoint {
    float x, y, z, w;
    MFloatPoint(float a = 0, float b = 0, float c = 0) : x(a), y(b), z(c), w(1) {}
    float operator[](unsigned int i) const {return i == 0U ? x : i == 1U ? y : i == 2U ? z : w;}
};
struct MPoint {
    double x, y, z, w;
    explicit MPoint(const MFloatPoint& point) : x(point.x), y(point.y), z(point.z), w(point.w) {}
    MPoint(double a, double b, double c, double d = 1) : x(a), y(b), z(c), w(d) {}
    double operator[](unsigned int i) const {return i == 0U ? x : i == 1U ? y : i == 2U ? z : w;}
};
// Row-vector multiplication seam; native Maya rounding/projective behavior is
// not proved here. Production continues to call Maya's own multiplication.
MPoint operator*(const MPoint& point, const MMatrix& matrix) {
    double values[4] = {};
    for (unsigned int column = 0; column < 4; ++column)
        for (unsigned int row = 0; row < 4; ++row)
            values[column] += point[row] * matrix[row][column];
    return MPoint(values[0], values[1], values[2], values[3]);
}
MMatrix operator*(const MMatrix& left, const MMatrix& right) {
    MMatrix result;
    for (unsigned int row = 0; row < 4; ++row)
        for (unsigned int column = 0; column < 4; ++column) {
            result[row][column] = 0;
            for (unsigned int k = 0; k < 4; ++k)
                result[row][column] += left[row][k] * right[k][column];
        }
    return result;
}
namespace MSpace {enum Space {kObject};}
struct MTransformationMatrix {
    MMatrix value;
    MStatus setScale(const double* scale, MSpace::Space space) {
        assert(space == MSpace::kObject);
        const MStatus status = host::step("scale");
        if (status) for (unsigned int axis = 0; axis < 3; ++axis) value[axis][axis] = scale[axis];
        return status;
    }
    MMatrix asMatrix() const {return value;}
};
template <typename T> struct Array {
    std::vector<T> values;
    Array() = default;
    Array(std::initializer_list<T> input) : values(input) {}
    unsigned int length() const {return static_cast<unsigned int>(values.size());}
    const T& operator[](std::size_t i) const {return values.at(i);}
    T& operator[](std::size_t i) {return values.at(i);}
    MStatus append(const T& value) {
        const MStatus status = host::step("uv.append");
        if (status) values.push_back(value);
        return status;
    }
    MStatus copy(const Array& source) {
        const MStatus status = host::step("uv.copy");
        if (status) values = source.values;
        return status;
    }
};
using MFloatPointArray = Array<MFloatPoint>;
using MFloatArray = Array<float>;
using MIntArray = Array<int>;

// Serial execution seam for the unchanged base-array copy template.
namespace tbb {
template <typename T> struct blocked_range {
    T first, last;
    blocked_range(T a, T b, std::size_t grain) : first(a), last(b) {assert(grain > 0);}
    T begin() const {return first;}
    T end() const {return last;}
};
template <typename Range, typename Function>
void parallel_for(const Range& range, Function function) {function(range);}
}

struct dupMesh {
    MFloatPointArray points{{1, 2, 3}, {-4, 5, -6}, {7, -8, 9}};
    MIntArray counts{3}, connections{0, 1, 2}, uvCounts{3}, uvIds{0, 1, 2};
    MFloatArray u{0, 1, 0}, v{0, 0, 1};
    bool uv = true;
    const MFloatPointArray& getVertexArray() const {return points;}
    const MIntArray& getPolyCountsList() const {return counts;}
    const MIntArray& getPolyConnectList() const {return connections;}
    const MFloatArray& getUCoords() const {return u;}
    const MFloatArray& getVCoords() const {return v;}
    const MIntArray& getUVCountsArray() const {return uvCounts;}
    const MIntArray& getUVIdsArray() const {return uvIds;}
    bool hasUVData() const {return uv;}
    std::size_t getTotalVertexNum() const {return points.length();}
    std::size_t getTotalPolygonNum() const {return counts.length();}
    std::size_t getTotalConnectionNum() const {return connections.length();}
    std::size_t getTotalUVNum() const {return u.length();}
};
struct DMeshInternalData {
    DPointList points;
    MIntArray counts, connections, uvCounts, uvIds;
    MFloatArray u, v;
    MMatrix matrix;
    DMeshInternalData(const DPointList& p, const MIntArray& c, const MIntArray& ids)
        : points(p), counts(c), connections(ids) {
        if (!host::step("mesh.construct")) throw std::bad_alloc();
        ++host::liveMeshes;
    }
    DMeshInternalData(const DPointList& p, const MIntArray& c, const MIntArray& ids,
        const MFloatArray& us, const MFloatArray& vs, const MIntArray& uc, const MIntArray& ui)
        : points(p), counts(c), connections(ids), uvCounts(uc), uvIds(ui), u(us), v(vs) {
        if (!host::step("mesh.constructUV")) throw std::bad_alloc();
        ++host::liveMeshes;
    }
    ~DMeshInternalData() {--host::liveMeshes;}
    void setTransformMatrix(const MMatrix& value) {matrix = value;}
    bool hasValidMeshData() const {
        if (!host::step("mesh.validate")) return false;
        for (const auto& point : points)
            for (float component : point) if (!std::isfinite(component)) return false;
        return true;
    }
};
using DMItem = std::shared_ptr<DMeshInternalData>;
struct DMeshData {
    using DMeshList = std::vector<DMItem>;
    DMeshList items;
    DPAttrList fields;
    template <typename T>
    void addDPAttributeValues(const std::string& fieldName, const std::vector<T>& values) {
        assert(values.size() == items.size());
        host::trace.push_back("transfer.write");
        fields[fieldName] = std::make_shared<TypedDPAttributeData<T>>(values);
    }
    bool insert(const DMeshList& input) {
        if (!host::step("batch.commit")) return false;
        items.insert(items.end(), input.begin(), input.end());
        return true;
    }
};
#include "mesh_duplication_vertices.inc"

struct Fixture {
    std::vector<dupMesh> sources{dupMesh(), dupMesh()};
    std::vector<int> ids{1, 0};
    std::vector<MMatrix> matrices{MMatrix(), MMatrix()};
    std::vector<MVector> scales{MVector(2, 3, 4), MVector(1, 1, 1)};
    DMeshData destination;
    DPData inputPoints;
    MStringArray transfers;
    bool outputUV = true;
    int uvMode = 0, matrixMode = 1;
    Fixture() {
        host::reset();
        matrices[0][0][0] = 0; matrices[0][0][1] = 1;
        matrices[0][1][0] = -1; matrices[0][1][1] = 0; matrices[0][1][2] = .5;
        matrices[0][2][1] = .25;
        matrices[0][3][0] = 10; matrices[0][3][1] = -20; matrices[0][3][2] = 30;
        sources[1].points = {{-1, 3, 2}, {5, -2, 1}, {4, 7, -8}};
        destination.items.push_back(std::make_shared<DMeshInternalData>(
            DPointList{{{91, 92, 93}}}, MIntArray{3}, MIntArray{0, 0, 0}));
        host::reset();
    }
    MStatus run() {
        inputPoints.count = ids.size();
        return generateDuplicateGeometry(sources, ids, matrices, scales,
            outputUV, uvMode, matrixMode, inputPoints, transfers, destination);
    }
    void preserved() const {
        assert(destination.items.size() == 1);
        assert((destination.items[0]->points[0] == DPoint{{91, 92, 93}}));
        assert(host::liveMeshes == 1);
    }
};
void sameMatrix(const MMatrix& a, const MMatrix& b) {
    for (unsigned int row = 0; row < 4; ++row)
        for (unsigned int column = 0; column < 4; ++column) assert(a[row][column] == b[row][column]);
}
void successfulGeometry() {
    for (int mode : {0, 1}) for (bool uv : {false, true}) for (int uvMode : {0, 1, 2}) {
        Fixture f; f.matrixMode = mode; f.outputUV = uv; f.uvMode = uvMode;
        assert(f.run() == MS::kSuccess && f.destination.items.size() == 3);
        assert(host::calls("batch.commit") == 1);
        for (std::size_t mesh = 0; mesh < 2; ++mesh) {
            const auto& result = *f.destination.items[mesh + 1];
            const auto& source = f.sources[f.ids[mesh]];
            MMatrix expected = f.matrices[mesh];
            // Independent oracle: scale each matrix row, including rotation/
            // shear, while leaving the translation row unchanged.
            for (unsigned int row = 0; row < 3; ++row)
                for (unsigned int column = 0; column < 4; ++column)
                    expected[row][column] *= f.scales[mesh][row];
            sameMatrix(result.matrix, mode == 0 ? expected : MMatrix());
            for (unsigned int point = 0; point < source.points.length(); ++point)
                for (unsigned int axis = 0; axis < 3; ++axis) {
                    double transformed = f.matrices[mesh][3][axis];
                    for (unsigned int row = 0; row < 3; ++row)
                        transformed += double(source.points[point][row]) *
                            f.scales[mesh][row] * f.matrices[mesh][row][axis];
                    const float value = mode == 0 ? source.points[point][axis] : static_cast<float>(transformed);
                    assert(result.points[point][axis] == value);
                }
            assert(result.counts.values == source.counts.values && result.connections.values == source.connections.values);
            assert(result.u.length() == (uv ? 3U : 0U));
            if (uv) {
                assert(result.uvCounts.values == source.uvCounts.values && result.uvIds.values == source.uvIds.values);
                for (unsigned int point = 0; point < 3; ++point) {
                    const float u = uvMode == 0 ? source.u[point] :
                        uvMode == 1 ? float(double(source.u[point]) / 2.0 + double(mesh) / 2.0) :
                                      float((double(source.u[point]) + double(mesh)) / 2.0);
                    const float v = uvMode == 2 ? source.v[point] / 2.0f : source.v[point];
                    assert(result.u[point] == u && result.v[point] == v);
                }
            }
        }
    }
    assert(host::liveMeshes == 0);
}
void numericBoundaries() {
    const float maximum = std::numeric_limits<float>::max();
    for (unsigned int axis = 0; axis < 3; ++axis) for (float sign : {-1.0f, 1.0f}) {
        Fixture f; f.ids = {0}; f.scales = {MVector(1, 1, 1)}; f.matrices = {MMatrix()};
        f.sources[0].points.values.back() = MFloatPoint();
        if (axis == 0) f.sources[0].points.values.back().x = sign * maximum;
        if (axis == 1) f.sources[0].points.values.back().y = sign * maximum;
        if (axis == 2) f.sources[0].points.values.back().z = sign * maximum;
        assert(f.run() == MS::kSuccess);
        assert(f.destination.items[1]->points.back()[axis] == sign * maximum);
        f.destination.items.resize(1); host::reset();
        const double outside = std::nextafter(double(sign * maximum),
            double(sign) * std::numeric_limits<double>::infinity());
        f.matrices[0][3][axis] = outside - double(sign * maximum);
        assert(f.run() == MS::kInvalidParameter);
        assert(host::calls("mesh.constructUV") == 0 && host::calls("batch.commit") == 0); f.preserved();
        host::reset(); f.matrices[0][3][axis] = 0; f.matrices[0][axis][axis] = 2;
        assert(f.run() == MS::kInvalidParameter); f.preserved();
        // Retaining a transform does not require its evaluated vertices to fit float.
        host::reset(); f.matrixMode = 0;
        assert(f.run() == MS::kSuccess && f.destination.items[1]->points.back()[axis] == sign * maximum);
    }
    for (double invalid : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        Fixture f; f.matrices[0][0][0] = invalid;
        assert(f.run() == MS::kInvalidParameter && host::calls("batch.commit") == 0); f.preserved();
    }
    {
        Fixture f; f.sources[0].points.values.back().x = maximum; f.matrices[1][0][0] = 2;
        assert(f.run() == MS::kInvalidParameter);
        assert(host::calls("mesh.constructUV") == 1 && host::calls("batch.commit") == 0); f.preserved();
        f.matrices[1][0][0] = 1; host::reset(); assert(f.run() == MS::kSuccess);
    }
    {
        Fixture f; f.scales[0] = MVector(1, 1, 1);
        f.sources[1].points.values.front().x = 2;
        f.matrices[0][0][0] = std::numeric_limits<double>::max();
        assert(f.run() == MS::kInvalidParameter && host::calls("batch.commit") == 0); f.preserved();
    }
    assert(host::liveMeshes == 0);
}
void failureBoundaries() {
    int tested = 0;
    for (bool uv : {false, true}) {
        std::vector<std::string> trace;
        {Fixture f; f.outputUV = uv; assert(f.run() == MS::kSuccess); trace = host::trace;}
        for (std::size_t at = 0; at < trace.size(); ++at) for (int fault : {0, 1, 2}) {
            Fixture f; f.outputUV = uv; host::failAt = static_cast<int>(at);
            host::throwFailure = fault == 2;
            host::failure = fault == 1 ? MS::kInvalidParameter : MS::kFailure;
            const MS::MStatusCode expected = fault == 1 &&
                (trace[at] == "scale" || trace[at] == "uv.copy") ? MS::kInvalidParameter : MS::kFailure;
            assert(f.run() == expected);
            assert(host::trace.size() == at + 1); f.preserved(); ++tested;
            host::reset(); assert(f.run() == MS::kSuccess);
        }
    }
    assert(host::liveMeshes == 0);
    std::cout << tested << " geometry/UV/commit failure and retry cases passed\n";
}
template <typename T> void sameValue(const T& left, const T& right) {
    using Parts = dommy::numeric::ValueComponents<T>;
    for (unsigned int axis = 0U; axis < Parts::count; ++axis)
        assert(Parts::get(left, axis) == Parts::get(right, axis));
}
void sameValue(const MMatrix& left, const MMatrix& right) { sameMatrix(left, right); }
template <typename T> void transferFamily(const T& value, const T& sourceDefault) {
    Fixture f; f.destination.items.clear(); f.transfers = {"field"};
    const auto input = std::make_shared<TypedDPAttributeData<T>>(std::vector<T>(2U, value), sourceDefault);
    f.inputPoints.fields["field"] = input;
    assert(f.run() == MS::kSuccess);
    const auto output = std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(f.destination.fields.at("field"));
    assert(output && output != input && output->value().size() == 2U);
    const TypedDPAttributeData<T> historicalDefault;
    sameValue(output->defaultValue(), historicalDefault.defaultValue());
    for (const T& row : output->value()) sameValue(row, value);
    sameValue(input->defaultValue(), sourceDefault);
}
void transferFamilies() {
    transferFamily<bool>(true, true); transferFamily<int32_t>(-7, 19);
    transferFamily<uint32_t>(4000000000U, 99U);
    transferFamily<int64_t>(9007199254740993LL, 77);
    transferFamily<float>(1.25f, 5.0f); transferFamily<double>(1.25, 5.0);
    transferFamily<Imath::V2i>({1,2}, {5,6}); transferFamily<Imath::V2f>({1,2}, {5,6});
    transferFamily<Imath::V2d>({1,2}, {5,6}); transferFamily<Imath::V3i>({1,2,3}, {5,6,7});
    transferFamily<Imath::V3f>({1,2,3}, {5,6,7}); transferFamily<Imath::V3d>({1,2,3}, {5,6,7});
    transferFamily<std::string>("value", "source-default");
    MMatrix matrix, sourceDefault; matrix[3][0] = 19; sourceDefault[3][1] = 27;
    transferFamily<MMatrix>(matrix, sourceDefault);
    assert(host::liveMeshes == 0);
    std::cout << "All 14 legacy transfer types, value precision and type-default policies passed\n";
}
void admissionBoundaries() {
    {
        Fixture f; f.sources[0].points.values.assign(50U, MFloatPoint());
        f.sources[1].points.values.assign(50U, MFloatPoint());
        assert(f.run() == MS::kInvalidParameter);
        assert(host::calls("mesh.constructUV") == 0 && host::calls("batch.commit") == 0);
        f.preserved();
        f.sources[0].points.values.resize(3U); f.sources[1].points.values.resize(3U);
        host::reset(); assert(f.run() == MS::kSuccess);
    }
    {
        Fixture f; f.ids.assign(8U, 0); f.matrices.assign(8U, MMatrix());
        f.scales.assign(8U, MVector(1,1,1));
        assert(f.run() == MS::kInvalidParameter);
        assert(host::calls("mesh.constructUV") == 0 && host::calls("batch.commit") == 0);
        f.preserved();
    }
    {
        Fixture f;
        for (const char* field : {"a", "b", "c", "d"}) {
            f.transfers.emplace_back(field);
            f.inputPoints.fields[field] = std::make_shared<DPMat4Attr>(std::vector<MMatrix>(2U));
        }
        assert(f.run() == MS::kInvalidParameter);
        assert(host::calls("mesh.constructUV") == 0 && host::calls("transfer.write") == 0);
        f.preserved();
        f.inputPoints.fields.erase("d"); f.destination.items.clear(); host::reset();
        assert(f.run() == MS::kSuccess && f.destination.items.size() == 2U);
        assert(f.destination.fields.size() == 3U);
    }
    {
        Fixture f;
        for (const char* field : {"a", "b"}) {
            f.transfers.emplace_back(field);
            f.inputPoints.fields[field] = std::make_shared<DPStringAttr>(
                std::vector<std::string>(2U, std::string(450U, 'x')));
        }
        assert(f.run() == MS::kFailure);
        assert(host::calls("mesh.constructUV") == 0 && host::calls("transfer.write") == 0);
        f.preserved();
    }
    {
        Fixture f; f.destination.items.clear();
        f.inputPoints.fields["s"] = std::make_shared<DPStringAttr>(
            std::vector<std::string>(2U, "x"), std::string(8192U, 'd'));
        f.inputPoints.fields["array"] = std::make_shared<TypedDPAttributeData<DIntArray>>(
            std::vector<DIntArray>(2U, DIntArray(20U, 1)));
        f.transfers = {"s", "missing", "s", "array"};
        assert(f.run() == MS::kSuccess && f.destination.items.size() == 2U);
        const auto field = std::dynamic_pointer_cast<const DPStringAttr>(f.destination.fields.at("s"));
        assert(field->value() == std::vector<std::string>({"x", "x"}) && field->defaultValue().empty());
        assert(f.inputPoints.fields.at("s") != field && f.destination.fields.size() == 1U);
        assert(host::calls("transfer.warning") == 1U);
    }
    assert(host::liveMeshes == 0);
    std::cout << "Duplicator: pre-generation geometry/UV/retained/transfer aggregate admission and retry passed\n";
}
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "budget") { admissionBoundaries(); return 0; }
    assert(argc == 1);
    successfulGeometry(); numericBoundaries(); failureBoundaries(); transferFamilies();
    std::cout << "Production duplicator loop: float admission, matrix modes/order and pre-commit preservation passed\n";
}
