#include "vdb_analysis_doubles.h"
#include "vdb_analysis_compute.inc"

static DOpenVDBData maskData;
template <typename T = openvdb::FloatGrid> std::shared_ptr<T> grid(const std::string& name) {
    auto result = std::make_shared<T>();
    result->name = name;
    return result;
}
template <typename T = openvdb::FloatGrid> std::shared_ptr<T> source() {
    return std::dynamic_pointer_cast<T>(std::const_pointer_cast<Grid>(inputData.grids[0]));
}
void setup() {
    resetCalls();
    injectedStatus = MS::kFailure;
    inputData = DOpenVDBData();
    stagedData = DOpenVDBData();
    outputData = DOpenVDBData();
    maskData = DOpenVDBData();
    inputData.grids = {grid("source"), grid<Grid>("keep")};
    maskData.grids = {grid("mask")};
    outputData.grids = {grid<Grid>("previous")};
    publications = invalidReads = analysisCalls = maskedCalls = resampleCalls = maskCalls = 0;
    meshConnected = false;
    nullAnalysis = wrongAnalysis = aliasAnalysis = badAnalysis = false;
    nullOutput = invalidOutputPayload = nullCopy = false;
    failAnalysisAt = -1;
}
MDataBlock block(int operation = 0, int renaming = 0) {
    MDataBlock data;
    data.inputs["aVdbInput"].payload = &inputData;
    data.inputs["aVdbSelectedGridNames"].text = "source";
    data.inputs["aOperator"].integer = operation;
    data.inputs["aOutputName"].integer = renaming;
    data.inputs["aCustomName"].text = "custom";
    data.inputs["aMaskVdbInput"].payload = &maskData;
    data.inputs["aMaskGridNames"].text = "*";
    return data;
}
MStatus compute(MDataBlock& data) {
    return DOpenVDBAnalysis().compute(MPlug(DOpenVDBAnalysis::aVdbOutput), data);
}
void reject(MDataBlock& data, bool beforeNative = true) {
    assert(!compute(data) && publications == 0 && invalidReads == 0);
    assert(outputData.grids.size() == 1 && outputData.grids[0]->name == "previous");
    if (beforeNative) {
        assert(analysisCalls == 0 && resampleCalls == 0 && maskCalls == 0);
        for (const char* operation : {"native.topologyUnion", "grid.create", "output.create"})
            assert(std::find(operations.begin(), operations.end(), operation) == operations.end());
    }
}
void contracts() {
    const char* suffixes[] = {"_gradient",   "_curvature", "_laplacian", "_cpt",
                              "_divergence", "_curl",      "_magnitude", "_normalize"};
    for (int operation = 0; operation < 8; ++operation)
        for (int precision = 0; precision < 2; ++precision)
            for (int rename = 0; rename < 3; ++rename)
                for (bool masked : {false, true}) {
                    setup();
                    auto data = block(operation, rename);
                    meshConnected = masked;
                    if (operation < 4)
                        inputData.grids[0] = precision
                                                 ? GridCPtr(grid<openvdb::DoubleGrid>("source"))
                                                 : grid("source");
                    else
                        inputData.grids[0] = precision
                                                 ? GridCPtr(grid<openvdb::Vec3DGrid>("source"))
                                                 : grid<openvdb::Vec3SGrid>("source");
                    assert(compute(data) && publications == 1 && analysisCalls == 1);
                    assert(maskedCalls == int(masked) && resampleCalls == int(masked));
                    assert(outputData.grids.size() == 2 && outputData.grids[0]->name == "keep");
                    const auto& result = outputData.grids[1];
                    assert(result.get() != inputData.grids[0].get());
                    assert(result->name == (rename == 2 ? "custom"
                                                        : std::string("source") +
                                                              (rename ? suffixes[operation] : "")));
                    assert(getVdbGridType(*result) ==
                           analysisOutputType(operation, getVdbGridType(*inputData.grids[0])));
                    assert(lastDensify == (operation < 6));
                    assert(inputData.grids[0]->name == "source");
                }
    for (const auto& unsupported :
         GridCPtrVec{grid<openvdb::Int32Grid>("source"), grid<openvdb::Int64Grid>("source"),
                     grid<openvdb::Vec3IGrid>("source"), grid<openvdb::BoolGrid>("source"),
                     grid<Grid>("source")}) {
        for (int operation = 0; operation < 8; ++operation) {
            setup();
            inputData.grids[0] = unsupported;
            auto data = block(operation, 1);
            assert(compute(data) && analysisCalls == 0 && outputData.grids[1]->name == "source");
        }
    }
    setup();
    auto data = block();
    data.inputs["aVdbSelectedGridNames"].text = "missing";
    data.inputs.erase("aOperator");
    data.inputs.erase("aMaskVdbInput");
    assert(compute(data) && analysisCalls == 0 && outputData.grids.size() == 2);
    setup();
    data = block();
    data.inputs.erase("aCustomName");
    data.inputs.erase("aMaskVdbInput");
    data.inputs.erase("aMaskGridNames");
    assert(compute(data));
    setup();
    data = block(0, 2);
    data.inputs["aCustomName"].text.clear();
    assert(compute(data) && outputData.grids[1]->name == "source");
    setup();
    data = block();
    source()->vacant = true;
    assert(compute(data) && analysisCalls == 1); // Background analysis is retained for empty grids.
    for (int kind = 0; kind < 4; ++kind) {
        setup();
        data = block();
        meshConnected = true;
        if (kind == 0)
            data.inputs["aMaskGridNames"].text = "missing";
        if (kind == 1)
            maskData.grids = {grid<openvdb::BoolGrid>("unsupported"), grid("second")};
        if (kind == 2)
            maskData.grids = {grid<openvdb::Int32Grid>("mask")};
        if (kind == 3) {
            auto mask = grid("mask");
            mask->kind = openvdb::GRID_LEVEL_SET;
            maskData.grids = {mask};
        }
        assert(compute(data) && maskedCalls == (kind >= 2 ? 1 : 0));
        assert(maskCalls == (kind == 3 ? 1 : 0));
    }
    setup();
    data = block();
    assert(DOpenVDBAnalysis().compute(MPlug({"unknown"}), data) == MS::kUnknownParameter &&
           callCount == 0);
}
template <typename GridT, typename T> void vectorMath(T large) {
    using Vector = typename GridT::ValueType;
    setup();
    inputData.grids[0] = grid<GridT>("source");
    source<GridT>()->values = {Vector(large, 0, 0)};
    auto data = block(6);
    assert(compute(data));
    using Scalar = typename openvdb::tools::VectorToScalarConverter<GridT>::Type;
    assert(dynamic_cast<const Scalar&>(*outputData.grids[1]).values[0] == large);
    for (const Vector value :
         {Vector(3, 4, 0), Vector(large, large, large), Vector(0), Vector(T(1e-10), 0, 0)}) {
        setup();
        inputData.grids[0] = grid<GridT>("source");
        source<GridT>()->values = {value};
        source<GridT>()->vectorType = openvdb::VEC_COVARIANT;
        data = block(7);
        assert(compute(data));
        const auto& result = dynamic_cast<const GridT&>(*outputData.grids[1]);
        const auto n = result.values[0];
        const double length = std::hypot(std::hypot(double(n[0]), double(n[1])), double(n[2]));
        assert(std::fabs(length - (value[0] >= T(1e-7) ? 1.0 : 0.0)) < 1e-6);
        if (value[0] == T(3))
            assert(std::fabs(n[0] - .6) < 1e-6 && std::fabs(n[1] - .8) < 1e-6);
        assert(result.vectorType == openvdb::VEC_COVARIANT_NORMALIZE);
    }
    setup();
    inputData.grids[0] = grid<GridT>("source");
    source<GridT>()->values = {Vector(std::numeric_limits<T>::max())};
    data = block(6);
    reject(data); // finite components but unrepresentable length
    setup();
    inputData.grids[0] = grid<GridT>("source");
    source<GridT>()->bg = Vector(std::numeric_limits<T>::max());
    data = block(6);
    reject(data); // background is also evaluated by the native operator
}
void invalidInputs() {
    for (int kind = 0; kind < 13; ++kind) {
        setup();
        auto data = block();
        if (kind == 0)
            data.inputs["aOperator"].integer = -1;
        if (kind == 1)
            data.inputs["aOperator"].integer = 8;
        if (kind == 2)
            data.inputs["aOutputName"].integer = -1;
        if (kind == 3)
            data.inputs["aOutputName"].integer = 3;
        if (kind == 4)
            data.inputs["aVdbInput"].payload = nullptr;
        if (kind == 5)
            data.inputs["aVdbInput"].type = 999;
        if (kind == 6)
            inputData.valid = false;
        if (kind == 7)
            inputData.grids[0] = nullptr;
        if (kind >= 8)
            meshConnected = true;
        if (kind == 8)
            data.inputs["aMaskVdbInput"].payload = nullptr;
        if (kind == 9)
            data.inputs["aMaskVdbInput"].type = 999;
        if (kind == 10)
            maskData.valid = false;
        if (kind == 11)
            maskData.grids[0] = nullptr;
        if (kind == 12) {
            auto m = grid("mask");
            m->kind = openvdb::GRID_LEVEL_SET;
            m->bg = 0;
            maskData.grids = {m};
        }
        reject(data);
    }
    for (float value :
         {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(), 1e30f})
        for (bool background : {false, true}) {
            setup();
            auto data = block();
            if (background)
                source()->bg = value;
            else
                source()->values[0] = value;
            reject(data);
        }
    for (int kind = 0; kind < 7; ++kind) {
        setup();
        auto data = block();
        if (kind == 0)
            source()->xform.linear = false;
        if (kind == 1)
            source()->dx[0] = 0;
        if (kind == 2)
            source()->xform.shift[0] = std::numeric_limits<double>::infinity();
        if (kind == 3)
            source()->xform.forward[0][0] = 1e7;
        if (kind == 4)
            source()->xform.inverse[0][0] = 1e7;
        if (kind == 5)
            source()->storedBox.reset({INT32_MAX - 7, 0, 0}, {INT32_MAX, 0, 0});
        if (kind == 6)
            source()->storedBox.reset({INT32_MIN, 0, 0}, {INT32_MIN + 7, 0, 0});
        reject(data);
    }
    for (int kind = 0; kind < 5; ++kind) {
        setup();
        auto data = block();
        meshConnected = true;
        auto mask = grid("mask");
        maskData.grids = {mask};
        if (kind == 0)
            mask->xform.shift[0] = 3e9;
        if (kind == 1) {
            mask->xform.forward[0][0] = .001;
            mask->xform.inverse[0][0] = 1000;
        }
        if (kind == 2) {
            mask->xform.inverse[0][0] = .001;
            mask->xform.forward[0][0] = 1000;
        }
        if (kind == 3)
            mask->values[0] = std::numeric_limits<float>::quiet_NaN();
        if (kind == 4)
            mask->bg = std::numeric_limits<float>::infinity();
        reject(data);
    }
}
void budgetsAndTransactions() {
    setup();
    auto data = block();
    source()->tile = true;
    source()->storedBox.reset({0, 0, 0}, {4095, 4095, 4095});
    reject(data);
    for (int operation : {6, 7}) {
        setup();
        data = block(operation);
        inputData.grids[0] = grid<openvdb::Vec3DGrid>("source");
        auto g = source<openvdb::Vec3DGrid>();
        g->tile = true;
        g->storedBox.reset({0, 0, 0}, {4095, 4095, 4095});
        assert(compute(data) && !lastDensify);
    }
    setup();
    data = block();
    source()->memory = dommy::vdb::kMaxVdbPayloadBytes / 4 + 1;
    reject(data);
    setup();
    data = block(7);
    inputData.grids[0] = grid<openvdb::Vec3DGrid>("source");
    source<openvdb::Vec3DGrid>()->memory = dommy::vdb::kMaxVdbPayloadBytes / 12;
    reject(data);
    setup();
    data = block();
    meshConnected = true;
    auto mask = grid("mask");
    mask->tile = true;
    mask->storedBox.reset({0, 0, 0}, {4095, 4095, 4095});
    maskData.grids = {mask};
    reject(data);
    setup();
    data = block();
    inputData.grids.push_back(grid("late"));
    data.inputs["aVdbSelectedGridNames"].text = "source late";
    auto late = std::dynamic_pointer_cast<openvdb::FloatGrid>(
        std::const_pointer_cast<Grid>(inputData.grids.back()));
    late->bg = std::numeric_limits<float>::quiet_NaN();
    meshConnected = true;
    reject(data);
    for (int kind = 0; kind < 7; ++kind) {
        setup();
        data = block();
        if (kind == 0)
            nullAnalysis = true;
        if (kind == 1)
            wrongAnalysis = true;
        if (kind == 2) {
            aliasAnalysis = true;
            data.inputs["aOperator"].integer = 2;
        }
        if (kind == 3)
            badAnalysis = true;
        if (kind == 4)
            nullOutput = true;
        if (kind == 5)
            invalidOutputPayload = true;
        if (kind == 6) {
            inputData.grids.push_back(grid("late"));
            data.inputs["aVdbSelectedGridNames"].text = "source late";
            failAnalysisAt = 1;
        }
        reject(data, false);
        assert(source()->name == "source" && source()->values[0] == 1);
    }
    setup();
    data = block();
    meshConnected = true;
    mask = grid("mask");
    mask->kind = openvdb::GRID_LEVEL_SET;
    maskData.grids = {mask};
    nullCopy = true;
    reject(data, false);
}
void injection() {
    int tested = 0;
    for (int mode = 0; mode < 4; ++mode) {
        auto prepare = [mode]() {
            setup();
            meshConnected = mode != 0;
            auto data = block(mode == 3 ? 6 : 0, 2);
            if (mode == 2) {
                auto m = grid("mask");
                m->kind = openvdb::GRID_LEVEL_SET;
                maskData.grids = {m};
            }
            if (mode == 3)
                inputData.grids[0] = grid<openvdb::Vec3DGrid>("source");
            return data;
        };
        auto data = prepare();
        assert(compute(data));
        const int calls = callCount;
        for (int failure = 0; failure < calls; ++failure)
            for (int mechanism = 0; mechanism < 3; ++mechanism) {
                data = prepare();
                if (mechanism == 2)
                    attributeThrowAt = failure;
                else {
                    failAt = failure;
                    injectedStatus = mechanism ? MS::kInvalidParameter : MS::kFailure;
                }
                assert(!compute(data) && invalidReads == 0 && callCount == failure + 1);
                if (failure == calls - 1)
                    assert(publications == 1);
                else
                    assert(publications == 0 && outputData.grids[0]->name == "previous");
                assert(inputData.grids[0]->name == "source");
                ++tested;
            }
    }
    std::cout << "Analysis compute: " << tested << " failed statuses/exceptions passed\n";
}
int main() {
    contracts();
    vectorMath<openvdb::Vec3SGrid>(1e30f);
    vectorMath<openvdb::Vec3DGrid>(1e300);
    invalidInputs();
    budgetsAndTransactions();
    injection();
    std::cout << "Analysis: type policy, stable vectors, mask/coordinate/tree budgets and "
                 "transactions passed\n";
}
