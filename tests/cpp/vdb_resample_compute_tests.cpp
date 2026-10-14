#include "vdb_clip_doubles.h"

// Native sampling, rebuilding, allocation and Maya decomposition are recording
// boundaries, NOT numerical implementations of those libraries.
struct MMatrix {
    double v[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    double* operator[](unsigned int i) { return v[i]; }
    const double* operator[](unsigned int i) const { return v[i]; }
};
static MMatrix matrixInput;
const MMatrix& MDataHandle::asMatrix() const { check(); return matrixInput; }
using MVector = MPoint;
namespace MSpace { enum Space { kWorld }; }
static double matrixScale[3] = {1,1,1}, matrixRotation[3] = {0,0,0};
static MPoint matrixTranslation, matrixPivot;
static int matrixOrder = 0;
struct MTransformationMatrix {
    enum RotationOrder { kXYZ, kYZX, kZXY, kXZY, kYXZ, kZYX, kInvalid };
    explicit MTransformationMatrix(const MMatrix&) {}
    MVector getTranslation(MSpace::Space, MStatus* status) const {
        *status = operation("matrix.translation"); return matrixTranslation;
    }
    MStatus getRotation(double* result, RotationOrder& order) const {
        const auto status = operation("matrix.rotation");
        std::copy(matrixRotation, matrixRotation + 3, result);
        order = static_cast<RotationOrder>(matrixOrder); return status;
    }
    MStatus getScale(double* result, MSpace::Space) const {
        const auto status = operation("matrix.scale");
        std::copy(matrixScale, matrixScale + 3, result); return status;
    }
    MPoint rotatePivot(MSpace::Space, MStatus* status) const {
        *status = operation("matrix.pivot"); return matrixPivot;
    }
};
static int constructors, resamples, rebuilds, automaticRebuilds, prunes, vectors, sampler;
static bool nullRebuild, aliasRebuild, badVectors;
static std::string receivedRotationOrder;
static openvdb::GridClass receivedClass;
namespace openvdb {
struct Mat4d {
    Vec3d scale{1}, translation{0};
    Mat4d inverse() const {
        nativeCall("matrix.inverse"); Mat4d result;
        for (int i = 0; i < 3; ++i) {
            result.scale[i] = 1 / scale[i]; result.translation[i] = -translation[i] / scale[i];
        }
        return result;
    }
};
namespace util { struct NullInterrupter {}; }
template<typename G> typename G::Ptr gridPtrCast(const GridBase::Ptr& grid) {
    return std::dynamic_pointer_cast<G>(grid);
}
namespace tools {
struct PointSampler { enum { order = 0 }; };
struct BoxSampler { enum { order = 1 }; };
struct QuadraticSampler { enum { order = 2 }; };
template<typename S, typename G> void sample(const G& input, G& output) {
    nativeCall("native.resample"); ++resamples; sampler = S::order;
    receivedClass = input.getGridClass();
    output.values = input.values;
    if (invalidNativeOutput && !output.values.empty())
        output.values[0] = InvalidFillValue<typename G::ValueType>::get();
}
struct GridTransformer {
    Mat4d matrix;
    GridTransformer(const Vec3d&, const Vec3d& scale, const Vec3d&, const Vec3d& translate,
                    const std::string& order, const std::string& rotation) {
        nativeCall("native.transformer"); ++constructors;
        assert(order == "tsr"); receivedRotationOrder = rotation;
        // Model the hazardous native 5.2 constructor boundary, including point
        // sampling. This is not a full copy of GridTransformer::init.
        for (int axis = 0; axis < 3; ++axis) {
            if (std::fabs(scale[axis]) < .5) {
                const int mip = int(std::floor(std::log(1 / std::fabs(scale[axis])) / std::log(2.0)));
                const volatile int shifted = 1 << mip; (void)shifted;
            }
        }
        matrix.scale = scale; matrix.translation = translate;
    }
    const Mat4d& getTransform() const { return matrix; }
    void setInterrupter(util::NullInterrupter&) {}
    template<typename S, typename G> void transformGrid(const G& input, G& output) {
        sample<S>(input, output);
    }
};
template<typename S, typename G> void resampleToMatch(const G& input, G& output, util::NullInterrupter&) {
    // Model the native input-class branch, so dispatching the original level
    // set when rebuild is OFF cannot pass this regression.
    if (input.getGridClass() == GRID_LEVEL_SET && input.transform() != output.transform())
        ++automaticRebuilds;
    sample<S>(input, output);
}
template<typename G> typename G::Ptr doLevelSetRebuild(const G& input, typename G::ValueType,
    float outer, float inner, const math::Transform* target, util::NullInterrupter*) {
    nativeCall("native.rebuild"); ++rebuilds;
    assert(outer > 0 && outer == inner && outer <= 64 && target);
    if (nullRebuild) return nullptr;
    if (aliasRebuild) return typename G::Ptr(const_cast<G*>(&input), [](G*){});
    auto result = std::make_shared<G>(input); result->xform = *target;
    if (invalidNativeOutput) result->values[0] = InvalidFillValue<typename G::ValueType>::get();
    return result;
}
template<typename T> void prune(T&, const typename T::ValueType&) { nativeCall("native.prune"); ++prunes; }
template<typename G> void transformVectors(G& grid, const Mat4d&) {
    nativeCall("native.vectors"); ++vectors;
    if (badVectors) grid.values[0] = InvalidFillValue<typename G::ValueType>::get();
}
}}

#include "vdb_resample_compute.inc"

static DOpenVDBData referenceData;
static std::shared_ptr<openvdb::FloatGrid> source;
template<typename G> std::shared_ptr<G> fixture() {
    auto grid = std::make_shared<G>();
    grid->storedBox.reset({32,32,32}, {33,33,33});
    return grid;
}
void setup() {
    resetCalls(); inputData = DOpenVDBData(); outputData = DOpenVDBData(); stagedData = DOpenVDBData();
    referenceData = DOpenVDBData(); source = fixture<openvdb::FloatGrid>();
    inputData.grids = {source};
    auto reference = fixture<openvdb::FloatGrid>();
    reference->xform.forward[0][0] = reference->xform.forward[1][1] = reference->xform.forward[2][2] = 1.25;
    reference->xform.inverse[0][0] = reference->xform.inverse[1][1] = reference->xform.inverse[2][2] = .8;
    referenceData.grids = {reference};
    publications = gridCopies = invalidReads = 0;
    nullCopy = wrongCopy = invalidNativeOutput = nullOutput = invalidOutputPayload = false;
    constructors = resamples = rebuilds = automaticRebuilds = prunes = vectors = 0; sampler = -1;
    nullRebuild = aliasRebuild = badVectors = false;
    socketConnections.clear(); meshConnected = true;
    matrixInput = MMatrix(); matrixTranslation = matrixPivot = MPoint(); matrixOrder = 0;
    std::fill(matrixScale, matrixScale + 3, 1); std::fill(matrixRotation, matrixRotation + 3, 0);
}
MDataBlock block(int mode = 1, bool rebuild = false) {
    MDataBlock data;
    data.inputs["aVdbInput"].payload = &inputData;
    data.inputs["aVdbSelectedGridNames"].text = "*";
    data.inputs["defineMode"].integer = mode;
    data.inputs["interpolationMode"].integer = 1;
    data.inputs["voxelSize"].number = 1.25f;
    data.inputs["voxelScale"].number = 1.25f;
    data.inputs["isRebuild"].integer = rebuild;
    data.inputs["isPrune"].integer = 1;
    data.inputs["tolerance"].number = 0;
    data.inputs["transformVec"].integer = 1;
    data.inputs["referenceVdbInput"].payload = &referenceData;
    data.inputs["referenceName"].text = "*";
    data.inputs["referenceMatrix"] = InputValue();
    return data;
}
MStatus compute(MDataBlock& data) {
    return DOpenVDBResample().compute(MPlug(DOpenVDBResample::aVdbOutput), data);
}
void reject(MDataBlock& data, bool preflight = true) {
    assert(!compute(data) && publications == 0 && invalidReads == 0);
    if (preflight) assert(gridCopies == 0 && resamples == 0 && rebuilds == 0);
}
template<typename G> void typeCases() {
    for (int mode = 0; mode < 4; ++mode) for (int order = 0; order < 3; ++order) {
        setup(); auto grid = fixture<G>(); inputData.grids = {grid}; auto data = block(mode);
        data.inputs["interpolationMode"].integer = order;
        assert(compute(data) && publications == 1 && resamples == 1 && rebuilds == 0 && prunes == 1);
        assert(sampler == (std::is_same<typename G::ValueType,bool>::value ? 0 : order));
        assert(grid->values[0] == typename G::ValueType(1) && grid->getName() == "source");
        assert(outputData.grids.size() == 1 && outputData.grids[0].get() != grid.get());
        assert(outputData.grids[0]->getName() == "source");
        assert(outputData.grids[0]->xform.forward[0][0] == (mode == 0 ? 1 : 1.25));
    }
}
template<typename G> void levelSetCases() {
    for (int mode = 0; mode < 4; ++mode) for (bool rebuild : {false, true}) {
        setup(); auto grid = fixture<G>(); grid->kind = openvdb::GRID_LEVEL_SET;
        inputData.grids = {grid}; auto data = block(mode, rebuild);
        assert(compute(data) && publications == 1 && automaticRebuilds == 0);
        assert(rebuilds == int(rebuild) && resamples == int(!rebuild));
        assert(grid->kind == openvdb::GRID_LEVEL_SET && grid->xform.forward[0][0] == 1);
        assert(outputData.grids[0]->getGridClass() ==
               (rebuild ? openvdb::GRID_LEVEL_SET : openvdb::GRID_UNKNOWN));
        if (!rebuild) assert(receivedClass == openvdb::GRID_UNKNOWN);
    }
}
int main() {
    setup();
    // Check work/byte admission independently of coordinate rejection.
    openvdb::CoordBBox small; small.reset({32,32,32}, {33,33,33});
    for (bool bytes : {false,true}) {
        dommy::vdb::TreeEditBudget budget;
        if (bytes) budget.bytes = dommy::vdb::kMaxVdbPayloadBytes / 4;
        else budget.work = dommy::generation::kMaxWorkItemCount;
        ResampleRegionBudget cost(small, budget, false); cost(*source); assert(!cost.valid);
    }
    for (double scale : {1.0/64, 64.0}) {
        const auto target = openvdb::math::Transform::createLinearTransform(scale);
        assert(resampleMap(source->transform(), *target));
    }
    for (double scale : {1.0/128, 128.0}) {
        const auto target = openvdb::math::Transform::createLinearTransform(scale);
        assert(!resampleMap(source->transform(), *target));
    }
    setup(); auto shifted = source->xform; shifted.shift[0] = 1e7; shifted.inverseShift[0] = -1e7;
    assert(!resampleMap(source->xform, shifted));
    typeCases<openvdb::FloatGrid>(); typeCases<openvdb::DoubleGrid>();
    typeCases<openvdb::BoolGrid>(); typeCases<openvdb::Int32Grid>(); typeCases<openvdb::Int64Grid>();
    typeCases<openvdb::Vec3SGrid>(); typeCases<openvdb::Vec3DGrid>(); typeCases<openvdb::Vec3IGrid>();
    levelSetCases<openvdb::FloatGrid>(); levelSetCases<openvdb::DoubleGrid>();
    int injected = 0;
    for (int scenario = 0; scenario < 6; ++scenario) {
        const int mode = scenario < 4 ? scenario : 0;
        auto prepare = [&]() {
            setup();
            if (scenario == 4) source->kind = openvdb::GRID_LEVEL_SET;
            if (scenario == 5) {
                auto grid = fixture<openvdb::Vec3fGrid>(); grid->vectorType = openvdb::VEC_COVARIANT;
                inputData.grids = {grid};
            }
            return block(mode, scenario == 4);
        };
        auto data = prepare(); assert(compute(data)); const auto trace = operations;
        for (std::size_t i = 0; i < trace.size(); ++i) for (bool exception : {false, true}) {
            data = prepare(); if (exception) attributeThrowAt = int(i); else failAt = int(i);
            assert(!compute(data) && invalidReads == 0);
            assert(publications == (trace[i] == "output.clean" ? 1 : 0));
            assert(source->values[0] == 1 && source->xform.forward[0][0] == 1);
            ++injected;
        }
    }
    for (int mode : {-1, 4}) { setup(); auto data = block(mode); reject(data); }
    for (int order : {-1, 3}) {
        setup(); auto data = block(); data.inputs["interpolationMode"].integer = order; reject(data);
    }
    for (float value : {-1.f, 0.f, 1e-10f, 1e20f, std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity()}) {
        for (int mode : {2,3}) {
            setup(); auto data = block(mode);
            data.inputs[mode == 2 ? "voxelSize" : "voxelScale"].number = value;
            if (mode == 3) { ResampleSettings settings; assert(!readResampleSettings(data, settings)); }
            reject(data);
        }
    }
    for (double value : {0., 1e-10, 1e20, std::numeric_limits<double>::quiet_NaN()}) {
        setup(); auto data = block(0); matrixScale[0] = value; reject(data); assert(constructors == 0);
    }
    for (int order = 0; order < 6; ++order) {
        setup(); auto data = block(0); matrixOrder = order; assert(compute(data));
        assert(receivedRotationOrder == std::vector<std::string>({"xyz","yzx","zxy","xzy","yxz","zyx"})[order]);
    }
    setup(); auto data = block(0); matrixOrder = MTransformationMatrix::kInvalid; reject(data); assert(constructors == 0);
    setup(); data = block(0); matrixInput[0][0] = NAN; reject(data);
    setup(); data = block(0); matrixTranslation.x = 1e7; reject(data);
    setup(); data = block(0); matrixPivot.y = INFINITY; reject(data);
    setup(); data = block(0);
    matrixRotation[2] = NAN; reject(data);
    setup(); data = block(0); meshConnected = false; data.inputs.erase("referenceMatrix");
    assert(compute(data));
    // Disabled controls and unrelated mode inputs are deliberately not consumed.
    for (int mode = 0; mode < 4; ++mode) {
        setup(); data = block(mode); data.inputs["isPrune"].integer = 0; data.inputs.erase("tolerance");
        if (mode != 0) { data.inputs.erase("transformVec"); data.inputs.erase("referenceMatrix"); }
        if (mode != 1) { data.inputs.erase("referenceVdbInput"); data.inputs.erase("referenceName"); }
        if (mode != 2) data.inputs.erase("voxelSize");
        if (mode != 3) data.inputs.erase("voxelScale");
        assert(compute(data) && prunes == 0);
    }
    for (float value : {-1.f, NAN, INFINITY}) {
        setup(); data = block(); data.inputs["tolerance"].number = value; reject(data);
    }
    for (int kind = 0; kind < 4; ++kind) {
        setup(); data = block();
        if (kind == 0) inputData.valid = false;
        if (kind == 1) referenceData.valid = false;
        if (kind == 2) inputData.grids[0].reset();
        if (kind == 3) data.inputs["aVdbInput"].type = 99;
        reject(data);
    }
    setup(); data = block(); referenceData.grids.clear(); reject(data);
    setup(); data = block(); source->values[0] = NAN; reject(data);
    setup(); data = block(); source->bg = INFINITY; reject(data);
    setup(); data = block(); source->values[0] = std::numeric_limits<float>::max(); reject(data);
    setup(); data = block(); source->xform.linear = false; reject(data);
    setup(); data = block(); source->storedBox.reset({1000001,0,0},{1000002,1,1}); reject(data);
    setup(); data = block(); source->storedBox.reset({0,0,0},{10000,10000,10000}); reject(data);
    setup(); data = block(); source->memory = dommy::vdb::kMaxVdbPayloadBytes; reject(data);
    setup(); data = block(); source->storedBox.reset({INT32_MIN,0,0},{INT32_MAX,1,1}); reject(data);
    setup(); data = block(); source->tile = true; source->active = false;
    source->storedBox.reset({0,0,0},{4095,4095,4095}); reject(data);
    source->values[0] = source->bg; resetCalls(); assert(compute(data));
    setup(); data = block(); auto late = fixture<openvdb::FloatGrid>(); late->values[0] = NAN;
    inputData.grids.push_back(late); reject(data);
    for (float background : {0.f, -1.f, 65.f}) {
        setup(); data = block(0, true); source->kind = openvdb::GRID_LEVEL_SET;
        source->bg = background; reject(data);
    }
    setup(); data = block(0, true); auto tinyBand = fixture<openvdb::DoubleGrid>();
    tinyBand->kind = openvdb::GRID_LEVEL_SET; tinyBand->bg = 1e-300;
    inputData.grids = {tinyBand}; reject(data); // positive double must not narrow to zero float
    setup(); data = block(1, true); source->kind = openvdb::GRID_LEVEL_SET;
    source->xform.uniform = false; reject(data);
    // No match and empty input pass through without requiring unused controls.
    setup(); data = block(); data.inputs["aVdbSelectedGridNames"].text = "absent";
    data.inputs.erase("defineMode"); assert(compute(data) && resamples == 0 && gridCopies == 0);
    setup(); data = block(); inputData.grids.clear(); data.inputs.erase("defineMode"); assert(compute(data));
    setup(); data = block(); auto unsupported = std::make_shared<openvdb::points::PointDataGrid>();
    unsupported->xform.linear = false; inputData.grids = {unsupported};
    assert(compute(data) && resamples == 0 && gridCopies == 0 && outputData.grids[0].get() == unsupported.get());
    setup(); data = block(); source->name = "chosen";
    auto other = fixture<openvdb::FloatGrid>(); other->name = "other";
    inputData.grids.push_back(other); data.inputs["aVdbSelectedGridNames"].text = "chosen";
    assert(compute(data) && outputData.grids.size() == 2 && outputData.grids[0]->getName() == "other" &&
           outputData.grids[1]->getName() == "chosen");
    for (bool* flag : {&nullCopy, &wrongCopy, &nullOutput, &invalidOutputPayload, &invalidNativeOutput}) {
        setup(); data = block(); *flag = true; reject(data, false);
    }
    for (bool* flag : {&nullRebuild, &aliasRebuild, &invalidNativeOutput}) {
        setup(); data = block(0, true); source->kind = openvdb::GRID_LEVEL_SET; *flag = true;
        reject(data, false); assert(source->xform.forward[0][0] == 1);
    }
    setup(); data = block(0); auto vector = fixture<openvdb::Vec3fGrid>();
    vector->vectorType = openvdb::VEC_COVARIANT; inputData.grids = {vector}; badVectors = true;
    data.inputs["isPrune"].integer = 0;
    reject(data, false); assert(vector->values[0][0] == 1);
    for (bool world : {false,true}) {
        setup(); data = block(0); vector = fixture<openvdb::Vec3fGrid>();
        vector->vectorType = openvdb::VEC_COVARIANT; vector->worldSpace = world;
        inputData.grids = {vector}; assert(compute(data) && vectors == int(world));
    }
    setup(); data = block(); auto integer = fixture<openvdb::Int32Grid>(); inputData.grids = {integer};
    data.inputs["tolerance"].number = 1e20f; reject(data);
    ResamplePrune prune(0); integer->bg = INT32_MAX; integer->values[0] = INT32_MIN;
    prune(*integer); assert(!prune.valid && prunes == 0);
    setup(); data = block();
    assert(DOpenVDBResample().compute(MPlug(MObject{"other"}), data) == MS::kUnknownParameter && operations.empty());
    std::cout << "Resample compute: 8 types, 4 modes, 3 samplers, rebuild ON/OFF, boundaries and "
              << injected << " status/exception injections passed\n";
}
