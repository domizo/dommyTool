#include "vdb_field_doubles.h"
#include "vdb_field.inc"

MDataBlock baseline()
{
    deferFieldPublication = true;
    resetCalls();
    objects.clear();
    indexScale = 1;
    indexOffset = MVector::zero;
    sampledValue = MVector(1, 2, 3);
    samples = 0;
    throwTransform = throwSampler = false;
    activeSamples = validPayload = gridsExist = true;
    gridType = DVdbGridType::kVec3f;
    const int attributes[] = {mInputPositions, mInputVelocities, mInputMass, DOpenVDBField::aVdbInput};
    for (int attr : attributes) {
        MObject object(attr);
        object.payload = std::make_shared<Payload>();
        objects[attr] = object;
    }
    objects[mInputPositions].payload->vectors.values = {MVector(1, 1, 1), MVector(2, 2, 2)};
    objects[mInputVelocities].payload->vectors.values = {MVector::zero, MVector::zero};
    objects[mInputMass].payload->doubles.values = {1};
    published = MObject(999);
    published.payload = std::make_shared<Payload>();
    published.payload->vectors.values = {MVector(99, 98, 97)};
    lastOutputIndex = 0;
    MDataBlock block;
    block.values[mMagnitude] = 2;
    block.strings[DOpenVDBField::aVdbSelectedGridNames] = "velocity";
    return block;
}

void checkForces(const MVector& expected, unsigned int count = 2)
{
    assert(publications == 1 && cleanCalls == 1 && lastOutputIndex == 17);
    assert(published.payload->vectors.length() == count);
    for (unsigned int i = 0; i < count; ++i) {
        for (unsigned int axis = 0; axis < 3; ++axis)
            assert(published.payload->vectors[i][axis] == expected[axis]);
    }
    assert(invalidReads == 0);
}

void failureInjection(bool zeroFallback)
{
    DOpenVDBField node;
    auto block = baseline();
    gridsExist = !zeroFallback;
    assert(node.compute(MPlug(mOutputForce, 17), block) == MS::kSuccess);
    const auto trace = operations;
    for (std::size_t i = 0; i < trace.size(); ++i) {
        block = baseline();
        gridsExist = !zeroFallback;
        resetCalls(static_cast<int>(i));
        const MStatus status = node.compute(MPlug(mOutputForce, 17), block);
        assert(status == MS::kFailure && invalidReads == 0 && cleanCalls == 0);
        assert(operations.size() == i + 1);
        if (trace[i] == "block.setClean") {
            assert(publications == 1); // Publication cannot roll back a native clean failure.
        } else {
            assert(publications == 0 && published.id == 999);
            assert(published.payload->vectors[0].x == 99);
        }
        block = baseline();
        assert(node.compute(MPlug(mOutputForce, 17), block) == MS::kSuccess);
        checkForces(MVector(2, 4, 6));
    }
}

int main()
{
    DOpenVDBField node;
    for (DVdbGridType type : {DVdbGridType::kVec3f, DVdbGridType::kVec3d}) {
        auto block = baseline();
        gridType = type;
        activeSamples = false; // Background values are sampled even for inactive voxels.
        assert(node.compute(MPlug(mOutputForce, 17), block) == MS::kSuccess);
        checkForces(MVector(2, 4, 6));
        assert(samples == 2 && selectedNames == "velocity");
        assert(std::find(operations.begin(), operations.end(), "block.inputArray") == operations.end());
    }
    failureInjection(false);
    failureInjection(true);
    // Exercise the internal status contract independently of compute's final
    // cardinality guard, with a plausible pre-existing array of the same size.
    auto fallbackBlock = baseline();
    gridsExist = false;
    MVectorArray fallback;
    fallback.values = {MVector(77, 77, 77), MVector(77, 77, 77)};
    assert(node.applyNoMaxDist(fallbackBlock, objects[mInputPositions].payload->vectors,
        objects[mInputVelocities].payload->vectors, objects[mInputMass].payload->doubles,
        fallback) == MS::kSuccess);
    const auto fallbackTrace = operations;
    for (std::size_t i = 0; i < fallbackTrace.size(); ++i) {
        if (fallbackTrace[i] != "array.setLength" && fallbackTrace[i] != "array.copy") continue;
        fallbackBlock = baseline();
        gridsExist = false;
        fallback.values = {MVector(77, 77, 77), MVector(77, 77, 77)};
        resetCalls(static_cast<int>(i));
        assert(node.applyNoMaxDist(fallbackBlock, objects[mInputPositions].payload->vectors,
            objects[mInputVelocities].payload->vectors, objects[mInputMass].payload->doubles,
            fallback) != MS::kSuccess);
        assert(fallback.length() == 2 && fallback[0].x == 77);
    }
    resetCalls();
    assert(setZeroForces(static_cast<unsigned int>(dommy::generation::kMaxGeneratedPointCount + 1),
                         fallback) == MS::kInvalidParameter && operations.empty());
    for (int mode = 0; mode < 5; ++mode) {
        auto block = baseline();
        if (mode == 0) gridsExist = false;
        if (mode == 1) objects[DOpenVDBField::aVdbInput].payload.reset();
        if (mode == 2) gridType = DVdbGridType::kOther;
        if (mode == 3) objects[mInputVelocities].payload->vectors.values.clear();
        if (mode == 4) block.values[mMagnitude] = std::numeric_limits<double>::infinity();
        assert(node.compute(MPlug(mOutputForce, 17), block) == MS::kSuccess);
        checkForces(MVector::zero);
        assert(samples == 0);
    }
    auto block = baseline();
    validPayload = false;
    assert(node.compute(MPlug(mOutputForce, 17), block) == MS::kInvalidParameter);
    assert(publications == 0 && cleanCalls == 0 && published.id == 999);
    for (int mode = 0; mode < 2; ++mode) {
        block = baseline();
        throwTransform = mode == 0;
        throwSampler = mode == 1;
        assert(node.compute(MPlug(mOutputForce, 17), block) == MS::kFailure);
        assert(publications == 0 && cleanCalls == 0 && published.id == 999);
    }
    block = baseline();
    sampledValue.x = std::numeric_limits<double>::infinity();
    assert(node.compute(MPlug(mOutputForce, 17), block) == MS::kSuccess);
    checkForces(MVector::zero);
    block = baseline();
    objects[mInputPositions].payload->vectors.values[0].x = std::numeric_limits<double>::quiet_NaN();
    assert(node.compute(MPlug(mOutputForce, 17), block) == MS::kSuccess);
    assert(samples == 1 && published.payload->vectors.length() == 2);
    assert(published.payload->vectors[0].x == 0 && published.payload->vectors[1].x == 2);
    // Coordinates after world-to-index, rather than just the world point,
    // must fit the floor/+1 trilinear stencil. Test every axis independently.
    const double invalidCoordinates[] = {2147483647.0,
        std::nextafter(-2147483648.0, -std::numeric_limits<double>::infinity()),
        1e100, std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN()};
    for (unsigned int axis = 0; axis < 3; ++axis) {
        for (double coordinate : invalidCoordinates) {
            block = baseline();
            objects[mInputPositions].payload->vectors.values = {MVector::zero, MVector::zero};
            indexOffset[axis] = coordinate;
            assert(node.compute(MPlug(mOutputForce, 17), block) == MS::kInvalidParameter);
            assert(samples == 0 && publications == 0 && cleanCalls == 0 && published.id == 999);
        }
        for (double coordinate : {-2147483648.0,
            std::nextafter(2147483647.0, 0.0), -1.75}) {
            block = baseline();
            objects[mInputPositions].payload->vectors.values = {MVector::zero, MVector::zero};
            indexOffset[axis] = coordinate;
            assert(node.compute(MPlug(mOutputForce, 17), block) == MS::kSuccess);
            checkForces(MVector(2, 4, 6));
            assert(samples == 2);
        }
    }
    for (double scale : {1e100, std::numeric_limits<double>::infinity()}) {
        block = baseline();
        indexScale = scale;
        assert(node.compute(MPlug(mOutputForce, 17), block) == MS::kInvalidParameter);
        assert(samples == 0 && publications == 0 && published.id == 999);
    }
    block = baseline();
    objects[mInputPositions].payload->vectors.values[1].x = 1e100;
    assert(node.compute(MPlug(mOutputForce, 17), block) == MS::kInvalidParameter);
    assert(samples == 1 && publications == 0 && cleanCalls == 0 && published.id == 999);
    block = baseline();
    assert(node.compute(MPlug(999), block) == MS::kUnknownParameter && operations.empty());
    block = baseline();
    objects[mInputPositions].payload->vectors.values.clear();
    objects[mInputVelocities].payload->vectors.values.clear();
    assert(node.compute(MPlug(mOutputForce, 17), block) == MS::kSuccess);
    checkForces(MVector::zero, 0);
    assert(samples == 0);
    std::cout << "Complete production VDB field compute: host failures, staged arrays, sparse output, fallbacks and recovery passed; OpenVDB/Maya seams\n";
}
