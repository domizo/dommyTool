#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "modules/curve/DCurve/Generate/DCurveFromCurvePlan.h"
#include <maya/MMatrix.h>
#include <maya/MString.h>
#include <array>
#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>

#define CHECK_MSTATUS(x)                                                                           \
    do {                                                                                           \
        const MStatus s = (x);                                                                     \
        if (!s)                                                                                    \
            s.perror("test");                                                                      \
    } while (false)
#define CHECK_MSTATUS_AND_RETURN_IT(x) DOMMY_RETURN_IF_FAILURE(x)
namespace {
int nullAt = -1, throwAt = -1, dagCalls = 0, ownerCalls = 0, transformedCalls = 0;
int closedExtraCvs = 0;
MStatus api(const std::string& name) {
    const int index = callCount;
    const auto status = operation(name);
    if (index == throwAt)
        throw std::runtime_error("injected error");
    return status;
}
void reset(int index = -1) {
    resetCalls(index);
    nullAt = throwAt = -1;
    dagCalls = ownerCalls = transformedCalls = 0;
}
} // namespace
struct MPoint {
    double x, y, z;
    MPoint(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
};
using MPointArray = std::vector<MPoint>;
struct DCurveInternalData {
    bool valid = true, open = true;
    std::size_t count = 4;
    MPointArray cvs = {{0, 0, 0}, {1, 2, 3}, {2, 4, 6}, {3, 6, 9}};
    bool hasValidCurveTopology() const {
        return valid;
    }
    std::size_t numberOfPoints() const {
        return count;
    }
    bool isCurveOpen() const {
        return open;
    }
    const MPointArray& getCvs() const {
        return cvs;
    }
    std::vector<double> getKnots() const {
        return {0, 0, 0, 1, 1, 1};
    }
    int getDegree() const {
        return 3;
    }
};
using DCItem = std::shared_ptr<DCurveInternalData>;
using ConstDCItem = std::shared_ptr<const DCurveInternalData>;
struct DCurveData {
    static constexpr MFnData::Type id = MFnData::kAny;
    using ConstDCurveIter = std::vector<DCItem>::const_iterator;
    std::vector<DCItem> curves;
    bool valid = true;
    std::size_t numberOfCurves() const {
        return curves.size();
    }
    bool hasValidCurveTopology() const {
        return valid;
    }
    ConstDCurveIter beginCurves() const {
        return curves.begin();
    }
    ConstDCurveIter endCurves() const {
        return curves.end();
    }
};
struct DPData {
    static constexpr MFnData::Type id = MFnData::kAny;
    bool valid = true;
    std::size_t count = 2;
    bool hasValidPointTopology() const {
        return valid;
    }
    std::size_t numberOfPoints() const {
        return count;
    }
};
struct Geometry {
    MPointArray cvs;
    std::size_t cvCount = 4;
};
namespace {
std::map<std::string, Geometry> geometries;
DCurveData sourceData;
DPData profileData;
dommy::curve::CurveFromCurvePlan preparedPlan;
std::vector<MObject> preparedObjects;
bool preparedTangent = false;
int connectionAdmissions = 0;
bool transformConnected = true, transformHasParent = true;
} // namespace
namespace MSpace {
enum Space { kObject, kWorld };
}
struct MFnNurbsCurveData {
    MObject create(MStatus* status = nullptr) {
        const int index = callCount;
        const auto s = api("owner.create");
        ++ownerCalls;
        if (status)
            *status = s;
        if (!s || index == nullAt)
            return MObject::kNullObj;
        const MObject result{"owner" + std::to_string(ownerCalls)};
        geometries[result.name] = Geometry();
        return result;
    }
};
struct MFnNurbsCurve {
    enum Form { kOpen, kClosed };
    MObject object;
    MFnNurbsCurve() = default;
    MFnNurbsCurve(const MObject& value, MStatus* status) {
        *status = setObject(value);
    }
    MStatus setObject(const MObject& value) {
        const auto status = api("curve.bind");
        if (!status)
            return status;
        assert(!value.isNull() && geometries.count(value.name));
        object = value;
        return MS::kSuccess;
    }
    MObject create(const MPointArray& cvs, const std::vector<double>&, int, Form form, bool, bool,
                   MObject& owner, MStatus* status) {
        const int index = callCount;
        *status = api("curve.create");
        if (owner.isNull())
            ++dagCalls;
        if (!*status || index == nullAt)
            return MObject::kNullObj;
        auto& result = geometries[owner.name];
        result.cvs = cvs;
        result.cvCount = cvs.size() + (form == kClosed ? closedExtraCvs : 0);
        object = owner;
        return MObject{"geometry"};
    }
    int numCVs(MStatus* status) const {
        *status = api("curve.count");
        return *status ? static_cast<int>(geometries.at(object.name).cvCount) : 0;
    }
    MStatus getCVs(MPointArray& result, MSpace::Space space) const {
        assert(space == MSpace::kObject); // A geometry-data object has no DAG path.
        const auto status = api("curve.cvs");
        if (status)
            result = geometries.at(object.name).cvs;
        return status;
    }
};
struct Input {
    double number = 0;
    bool connected = false;
    MFnData::Type kind = MFnData::kNurbsCurve;
    MObject object{"source"};
    MMatrix matrix;
};
namespace {
std::map<std::string, Input> inputs;
}
struct MDataHandle {
    Input* value = nullptr;
    bool valid = true;
    void check() const {
        assert(valid && value);
    }
    int asInt() const {
        check();
        return static_cast<int>(value->number);
    }
    short asShort() const {
        check();
        return static_cast<short>(value->number);
    }
    bool asBool() const {
        check();
        return value->number != 0;
    }
    double asDouble() const {
        check();
        return value->number;
    }
    MFnData::Type type() const {
        check();
        return value->kind;
    }
    MObject data() const {
        check();
        return value->object;
    }
    MMatrix geometryTransformMatrix() const {
        check();
        return value->matrix;
    }
    MObject asNurbsCurveTransformed() const {
        check();
        ++transformedCalls;
        if (value->object.isNull())
            return MObject::kNullObj;
        const MObject result{value->object.name + ".transformed"};
        Geometry candidate = geometries.at(value->object.name);
        for (MPoint& p : candidate.cvs) {
            const double v[] = {p.x, p.y, p.z, 1};
            double out[3] = {};
            for (unsigned i = 0; i < 3; ++i)
                for (unsigned j = 0; j < 4; ++j)
                    out[i] += v[j] * value->matrix[j][i];
            p = MPoint(out[0], out[1], out[2]);
        }
        geometries[result.name] = candidate;
        return result;
    }
};
struct MDataBlock {
    MDataHandle inputValue(const MObject& attribute, MStatus* status = nullptr) {
        const auto result = api("input " + attribute.name);
        if (status)
            *status = result;
        MDataHandle handle;
        handle.value = &inputs[attribute.name];
        handle.valid = bool(result);
        return handle;
    }
};
struct MPlugArray;
struct MPlug {
    std::string name;
    MPlug(const MObject&, const MObject& attribute) : name(attribute.name) {}
    bool operator!=(const MObject& attribute) const {
        return name != attribute.name;
    }
    bool isConnected(MStatus* status = nullptr) const {
        const auto result = api("connected " + name);
        if (status) *status = result;
        // Record failures even if production forgets the status argument.
        return result && inputs.at(name).connected;
    }
    bool connectedTo(MPlugArray&, bool, bool, MStatus*) const;
    MObject node() const { return {"connectedShape"}; }
};
struct MPlugArray : std::vector<MPlug> {
    unsigned int length() const { return static_cast<unsigned int>(size()); }
};
bool MPlug::connectedTo(MPlugArray& plugs, bool asDestination, bool asSource,
                        MStatus* status) const {
    assert(asDestination && !asSource);
    *status = api("connectedTo " + name);
    if (*status && transformConnected) plugs.emplace_back(MObject{}, MObject{"source"});
    return *status && transformConnected;
}
struct MFnDependencyNode {
    MFnDependencyNode(const MObject&, MStatus* status) { *status=api("node.bind"); }
    MPlug findPlug(const MObject& attr, bool networked, MStatus* status) const {
        assert(networked); *status=api("node.findPlug"); return MPlug({},attr);
    }
};
struct MFnDagNode {
    MObject object{"sentinel"};
    MFnDagNode() = default;
    MFnDagNode(const MObject& object, MStatus* status):object(object) { *status=api("dag.bind"); }
    unsigned int parentCount(MStatus* status) const {
        *status=api("dag.parentCount"); return transformHasParent?1U:0U;
    }
    MObject parent(unsigned int index, MStatus* status) const {
        assert(index==0U); *status=api("dag.parent"); return {"transform"};
    }
    MStatus setObject(const MObject& value) {
        const auto status=api("dag.setObject"); if(status)object=value; return status;
    }
};
MObject thisMObject() { return {"node"}; }
MString name() { return "connection probe"; }
void EchoWarning(const char*, const char*) {}
namespace dommy {
namespace maya {
bool isFiniteMatrix(const MMatrix& matrix) {
    for (unsigned r = 0; r < 4; ++r)
        for (unsigned c = 0; c < 4; ++c)
            if (!std::isfinite(matrix[r][c]))
                return false;
    return true;
}
MStatus getPluginDataFromHandle(MDataHandle& handle, MFnData::Type, const DPData*& data) {
    handle.check();
    const auto status = api("profile.read");
    if (status)
        data = &profileData;
    return status;
}
} // namespace maya
} // namespace dommy
MStatus getValidInputDC(const MObject& attribute, MDataBlock& data, const DCurveData*& result) {
    MStatus status;
    data.inputValue(attribute, &status);
    if (!status)
        return status;
    if (!sourceData.hasValidCurveTopology())
        return MS::kInvalidParameter;
    result = &sourceData;
    return MS::kSuccess;
}
// Compatibility-only helper for testing an archived preflight mutation.
const DPData* getInputDP(const MObject& attribute, MDataBlock& data) {
    MStatus status;
    data.inputValue(attribute, &status);
    return status ? &profileData : nullptr;
}
const DCurveData* getInputDC(const MObject& attribute, MDataBlock& data) {
    const DCurveData* result = nullptr;
    return getValidInputDC(attribute, data, result) ? result : nullptr;
}
#include "curve_boundary_geometry.inc"

namespace {
void configure(bool mayaCurve, bool tangent = false) {
    reset();
    inputs.clear();
    geometries.clear();
    closedExtraCvs = 0;
    sourceData = DCurveData();
    profileData = DPData();
    sourceData.curves = {DCItem(new DCurveInternalData), DCItem(new DCurveInternalData)};
    geometries["source"].cvs = sourceData.curves[0]->cvs;
    geometries["tangent"].cvs = sourceData.curves[1]->cvs;
    inputs["aSamplingType"].number = 0;
    inputs["curveLength"].number = 1;
    inputs["sampling"].number = 8;
    inputs["aCurveDivide"].number = 1;
    inputs["aDegree"].number = 1;
    inputs["aIsMultiEndKnot"].number = 1;
    inputs["inCurve"].connected = mayaCurve;
    inputs["inDC"].connected = !mayaCurve;
    inputs["aInTangentCurve"].connected = tangent;
    inputs["aInTangentCurve"].object = MObject{"tangent"};
    preparedPlan.outputPointCount = 999;
    preparedObjects.clear();
    preparedTangent = false;
}
MStatus prepare() {
    DCurveFromCurve node;
    MDataBlock data;
    return node.prepare(MPlug(node.thisMObject(), DCurveFromCurve::outDC), data);
}
void owners() {
    for (bool immutable : {false, true}) {
        DCItem curve(new DCurveInternalData);
        ConstDCItem constCurve = curve;
        const auto create = [&](MObject& out) {
            return immutable ? generateMCurveFromDcurve(constCurve, out)
                             : generateMCurveFromDcurve(curve, out);
        };
        reset();
        MObject output{"sentinel"};
        assert(create(output) == MS::kSuccess && output.name == "owner1" && dagCalls == 0);
        for (int index = 0; index < 2; ++index) {
            for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
                reset(index);
                injectedStatus = code;
                output = MObject{"sentinel"};
                assert(create(output) == code && output.name == "sentinel" && dagCalls == 0);
                assert(callCount == index + 1);
            }
            reset();
            nullAt = index;
            output = MObject{"sentinel"};
            assert(create(output) == MS::kFailure && output.name == "sentinel" && dagCalls == 0);
            assert(callCount == index + 1);
        }
        reset();
        curve->valid = false;
        output = MObject{"sentinel"};
        assert(create(output) == MS::kInvalidParameter && callCount == 0 &&
               output.name == "sentinel");
    }
}
int failures() {
    int tested = 0;
    for (bool mayaCurve : {false, true})
        for (bool tangent : {false, true}) {
            configure(mayaCurve, tangent);
            assert(prepare() == MS::kSuccess);
            const int count = callCount;
            for (int index = 0; index < count; ++index) {
                for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
                    configure(mayaCurve, tangent);
                    failAt = index;
                    injectedStatus = code;
                    assert(prepare() == code && callCount == index + 1 && dagCalls == 0);
                    assert(preparedPlan.outputPointCount == 999 && preparedObjects.empty());
                    ++tested;
                }
                configure(mayaCurve, tangent);
                throwAt = index;
                assert(prepare() == MS::kFailure && preparedObjects.empty());
                ++tested;
                configure(mayaCurve, tangent);
                assert(prepare() == MS::kSuccess);
            }
        }
    return tested;
}
void coordinateInput() {
    configure(true);
    auto& matrix = inputs["inCurve"].matrix;
    matrix[0][0] = 0;
    matrix[0][1] = 2;
    matrix[1][0] = -3;
    matrix[1][1] = 0;
    matrix[2][2] = 4;
    matrix[3][0] = 10;
    matrix[3][1] = 20;
    matrix[3][2] = 30;
    assert(prepare() == MS::kSuccess && transformedCalls == 1 && ownerCalls == 0);
    MPointArray cvs;
    assert(copyCvs(preparedObjects[0], cvs) == MS::kSuccess);
    assert(cvs[1].x == 4 && cvs[1].y == 22 && cvs[1].z == 42);
    assert(geometries.at("source").cvs[1].x == 1); // Read-only source.
    assert(copyCvs(preparedObjects[0], cvs) == MS::kSuccess && transformedCalls == 1);
    for (int variant = 0; variant < 3; ++variant) {
        configure(true);
        if (variant == 0)
            inputs["inCurve"].kind = MFnData::kMesh;
        if (variant == 1)
            inputs["inCurve"].matrix[0][0] = std::numeric_limits<double>::quiet_NaN();
        if (variant == 2)
            inputs["inCurve"].object = MObject::kNullObj;
        MDataBlock data;
        MObject output{"sentinel"};
        assert(DGetMayaCurveFromPlug(DCurveFromCurve::inCurve, data, output) != MS::kSuccess);
        assert(output.name == "sentinel");
    }
    configure(true, true);
    inputs["aInTangentCurve"].object = MObject::kNullObj;
    assert(prepare() == MS::kFailure && preparedObjects.empty());
    configure(true, false);
    inputs["aInTangentCurve"].object = MObject::kNullObj;
    assert(prepare() == MS::kSuccess && !preparedTangent);
}
void connectionSelection() {
    configure(true, true);
    inputs["inDC"].connected = true;
    sourceData.valid = false;
    assert(prepare() == MS::kSuccess && preparedObjects.size() == 1 && preparedTangent);
    assert(std::find(operations.begin(), operations.end(), "connected inDC") == operations.end());
    assert(std::find(operations.begin(), operations.end(), "input inDC") == operations.end());

    configure(false);
    inputs["inDC"].connected = false;
    assert(prepare() == MS::kInvalidParameter && preparedObjects.empty() && ownerCalls == 0);
}
void relatedConnectionFailures() {
    using Probe = MStatus (*)();
    for (Probe probe : {spiralConnections, timeShapeConnections, edgeDirectionConnections}) {
        for (bool tangent : {false, true}) {
            const auto setup = [&]() {
                configure(true,tangent);
                connectionAdmissions=0;
                inputs["inputCurve"]=inputs["inCurve"];
                inputs["inputTangentCurve"]=inputs["aInTangentCurve"];
                inputs["inMesh"].connected=inputs["aInObject"].connected=true;
            };
            setup(); assert(probe()==MS::kSuccess && connectionAdmissions==1);
            if(probe==spiralConnections)assert(preparedTangent==tangent);
            const int count=callCount;
            for(int index=0;index<count;++index) {
                for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
                    setup();failAt=index;injectedStatus=code;
                    assert(probe()==code && callCount==index+1 && connectionAdmissions==0);
                    setup();assert(probe()==MS::kSuccess && connectionAdmissions==1);
                }
                setup();throwAt=index;
                assert(probe()==MS::kFailure && connectionAdmissions==0);
            }
            setup();
            for(const auto& attr:{"inputCurve","inMesh","aInObject"})inputs[attr].connected=false;
            assert(probe()==MS::kFailure && connectionAdmissions==0);
        }
    }
}
MStatus transformProbe(MFnDagNode& result) try {
    return getTransformNodeFromPlug({"shape"},{"matrix"},result);
} DOMMY_COMPUTE_CATCH("transform connection")
void transformConnectionFailures() {
    const auto setup=[]() {
        reset();inputs["matrix"].connected=true;
        transformConnected=transformHasParent=true;
    };
    setup();MFnDagNode result;assert(transformProbe(result)==MS::kSuccess);
    assert(result.object.name=="transform");const int count=callCount;
    for(int index=0;index<count;++index) {
        for(auto code:{MS::kFailure,MS::kInvalidParameter}) {
            setup();failAt=index;injectedStatus=code;result=MFnDagNode();
            assert(transformProbe(result)==code && callCount==index+1);
            assert(result.object.name=="sentinel");
        }
        setup();throwAt=index;result=MFnDagNode();
        assert(transformProbe(result)==MS::kFailure && result.object.name=="sentinel");
        setup();assert(transformProbe(result)==MS::kSuccess);
    }
    for(bool missingConnection:{false,true}) {
        setup();transformConnected=!missingConnection;transformHasParent=missingConnection;
        result=MFnDagNode();assert(transformProbe(result)==MS::kFailure);
        assert(result.object.name=="sentinel");
    }
    reset();assert(getTransformNodeFromPlug({}, {"matrix"},result)==MS::kInvalidParameter);
    assert(callCount==0);
}
void budgets() {
    configure(false);
    profileData.count = dommy::generation::kMaxGeneratedCurveCount;
    assert(prepare() == MS::kInvalidParameter && ownerCalls == 0 && preparedObjects.empty());
    configure(false);
    inputs["sampling"].number = dommy::generation::kMaxGeneratedPointCount;
    assert(prepare() == MS::kInvalidParameter && ownerCalls == 0);
    configure(false);
    inputs["aSamplingType"].number = 1;
    assert(prepare() == MS::kSuccess && preparedPlan.outputPointCount == 28 && ownerCalls == 2);
    configure(false);
    inputs["aSamplingType"].number = 1;
    sourceData.curves[0]->open = false;
    closedExtraCvs = 1;
    assert(prepare() == MS::kSuccess && preparedPlan.outputPointCount == 32);
    configure(false);
    inputs["aSamplingType"].number = 1;
    sourceData.curves[0]->open = false;
    closedExtraCvs = dommy::generation::kMaxGeneratedPointCount;
    assert(prepare() == MS::kInvalidParameter && preparedObjects.empty());
    configure(false);
    sourceData.valid = false;
    assert(prepare() == MS::kInvalidParameter && ownerCalls == 0);
    configure(false);
    profileData.valid = false;
    assert(prepare() == MS::kInvalidParameter && ownerCalls == 0);
}
} // namespace
int main() {
    owners();
    const int count = failures();
    coordinateInput();
    connectionSelection();
    relatedConnectionFailures();
    transformConnectionFailures();
    budgets();
    std::cout << "Curve geometry: both ownership overloads, " << count
              << " preparation failure cases, transformed read-only input and preflight/recount "
                 "budgets passed.\n";
}
