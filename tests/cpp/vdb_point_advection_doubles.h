#pragma once
#include "common/utility/DCurveBatchBudget.h"
// Reuse the explicit grid/value/input boundaries without changing other suites.
#define MDataBlock TransportDataBlock
#define MPlug TransportPlug
#include "vdb_transport_doubles.h"
#undef MDataBlock
#undef MPlug
#include "common/utility/DPointPayloadBudget.h"
static int pointCopies = 0, outputCreates = 0, curveBatches = 0, nativeSamples = 0;
static int nativeSteps = 0, projectionSteps = 0, cleanCalls = 0;
static int corruptStep = -1, corruptProjection = -1;
static double corruptValue = 0;
static bool inWorker = false, invalidCurve = false, missingOutput = false;
static int lastOrder = 0;
static bool lastStaggered = false;
using DPoint = Imath::V3f;
using DPointList = std::vector<DPoint>;
struct Group {
    std::vector<bool> ids;
    const std::vector<bool>& getGroupIds() const {
        return ids;
    }
};
struct DPData : MPxData {
    static const MTypeId id = 22;
    DPointList points;
    DPAttrList attributes;
    std::map<std::string, std::shared_ptr<Group>> groups;
    std::size_t reportedCount = 0;
    bool valid = true;
    std::size_t numberOfPoints() const {
        return reportedCount ? reportedCount : points.size();
    }
    ConstDPAttrIter beginAttr() const {
        return attributes.begin();
    }
    ConstDPAttrIter endAttr() const {
        return attributes.end();
    }
    auto beginGrp() const -> decltype(groups.begin()) {
        return groups.begin();
    }
    auto endGrp() const -> decltype(groups.end()) {
        return groups.end();
    }
    bool hasValidPointTopology() const {
        dommy::attribute::ValidationBudget budget;
        if (!valid ||
            !dommy::attribute::validateAttributeList(attributes, numberOfPoints(), budget))
            return false;
        for (const auto& point : points)
            if (!dommy::attribute::isValidValue(point))
                return false;
        for (const auto& group : groups)
            if (!group.second || group.second->ids.size() != numberOfPoints())
                return false;
        return true;
    }
    const DPointList* getPositionPtrConst() const {
        return &points;
    }
    void setDPData(const DPData& source) {
        nativeCall("point.copy");
        ++pointCopies;
        *this = source;
        for (auto& field : attributes)
            field.second = field.second->copy();
        for (auto& group : groups)
            group.second = std::make_shared<Group>(*group.second);
    }
    bool replacePositions(const DPointList& values) {
        const auto status = operation("point.replace");
        if (status)
            points = values;
        return bool(status);
    }
    void updateBBox() {
        nativeCall("point.bounds");
    }
};
struct MPoint {
    double x, y, z, w;
    MPoint(double a = 0, double b = 0, double c = 0) : x(a), y(b), z(c), w(1) {}
};
struct MPointArray {
    std::vector<MPoint> values;
    MStatus append(const MPoint& value) {
        assert(!inWorker);
        const MStatus status = operation("curve.append");
        if (status)
            values.push_back(value);
        return status;
    }
    unsigned length() const {
        return static_cast<unsigned>(values.size());
    }
};
struct MDoubleArray {
    std::vector<double> values;
};
inline void GenerateDCurveKnots(std::size_t count, int degree, MDoubleArray& knots) {
    assert(!inWorker);
    nativeCall("curve.knots");
    knots.values.resize(count + degree - 1U);
}
struct DCurveInternalData {
    MPointArray points;
    int degree;
    DCurveInternalData(const MPointArray& p, const MDoubleArray&, int d) : points(p), degree(d) {
        assert(!inWorker);
        nativeCall("curve.create");
    }
    bool hasValidCurveTopology() const {
        return !invalidCurve && points.length() >= 2U;
    }
    std::size_t numberOfPoints() const {
        return points.length();
    }
    bool appendOperationCost(dommy::curve::CurveBatchBudget& budget) const {
        return hasValidCurveTopology() && budget.append(points.length(),points.length()+degree-1U,0U,0U);
    }
};
using DCItem = std::shared_ptr<DCurveInternalData>;
struct DCurveData : MPxData {
    static const MTypeId id = 23;
    using DCurveList = std::vector<DCItem>;
    DCurveList curves;
    bool insert(const DCurveList& values) {
        assert(!inWorker);
        ++curveBatches;
        const MStatus status = operation("curve.batch");
        if (status)
            curves = values;
        return bool(status);
    }
    bool hasValidCurveTopology() const {
        return !invalidCurve && std::all_of(curves.begin(), curves.end(), [](const DCItem& c) {
            return c && c->hasValidCurveTopology();
        });
    }
    void updateBBox() {
        nativeCall("curve.bounds");
    }
};
static DPData pointInput, pointStage, pointOutput;
const MTypeId DPData::id;
const MTypeId DCurveData::id;
static DCurveData curveStage, curveOutput;
struct MPlug : TransportPlug {
    using TransportPlug::TransportPlug;
    bool operator==(const MObject& other) const {
        return object.name == other.name;
    }
};
struct MDataBlock : TransportDataBlock {
    std::string requestedOutput, cleanedOutput;
    MDataHandle outputValue(const MObject& value, MStatus* status) {
        *status = operation("output.handle");
        requestedOutput = value.name;
        return {InputValue(), bool(*status)};
    }
    MStatus setClean(const MPlug& plug) {
        const MStatus status = operation("output.clean");
        if (status) {
            ++cleanCalls;
            cleanedOutput = plug.object.name;
        }
        return status;
    }
};
namespace dommy {
namespace maya {
template <typename T> MStatus createPluginDataObject(const MTypeId&, MObject& object, T*& result) {
    ++outputCreates;
    const MStatus status = operation("output.create");
    object = {"output"};
    result = missingOutput
                 ? nullptr
                 : dynamic_cast<T*>(T::id == DPData::id ? static_cast<MPxData*>(&pointStage)
                                                        : static_cast<MPxData*>(&curveStage));
    return status;
}
template <typename T>
MStatus publishPluginDataObject(MDataHandle&, const MObject&, const T& value) {
    const MStatus status = operation("output.set");
    if (status) {
        ++publications;
        if (const DPData* points = dynamic_cast<const DPData*>(&value))
            pointOutput = *points;
        if (const DCurveData* curves = dynamic_cast<const DCurveData*>(&value))
            curveOutput = *curves;
    }
    return status;
}
} // namespace maya
} // namespace dommy
namespace tbb {
template <typename T> struct blocked_range {
    T lo, hi;
    blocked_range(T a, T b, std::size_t) : lo(a), hi(b) {}
    T begin() const {
        return lo;
    }
    T end() const {
        return hi;
    }
};
// Serial boundary: does NOT claim to test native TBB scheduling or races.
template <typename R, typename Op> void parallel_for(const R& range, const Op& op) {
    nativeCall("parallel.run");
    inWorker = true;
    try {
        op(range);
    } catch (...) {
        inWorker = false;
        throw;
    }
    inWorker = false;
}
} // namespace tbb
namespace openvdb {
namespace tools {
template <typename G, typename V> void inspectPoint(const G& grid, const V& p) {
    ++nativeSamples;
    Vec3R world(p[0], p[1], p[2]);
    const auto index = grid.transform().worldToIndex(world);
    // Reproduce the pinned floor -> int boundary, not sparse interpolation.
    for (int axis = 0; axis < 3; ++axis) {
        volatile int cell = int(std::floor(index[axis]));
        (void)cell;
    }
}
template <typename G, bool Staggered> struct VelocityIntegrator {
    const G& grid;
    explicit VelocityIntegrator(const G& g) : grid(g) {
        nativeCall("integrator.create");
    }
    template <int Order, typename V> void rungeKutta(typename V::ValueType dt, V& p) {
        nativeCall("integrator.step");
        ++nativeSteps;
        lastOrder = Order;
        lastStaggered = Staggered;
        inspectPoint(grid, p);
        // Constant-field boundary, deliberately not a numerical RK implementation.
        for (int axis = 0; axis < 3; ++axis)
            p[axis] += dt * grid.bg[axis];
        if (nativeSteps == corruptStep)
            p[0] = typename V::ValueType(corruptValue);
    }
};
template <typename G> struct ClosestPointProjector {
    const G& grid;
    ClosestPointProjector(const G& g, int iterations) : grid(g) {
        assert(iterations == 1);
        nativeCall("projector.create");
    }
    template <typename V> void projectToConstraintSurface(V& p) {
        nativeCall("projector.step");
        ++projectionSteps;
        inspectPoint(grid, p);
        p = grid.bg;
        if (projectionSteps == corruptProjection)
            p[0] = typename V::ValueType(corruptValue);
    }
};
} // namespace tools
} // namespace openvdb
