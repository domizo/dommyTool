#include "field_api_doubles.h"

class DCurlNoiseField {
  public:
    double rampValue = 1.0;
    MObject thisMObject() const {
        return MObject(100);
    }
    double falloffCurve(double ratio, MStatus* status) {
        assert(ratio >= 0 && ratio < 1);
        operation("field.falloff", status);
        return rampValue;
    }
    MStatus calculateForces(MDataBlock&, const MVectorArray&, const MDoubleArray&, MVectorArray&);
    MStatus ownerPosition(MDataBlock&, MVectorArray&);
    MStatus getWorldPosition(MDataBlock&, MVector&);
    MStatus compute(const MPlug&, MDataBlock&);
};
#include "curl_field_methods.inc"

MDataBlock defaults() {
    deferFieldPublication = true;
    resetCalls();
    objects.clear();
    world = MMatrix();
    published = MObject();
    ageExists = true;
    wrongAgeType = false;
    MDataBlock block;
    block.values = {{mMagnitude, 2},           {curlSpeed, .3},
                    {curlAmplitude, 5},        {curlAgeSpeed, 1},
                    {noiseFrequencyX, .4},     {noiseFrequencyY, .4},
                    {noiseFrequencyZ, .4},     {phaseRateX, .4},
                    {phaseRateY, .4},          {phaseRateZ, .4},
                    {positionNoiseScaleX, .5}, {positionNoiseScaleY, .5},
                    {positionNoiseScaleZ, .5}, {positionNoiseFrequency, 2},
                    {mMaxDistance, 100}};
    for (int attr : {mInputPositions, mInputVelocities, mInputMass, mInputPPData, mOwnerPosData}) {
        MObject object(attr);
        object.payload.reset(new Payload);
        objects[attr] = object;
    }
    objects[mInputPositions].payload->vectors.values = {MVector(.125, .25, .375)};
    objects[mInputVelocities].payload->vectors.values = {MVector::zero};
    objects[mInputMass].payload->doubles.values = {1.0};
    objects[mInputPPData].payload->doubles.values = {.3};
    return block;
}
bool near(double a, double b, double tolerance = 1e-10) {
    return std::abs(a - b) <= tolerance * std::max(1.0, std::max(std::abs(a), std::abs(b)));
}
void assertVector(const MVector& a, const MVector& b, double scale = 1.0) {
    for (unsigned int axis = 0; axis < 3; ++axis)
        assert(near(a[axis], b[axis] * scale));
}
MVector evaluate(DCurlNoiseField& field, MDataBlock& block) {
    resetCalls();
    assert(field.compute(MPlug(mOutputForce), block) == MS::kSuccess);
    assert(publications == 1 && cleanCalls == 1 && invalidReads == 0 && lastOutputIndex == 5);
    assert(published.payload->vectors.length() ==
           objects[mInputPositions].payload->vectors.length());
    return published.payload->vectors.length() ? published.payload->vectors[0] : MVector::zero;
}

void checkMathAndModes() {
    DCurlNoiseField field;
    MDataBlock block = defaults();
    const MVector noMax = evaluate(field, block);
    // Baseline from the historical no-max 3D kernel (old intermediates were floats).
    assert(near(noMax.x, 3.62095793088277, 1e-6));
    assert(near(noMax.y, 2.01793173948924, 1e-6));
    assert(near(noMax.z, 3.54673266410828, 1e-6));
    block.values[mUseMaxDistance] = 1;
    assertVector(evaluate(field, block), noMax);
    block.values[mAttenuation] = 2;
    field.rampValue = .25;
    const double distance = std::hypot(std::hypot(.125, .25), .375);
    assertVector(evaluate(field, block), noMax, .25 * std::pow(1 - distance / 100, 2));
    block.values[mMaxDistance] = distance;
    assertVector(evaluate(field, block), MVector::zero);
    block.values[mMaxDistance] = 0;
    assertVector(evaluate(field, block), MVector::zero);
    field.rampValue = 1;
    block = defaults();
    for (int octave : {1, 32}) {
        block.values[positionNoiseFrequency] = octave;
        assert(dommy::maya::isFiniteVector(evaluate(field, block)));
        block.values[mUseMaxDistance] = 1;
        const MVector on = evaluate(field, block);
        block.values[mUseMaxDistance] = 0;
        assertVector(evaluate(field, block), on);
    }
    // Every formerly ignored maximum-distance control must change the signal.
    for (int attr : {curlAmplitude, curlAgeSpeed, positionNoiseFrequency, positionNoiseScaleX}) {
        block = defaults();
        block.values[mUseMaxDistance] = 1;
        block.values[phaseRateY] = .7;
        block.values[phaseRateZ] = 1.1;
        const MVector original = evaluate(field, block);
        block.values[attr] += .75;
        if (attr == positionNoiseFrequency)
            block.values[attr] = 4;
        const MVector changed = evaluate(field, block);
        assert(!near(original.x, changed.x) || !near(original.y, changed.y) ||
               !near(original.z, changed.z));
    }
    MVector direction(7, 8, 9);
    assert(sampleNoiseDirection(noise::PerlinOctave(2, 1, 2, 2), MVector::zero, direction));
    assertVector(direction, MVector::zero);
    direction = MVector(7, 8, 9);
    assert(!sampleNoiseDirection(noise::PerlinOctave(33, 1, 2, 2), MVector::zero, direction));
    assertVector(direction, MVector(7, 8, 9));
    const double infinity = std::numeric_limits<double>::infinity();
    assert(
        !sampleNoiseDirection(noise::PerlinOctave(2, 1, 2, 2), MVector(infinity, 0, 0), direction));
    const std::size_t limit = dommy::generation::kMaxWorkItemCount;
    assert(curlWorkWithinLimit(limit / (21U * 32U), 1, 32));
    assert(!curlWorkWithinLimit(limit / (21U * 32U) + 1U, 1, 32));
    assert(!curlWorkWithinLimit(1, 0, 2));
    assert(!curlWorkWithinLimit(1, std::numeric_limits<std::size_t>::max(), 2));
    block = defaults();
    block.values[mApplyPerVertex] = 1;
    objects[mOwnerPosData].payload->vectors.values = {MVector::zero, MVector::zero};
    assertVector(evaluate(field, block), noMax, 2.0);
    block.values[mUseMaxDistance] = 1;
    assertVector(evaluate(field, block), noMax, 2.0);
    objects[mInputPositions].payload->vectors.values.push_back(MVector(.125, .25, .375));
    objects[mInputVelocities].payload->vectors.values.push_back(MVector::zero);
    objects[mInputPPData].payload->doubles.values.push_back(.3);
    evaluate(field, block);
    assertVector(published.payload->vectors[1], noMax, 2.0);
}

void checkFailurePropagation() {
    DCurlNoiseField field;
    std::size_t failureCases = 0;
    // Centroid, per-vertex owner, and empty-owner/world-matrix routes.
    for (int ownerMode = 0; ownerMode < 3; ++ownerMode) {
        MDataBlock block = defaults();
        block.values[mUseMaxDistance] = 1;
        block.values[mApplyPerVertex] = ownerMode != 0;
        if (ownerMode == 1)
            objects[mOwnerPosData].payload->vectors.values = {MVector::zero};
        evaluate(field, block);
        const std::vector<std::string> baseline = operations;
        const auto publication =
            std::find(baseline.begin(), baseline.end(), "array.setBuilder") - baseline.begin();
        for (std::size_t failure = 0; failure < baseline.size(); ++failure) {
            published = MObject();
            resetCalls(static_cast<int>(failure));
            assert(field.compute(MPlug(mOutputForce), block) == MS::kFailure);
            assert(invalidReads == 0 && cleanCalls == 0);
            if (failure <= static_cast<std::size_t>(publication))
                assert(publications == 0);
            ++failureCases;
        }
        resetCalls();
    }
    std::cout << "Curl field: " << failureCases << " injected API failure boundaries passed\n";
    MDataBlock block = defaults();
    for (int component : {noiseFrequencyX, noiseFrequencyY, noiseFrequencyZ}) {
        resetCalls();
        block.values[component] = std::numeric_limits<double>::quiet_NaN();
        MVector sentinel(7, 8, 9);
        assert(readFiniteVector(block, noiseFrequencyX, noiseFrequencyY, noiseFrequencyZ,
                                sentinel) == MS::kInvalidParameter);
        assertVector(sentinel, MVector(7, 8, 9));
        block.values[component] = .4;
    }
    // Calculation errors leave an existing caller-owned output unchanged.
    MVectorArray output;
    output.values = {MVector(7, 8, 9)};
    MVectorArray points;
    points.values = {MVector(.1, .2, .3), MVector(std::numeric_limits<double>::infinity(), 0, 0)};
    MDoubleArray ages;
    ages.values = {0, 0};
    resetCalls();
    assert(field.calculateForces(block, points, ages, output) == MS::kInvalidParameter);
    assert(output.length() == 1);
    assertVector(output[0], MVector(7, 8, 9));
}

void checkInputContracts() {
    DCurlNoiseField field;
    const double infinity = std::numeric_limits<double>::infinity();
    for (const auto& invalid : std::vector<std::pair<int, double>>{{positionNoiseFrequency, 0},
                                                                   {positionNoiseFrequency, 33},
                                                                   {positionNoiseFrequency, 64},
                                                                   {curlSpeed, 0},
                                                                   {curlSpeed, 1e-7},
                                                                   {curlAmplitude, infinity},
                                                                   {mMagnitude, infinity},
                                                                   {mMaxDistance, -1},
                                                                   {mAttenuation, -1},
                                                                   {mOwnerCentroidY, infinity}}) {
        MDataBlock block = defaults();
        block.values[mUseMaxDistance] = 1;
        block.values[invalid.first] = invalid.second;
        resetCalls();
        assert(field.compute(MPlug(mOutputForce), block) == MS::kInvalidParameter);
        assert(publications == 0 && cleanCalls == 0 && invalidReads == 0);
    }
    MDataBlock block = defaults();
    // Preserve missing/short/long/nonfinite age-array zero-pad/truncate policy.
    objects[mInputPPData].payload->doubles.values = {0};
    const MVector zeroAge = evaluate(field, block);
    ageExists = false;
    assertVector(evaluate(field, block), zeroAge);
    ageExists = true;
    objects[mInputPPData].payload->doubles.values.clear();
    assertVector(evaluate(field, block), zeroAge);
    objects[mInputPPData].payload->doubles.values = {infinity, 9};
    assertVector(evaluate(field, block), zeroAge);
    wrongAgeType = true;
    resetCalls();
    assert(field.compute(MPlug(mOutputForce), block) == MS::kInvalidParameter && publications == 0);
    block = defaults();
    objects[mInputVelocities].payload->vectors.values.clear();
    resetCalls();
    assert(field.compute(MPlug(mOutputForce), block) == MS::kInvalidParameter && publications == 0);
    block = defaults();
    block.values[mApplyPerVertex] = 1;
    objects[mOwnerPosData] = MObject();
    assert(dommy::maya::isFiniteVector(evaluate(field, block)));
    world.values[0][0] = infinity;
    resetCalls();
    assert(field.compute(MPlug(mOutputForce), block) == MS::kInvalidParameter && publications == 0);
    block = defaults();
    block.values[mUseMaxDistance] = 1;
    field.rampValue = infinity;
    resetCalls();
    assert(field.compute(MPlug(mOutputForce), block) == MS::kInvalidParameter && publications == 0);
    field.rampValue = 1;
    block = defaults();
    block.values[curlAgeSpeed] = std::numeric_limits<double>::max();
    objects[mInputPPData].payload->doubles.values = {std::numeric_limits<double>::max()};
    resetCalls();
    assert(field.compute(MPlug(mOutputForce), block) == MS::kInvalidParameter && publications == 0);
    block = defaults();
    objects[mInputPositions].payload->vectors.values.clear();
    objects[mInputVelocities].payload->vectors.values.clear();
    evaluate(field, block);
    resetCalls();
    assert(field.compute(MPlug(mInputData), block) == MS::kUnknownParameter && operations.empty());
}

int main() {
    checkMathAndModes();
    checkFailurePropagation();
    checkInputContracts();
    std::cout << "Curl field: common 3D curl, distance falloff, octaves, budgets, and staged "
                 "output passed\n";
}
