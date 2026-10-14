#include "field_api_doubles.h"
#include "common/utility/DDirectionMath.h"
#include "modules/particles/particle/field/DCurveFieldMath.h"
#include <utility>

// A straight curve and affine ramps provide deterministic host query results.
// Real NURBS closest-point, world-space, and Maya ramp behavior need Maya.
namespace MSpace {
enum Space { kWorld };
}
double curveStart = 0.0, curveEnd = 2.0;
bool invalidClosest = false, invalidParameter = false;
MVector curveTangent(1, 0, 0);
std::map<int, std::pair<double, double>> rampValues;
double rampAt(int attribute, double position) {
    return rampValues.at(attribute).first + position * rampValues.at(attribute).second;
}
struct MFnNurbsCurve {
    MFnNurbsCurve(const MObject&, MStatus* status) {
        operation("curve.bind", status);
    }
    MStatus getKnotDomain(double& start, double& end) {
        const MStatus status = operation("curve.domain");
        if (status == MS::kSuccess) {
            start = curveStart;
            end = curveEnd;
        }
        return status;
    }
    MPoint closestPoint(const MPoint& point, double* parameter, double, MSpace::Space,
                        MStatus* status) {
        // On failure, deliberately leave *parameter untouched and return a
        // finite default point: the production path must check MStatus first.
        if (!operation("curve.closest", status))
            return MPoint();
        const double closestParameter = std::max(curveStart, std::min(curveEnd, point.x));
        *parameter = invalidParameter ? std::numeric_limits<double>::quiet_NaN() : closestParameter;
        return MPoint(invalidClosest ? std::numeric_limits<double>::infinity() : closestParameter,
                      0, 0);
    }
    MVector tangent(double, MSpace::Space, MStatus* status) {
        operation("curve.tangent", status);
        return curveTangent;
    }
};
struct MRampAttribute {
    int attribute;
    MRampAttribute(const MPlug& plug, MStatus* status) : attribute(plug.id) {
        operation("ramp.bind." + std::to_string(attribute), status);
    }
    void getValueAtPosition(float position, float& value, MStatus* status) {
        assert(std::isfinite(position) && position >= 0.0f && position <= 1.0f);
        if (operation("ramp.sample." + std::to_string(attribute), status))
            value = static_cast<float>(rampAt(attribute, position));
    }
};
struct MPxFieldNode {
    static const int mOutputForce = ::mOutputForce;
};
class DCurveField {
  public:
    double falloffValue = 1.0;
    MObject thisMObject() const {
        return MObject(100);
    }
    double falloffCurve(double ratio, MStatus* status) {
        assert(std::isfinite(ratio) && ratio >= 0 && ratio <= 1);
        operation("field.falloff", status);
        return falloffValue;
    }
    MStatus compute(const MPlug&, MDataBlock&);
    MStatus calculateForces(MDataBlock&, const MVectorArray&, const MVectorArray&,
                            const MDoubleArray&, MVectorArray&);
};
#include "curve_field_methods.inc"

MDataBlock defaults() {
    resetCalls();
    objects.clear();
    published = MObject();
    curveStart = 0.0;
    curveEnd = 2.0;
    invalidClosest = invalidParameter = false;
    curveTangent = MVector(1, 0, 0);
    rampValues = {{aRadiusRamp, {1, 0}},
                  {aForwardRamp, {1, 0}},
                  {aToCurveRamp, {1, 0}},
                  {aBlendRamp, {1, 0}}};
    for (int attribute : {mInputPositions, mInputVelocities, mInputMass, inputTargetCurve}) {
        MObject object(attribute);
        object.payload.reset(new Payload);
        objects[attribute] = object;
    }
    objects[inputTargetCurve].kind = MFnData::kNurbsCurve;
    objects[mInputPositions].payload->vectors.values = {MVector(1, 1, 0)};
    objects[mInputVelocities].payload->vectors.values = {MVector(.2, -.3, .5)};
    objects[mInputMass].payload->doubles.values = {2};
    MDataBlock block;
    block.values = {{mMagnitude, 2},   {tangentAffectDistance, 3}, {aDamping, .5},
                    {aAttraction, .5}, {aForwardScale, 1},         {aToCurveScale, 1},
                    {aBlend, .5},      {mMaxDistance, 10},         {mAttenuation, 0}};
    return block;
}
bool near(double a, double b) {
    return std::abs(a - b) <= 2e-7 * std::max(1.0, std::max(std::abs(a), std::abs(b)));
}
void assertVector(const MVector& a, const MVector& b) {
    for (unsigned int axis = 0; axis < 3; ++axis)
        assert(near(a[axis], b[axis]));
}
MVector unit(const MVector& vector) {
    const double length = std::hypot(std::hypot(vector.x, vector.y), vector.z);
    return length > 1e-12 ? vector / length : MVector::zero;
}
// Independent ordinary-input reference of the historical force formula.
MVector expected(MDataBlock& block, unsigned int index, double falloff) {
    const MVector point = objects[mInputPositions].payload->vectors[index];
    const MVector velocity = objects[mInputVelocities].payload->vectors[index];
    const MDoubleArray& masses = objects[mInputMass].payload->doubles;
    const double mass = masses[masses.length() == 1U ? 0U : index];
    const double parameter = std::max(curveStart, std::min(curveEnd, point.x));
    const float rampPosition =
        static_cast<float>((parameter - curveStart) / (curveEnd - curveStart));
    const MVector delta = MVector(parameter, 0, 0) - point;
    const double distance = std::hypot(std::hypot(delta.x, delta.y), delta.z);
    const MVector tangent =
        unit(curveTangent) * static_cast<float>(rampAt(aForwardRamp, rampPosition));
    const MVector direction = unit(delta) * static_cast<float>(rampAt(aToCurveRamp, rampPosition));
    double magnitude = block.values[mMagnitude];
    if (block.values[mUseMaxDistance]) {
        const double radius = block.values[mMaxDistance];
        if (radius <= 1e-12 || distance > radius)
            return MVector::zero;
        if (block.values[mAttenuation] > 0)
            magnitude *=
                std::pow(std::max(0.0, 1.0 - distance / radius), block.values[mAttenuation]);
        magnitude *= falloff;
    }
    const MVector drag = velocity * block.values[aDamping];
    const MVector toward =
        direction * block.values[aToCurveScale] * block.values[aAttraction] - drag;
    MVector force;
    if (block.values[aIsAffectTip] && std::abs(curveEnd - parameter) <= .1) {
        force = tangent * block.values[aForwardScale] - drag;
    } else {
        const double inner =
            std::max(0.0, block.values[tangentAffectDistance] *
                              static_cast<float>(rampAt(aRadiusRamp, rampPosition)));
        if (inner > 1e-12 && distance < inner) {
            double weight = distance / inner;
            if (block.values[aInnerMethod] == 1)
                weight = block.values[aBlend] *
                         static_cast<float>(rampAt(aBlendRamp, static_cast<float>(weight)));
            force = tangent * block.values[aForwardScale] + toward * weight;
        } else
            force = toward;
    }
    return force * magnitude * mass;
}
void evaluate(DCurveField& field, MDataBlock& block) {
    resetCalls();
    assert(field.compute(MPlug(mOutputForce), block) == MS::kSuccess);
    assert(invalidReads == 0 && publications == 1 && cleanCalls == 1 && lastOutputIndex == 5);
    assert(published.payload->vectors.length() ==
           objects[mInputPositions].payload->vectors.length());
}

void checkOrdinaryCompatibility() {
    DCurveField field;
    unsigned int cases = 0;
    for (int maxMode : {0, 1})
        for (int method : {0, 1})
            for (int tip : {0, 1}) {
                for (const MVector& point :
                     {MVector(1, 1, 0), MVector(1, 4, 0), MVector(1.95, .2, 0)}) {
                    MDataBlock block = defaults();
                    block.values[mUseMaxDistance] = maxMode;
                    block.values[aInnerMethod] = method;
                    block.values[aIsAffectTip] = tip;
                    block.values[mAttenuation] = 1.5;
                    block.values[aForwardScale] = -1.25;
                    block.values[aToCurveScale] = 2.5;
                    block.values[aBlend] = .35;
                    field.falloffValue = .6;
                    objects[mInputPositions].payload->vectors.values = {point,
                                                                        MVector(.25, .75, .5)};
                    objects[mInputVelocities].payload->vectors.values.push_back(
                        MVector(-.4, .2, .1));
                    objects[mInputMass].payload->doubles.values = {2, 3};
                    rampValues = {{aRadiusRamp, {.8, .2}},
                                  {aForwardRamp, {.5, .5}},
                                  {aToCurveRamp, {1, -.25}},
                                  {aBlendRamp, {.3, .7}}};
                    const MVector first = expected(block, 0, field.falloffValue);
                    const MVector second = expected(block, 1, field.falloffValue);
                    evaluate(field, block);
                    assertVector(published.payload->vectors[0], first);
                    assertVector(published.payload->vectors[1], second);
                    ++cases;
                }
            }
    MDataBlock block = defaults();
    field.falloffValue = 1;
    evaluate(field, block);
    const MVector noMax = published.payload->vectors[0];
    block.values[mUseMaxDistance] = 1;
    evaluate(field, block);
    assertVector(published.payload->vectors[0], noMax);
    block.values[mMaxDistance] = 1; // inclusive outer boundary
    evaluate(field, block);
    assertVector(published.payload->vectors[0], noMax);
    block.values[mMaxDistance] = .99;
    evaluate(field, block);
    assertVector(published.payload->vectors[0], MVector::zero);
    for (double radius : {0.0, 1e-13}) {
        block.values[mMaxDistance] = radius;
        evaluate(field, block);
        assertVector(published.payload->vectors[0], MVector::zero);
    }
    for (double magnitude : {0.0, -2.0}) {
        block = defaults();
        block.values[mMagnitude] = magnitude;
        const MVector reference = expected(block, 0, 1);
        evaluate(field, block);
        assertVector(published.payload->vectors[0], reference);
    }
    block = defaults();
    curveTangent = MVector::zero;
    evaluate(field, block);
    assertVector(published.payload->vectors[0], expected(block, 0, 1));
    block = defaults();
    objects[mInputPositions].payload->vectors[0].y = 1e200;
    evaluate(field, block);
    assertVector(published.payload->vectors[0], expected(block, 0, 1));
    block = defaults();
    curveTangent = MVector(1e200, 1e200, 0);
    evaluate(field, block);
    assertVector(published.payload->vectors[0], expected(block, 0, 1));
    block = defaults();
    objects[mInputPositions].payload->vectors[0].y = 0;
    block.values[aForwardScale] = 1e200;
    block.values[mMagnitude] = 1e200;
    objects[mInputMass].payload->doubles.values = {1e-200};
    evaluate(field, block);
    assertVector(published.payload->vectors[0], MVector(1e200, 0, 0));
    std::cout << "Curve field: " << cases << " two-receptor legacy formula cases passed\n";
}

void checkFailureBoundaries() {
    DCurveField field;
    unsigned int injected = 0;
    for (int maxMode : {0, 1})
        for (int method : {0, 1}) {
            MDataBlock block = defaults();
            block.values[mUseMaxDistance] = maxMode;
            block.values[aInnerMethod] = method;
            objects[mInputPositions].payload->vectors.values.push_back(MVector(.5, 1.5, .2));
            objects[mInputVelocities].payload->vectors.values.push_back(MVector::zero);
            evaluate(field, block);
            const std::vector<std::string> baseline = operations;
            const auto publication =
                std::find(baseline.begin(), baseline.end(), "output.set") - baseline.begin();
            for (std::size_t failure = 0; failure < baseline.size(); ++failure) {
                published = MObject();
                resetCalls(static_cast<int>(failure));
                assert(field.compute(MPlug(mOutputForce), block) == MS::kFailure);
                assert(invalidReads == 0 && cleanCalls == 0);
                if (failure <= static_cast<std::size_t>(publication))
                    assert(publications == 0);
                ++injected;
            }
            resetCalls();
            MVectorArray output;
            const auto& points = objects[mInputPositions].payload->vectors;
            const auto& velocities = objects[mInputVelocities].payload->vectors;
            const auto& masses = objects[mInputMass].payload->doubles;
            assert(field.calculateForces(block, points, velocities, masses, output) ==
                   MS::kSuccess);
            const auto firstQuery =
                std::find(operations.begin(), operations.end(), "curve.closest");
            const auto secondQuery = std::find(firstQuery + 1, operations.end(), "curve.closest");
            assert(secondQuery != operations.end());
            const int failure = static_cast<int>(secondQuery - operations.begin());
            output.values = {MVector(7, 8, 9)};
            resetCalls(failure);
            assert(field.calculateForces(block, points, velocities, masses, output) ==
                   MS::kFailure);
            assert(output.length() == 1);
            assertVector(output[0], MVector(7, 8, 9));
        }
    for (int noOpMode = 0; noOpMode < 3; ++noOpMode) {
        MDataBlock block = defaults();
        if (noOpMode == 0)
            objects[inputTargetCurve] = MObject();
        if (noOpMode == 1) {
            block.values[mUseMaxDistance] = 1;
            block.values[mMaxDistance] = 0;
        }
        if (noOpMode == 2) {
            objects[mInputPositions].payload->vectors.values.clear();
            objects[mInputVelocities].payload->vectors.values.clear();
        }
        evaluate(field, block);
        const std::vector<std::string> baseline = operations;
        for (std::size_t failure = 0; failure < baseline.size(); ++failure) {
            resetCalls(static_cast<int>(failure));
            assert(field.compute(MPlug(mOutputForce), block) == MS::kFailure);
            assert(invalidReads == 0 && cleanCalls == 0);
            if (baseline[failure] != "block.setClean")
                assert(publications == 0);
            ++injected;
        }
    }
    std::cout << "Curve field: " << injected << " injected API failures passed\n";
}

void assertInvalid(DCurveField& field, MDataBlock& block) {
    resetCalls();
    assert(field.compute(MPlug(mOutputForce), block) == MS::kInvalidParameter);
    assert(publications == 0 && cleanCalls == 0 && invalidReads == 0);
}
void checkInvalidAndEmptyInputs() {
    DCurveField field;
    const double infinity = std::numeric_limits<double>::infinity();
    for (int attribute : {mMagnitude, tangentAffectDistance, aDamping, aAttraction, aForwardScale,
                          aToCurveScale, aBlend, mMaxDistance, mAttenuation}) {
        MDataBlock block = defaults();
        block.values[mUseMaxDistance] = 1;
        block.values[attribute] = infinity;
        assertInvalid(field, block);
    }
    for (int attribute : {tangentAffectDistance, mMaxDistance, mAttenuation, aInnerMethod}) {
        MDataBlock block = defaults();
        block.values[mUseMaxDistance] = 1;
        block.values[attribute] = -1;
        assertInvalid(field, block);
    }
    MDataBlock block = defaults();
    block.values[aInnerMethod] = 2;
    assertInvalid(field, block);
    block = defaults();
    invalidClosest = true;
    assertInvalid(field, block);
    block = defaults();
    invalidParameter = true;
    assertInvalid(field, block);
    block = defaults();
    curveTangent.x = infinity;
    assertInvalid(field, block);
    block = defaults();
    curveStart = -std::numeric_limits<double>::max();
    curveEnd = std::numeric_limits<double>::max();
    evaluate(field, block); // Wide finite domains normalize to a finite ramp position.
    assert(std::isfinite(published.payload->vectors[0].x));
    block = defaults();
    curveEnd = curveStart;
    assertInvalid(field, block);
    for (int attribute : {aRadiusRamp, aForwardRamp, aToCurveRamp, aBlendRamp}) {
        block = defaults();
        block.values[aInnerMethod] = 1;
        rampValues[attribute].first = infinity;
        assertInvalid(field, block);
    }
    block = defaults();
    block.values[mUseMaxDistance] = 1;
    field.falloffValue = infinity;
    assertInvalid(field, block);
    field.falloffValue = 1;
    block = defaults();
    objects[mInputPositions].payload->vectors[0].y = infinity;
    assertInvalid(field, block);
    block = defaults();
    objects[mInputVelocities].payload->vectors[0].x = infinity;
    assertInvalid(field, block);
    block = defaults();
    objects[mInputMass].payload->doubles[0] = infinity;
    assertInvalid(field, block);
    block = defaults();
    objects[mInputMass].payload->doubles.values.clear();
    assertInvalid(field, block);
    block = defaults();
    objects[mInputVelocities].payload->vectors.values.clear();
    assertInvalid(field, block);
    block = defaults();
    objects[inputTargetCurve].kind = MFnData::kInvalid;
    assertInvalid(field, block);
    block = defaults();
    block.values[mMagnitude] = std::numeric_limits<double>::max();
    objects[mInputPositions].payload->vectors[0].y = 0;
    assertInvalid(field, block);
    block = defaults();
    objects[inputTargetCurve] = MObject();
    evaluate(field, block);
    assertVector(published.payload->vectors[0], MVector::zero);
    block = defaults();
    objects[mInputPositions].payload->vectors.values.clear();
    objects[mInputVelocities].payload->vectors.values.clear();
    objects[mInputMass].payload->doubles.values.clear();
    evaluate(field, block);
    assert(published.payload->vectors.length() == 0);
    resetCalls();
    assert(field.compute(MPlug(mInputData), block) == MS::kUnknownParameter && operations.empty());
}

int main() {
    checkOrdinaryCompatibility();
    checkFailureBoundaries();
    checkInvalidAndEmptyInputs();
    std::cout
        << "Curve field: checked queries, staged output, finite math, and sparse output passed\n";
}
