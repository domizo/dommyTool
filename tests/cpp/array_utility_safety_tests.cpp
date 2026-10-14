#include "array_utility_doubles.h"

enum {
    DP_V_COMP_NO_OP = 0,
    DP_V_COMP_MAG_MIN,
    DP_V_COMP_MAG_MAX,
    DP_V_COMP_LUM_MIN,
    DP_V_COMP_LUM_MAX,
    DP_VEC_COMPARE_TYPE_COUNT
};
using DP_VEC_COMPARE_TYPE = int;
enum { DP_PTF_MAGNITUDE = 0, DP_PTF_LUMINANCE, DP_PTF_TYPE_COUNT };
using DP_PTF_TYPE = int;
struct NodeBase : AttributeNode {
    MObject thisMObject() const {
        return {"node"};
    }
    MString name() const {
        return "array utility";
    }
};
struct DPStringArrayGen : NodeBase {
    static MObject value, valueSize, outDataArray;
    static MStatus initialize();
    MStatus compute(const MPlug&, MDataBlock&);
};
struct DPArraySize : NodeBase {
    static MObject inputPointArray, inDD, inObject, aCountType, resultCount;
    static MStatus initialize();
    MStatus compute(const MPlug&, MDataBlock&);
};
struct DPVecCompare : NodeBase {
    static MObject inputArray1, inputArray2, compareType, value, valueX, valueY, valueZ,
        resultArray;
    static MStatus initialize();
    MStatus compute(const MPlug&, MDataBlock&);
};
struct DPPointToFloat : NodeBase {
    static MObject inputPointArray, convertType, outDataArray;
    static MStatus initialize();
    MStatus compute(const MPlug&, MDataBlock&);
};
#include "array_utility_methods.inc"

namespace {
MDataBlock configured() {
    MDataBlock data;
    data.inputs["valueSize"].number = 3;
    for (const char* key : {"array", "inputPointArray", "inputArray1", "inputArray2"}) {
        auto& input = data.inputs[key];
        input.points.values = {MPoint(1, 2, 3), MPoint(3, 4, 0), MPoint(0, 0, 0)};
        input.vectors.values = {MVector(1, 2, 3), MVector(3, 4, 0), MVector(0, 0, 0)};
        input.matrices.values.resize(3);
        input.matrices.values[1][0][3] = 4;
        input.floats.values = {1.25f, -2.5f, 3};
        input.doubles.values = {1.25, -2.5, 3};
    }
    return data;
}
template <class Run> MStatus guarded(Run run) {
    try {
        return run();
    } catch (...) {
        return MS::kFailure;
    }
}
template <class Run> int failureCases(Run run, bool exceptions = true) {
    resetRun();
    assert(guarded(run) == MS::kSuccess);
    const auto trace = operations;
    const auto expected = published;
    const auto strings = publishedStrings;
    int tested = 0;
    for (std::size_t i = 0; i < trace.size(); ++i) {
        for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
            resetRun(static_cast<int>(i));
            injectedStatus = code;
            const MStatus result = guarded(run);
            if (trace[i] == "color.get")
                assert(result == MS::kFailure || result == MS::kInvalidParameter);
            else if (trace[i].find("attribute ") == 0 ||
                     (i > 0 && trace[i - 1].find("attribute ") == 0))
                assert(result == MS::kFailure);
            else {
                if (result != code)
                    std::cerr << "Unexpected status at " << i << ": " << trace[i] << '\n';
                assert(result == code);
            }
            assert(callCount == static_cast<int>(i) + 1);
            assert(publications == (trace[i] == "output.clean" ? 1 : 0));
            ++tested;
        }
        if (trace[i].find(".allocate") != std::string::npos ||
            trace[i].find(".copy") != std::string::npos) {
            resetRun();
            shortAt = static_cast<int>(i);
            assert(guarded(run) == MS::kFailure && publications == 0);
            assert(callCount == static_cast<int>(i) + 1);
            ++tested;
        }
        if (exceptions) {
            resetRun();
            throwAt = static_cast<int>(i);
            assert(guarded(run) == MS::kFailure);
            assert(callCount == static_cast<int>(i) + 1);
            assert(publications == (trace[i] == "output.clean" ? 1 : 0));
            ++tested;
        }
        resetRun();
        assert(guarded(run) == MS::kSuccess && published == expected &&
               publishedStrings == strings);
    }
    return tested;
}
void dump(const char* label) {
    std::cout << label;
    for (const auto& value : published)
        for (double component : value)
            std::cout << ':' << text(component);
    for (const auto& value : publishedStrings)
        std::cout << ':' << value;
    std::cout << '\n';
}
int readers(bool normalOnly) {
    auto data = configured();
    activeData = &data;
    const MObject node = {"node"}, attribute = {"array"};
    auto& source = data.inputs["array"];
    int tested = 0;
    for (auto type : {MFn::kPointArrayData, MFn::kVectorArrayData}) {
        source.arrayType = type;
        MPointArray points;
        MVectorArray vectors;
        auto pointRun = [&]() {
            points.values = {MPoint(9, 8, 7, 2)};
            return getPointArrayFromData(node, attribute, points);
        };
        auto vectorRun = [&]() {
            vectors.values = {MVector(9, 8, 7)};
            return getVectorArrayFromData(node, attribute, vectors);
        };
        resetRun();
        assert(pointRun() == MS::kSuccess && points.length() == 4 && points[0].w == 2 &&
               points[3].w == 1);
        resetRun();
        assert(vectorRun() == MS::kSuccess && vectors.length() == 4 && vectors[0].x == 9 &&
               vectors[1].y == 2);
        if (!normalOnly) {
            tested += failureCases(pointRun);
            tested += failureCases(vectorRun);
        } else {
            std::cout << "point/vector:" << int(type) << ':' << points.length() << ':'
                      << vectors.length() << ':' << points[0].w << '\n';
        }
    }
    source.arrayType = MFn::kMatrixArrayData;
    MMatrixArray matrices;
    auto matrixRun = [&]() {
        matrices.values.resize(1);
        return getMatrixArrayFromData(node, attribute, matrices);
    };
    resetRun();
    assert(matrixRun() == MS::kSuccess && matrices.length() == 4 && matrices[2][0][3] == 4);
    if (!normalOnly)
        tested += failureCases(matrixRun);
    for (auto type : {MFn::kDoubleArrayData, MFn::kFloatArrayData}) {
        source.arrayType = type;
        MFloatArray floats;
        MDoubleArray doubles;
        DScalarDataList scalar;
        auto floatRun = [&]() {
            floats.values = {9};
            return getFloatArrayFromData(node, attribute, floats);
        };
        auto doubleRun = [&]() {
            doubles.values = {9};
            return getDoubleArrayFromData(node, attribute, doubles);
        };
        auto scalarRun = [&]() {
            scalar = {9};
            return getDoubleArrayFromData(node, attribute, scalar);
        };
        resetRun();
        assert(floatRun() == MS::kSuccess &&
               floats.values == std::vector<float>({1.25f, -2.5f, 3}));
        resetRun();
        assert(doubleRun() == MS::kSuccess &&
               doubles.values == (type == MFn::kDoubleArrayData
                                      ? std::vector<double>{1.25, -2.5, 3}
                                      : std::vector<double>{9, 1.25, -2.5, 3}));
        resetRun();
        assert(scalarRun() == MS::kSuccess && scalar == std::vector<float>({9, 1.25f, -2.5f, 3}));
        if (!normalOnly) {
            tested += failureCases(floatRun);
            tested += failureCases(doubleRun);
            tested += failureCases(scalarRun);
        } else
            std::cout << "numeric:" << int(type) << ':' << floats.length() << ':'
                      << doubles.length() << ':' << scalar.size() << '\n';
    }
    if (normalOnly)
        return tested;
    MFloatArray floats;
    floats.values = {9};
    source.arrayType = MFn::kDoubleArrayData;
    for (double invalid : {1e100, std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()}) {
        source.doubles.values = {1, invalid, 3};
        resetRun();
        assert(getFloatArrayFromData(node, attribute, floats) == MS::kInvalidParameter &&
               floats.values == std::vector<float>{9});
        DScalarDataList scalar = {9};
        resetRun();
        assert(getDoubleArrayFromData(node, attribute, scalar) == MS::kInvalidParameter &&
               scalar == std::vector<float>{9});
    }
    source.arrayType = MFn::kInvalid;
    resetRun();
    assert(getFloatArrayFromData(node, attribute, floats) == MS::kInvalidParameter);
    MDoubleArray doubles;
    DScalarDataList scalar;
    resetRun();
    assert(getDoubleArrayFromData(node, attribute, doubles) == MS::kInvalidParameter);
    resetRun();
    assert(getDoubleArrayFromData(node, attribute, scalar) == MS::kInvalidParameter);
    source.arrayType = MFn::kPointArrayData;
    source.points.values[0].w = std::numeric_limits<double>::infinity();
    MPointArray points;
    resetRun();
    assert(getPointArrayFromData(node, attribute, points) == MS::kInvalidParameter &&
           points.length() == 0);
    source.arrayType = MFn::kVectorArrayData;
    source.vectors.values[0].x = std::numeric_limits<double>::quiet_NaN();
    MVectorArray vectors;
    resetRun();
    assert(getVectorArrayFromData(node, attribute, vectors) == MS::kInvalidParameter &&
           vectors.length() == 0);
    source.arrayType = MFn::kMatrixArrayData;
    source.matrices.values[0][1][2] = std::numeric_limits<double>::infinity();
    resetRun();
    assert(getMatrixArrayFromData(node, attribute, matrices) == MS::kInvalidParameter);
    source.arrayType = MFn::kDoubleArrayData;
    source.doubles.values.clear();
    resetRun();
    assert(getFloatArrayFromData(node, attribute, floats) == MS::kSuccess && floats.length() == 0);
    source.arrayType = MFn::kPointArrayData;
    source.points.values.clear();
    points.values = {MPoint(9, 8, 7)};
    resetRun();
    assert(getPointArrayFromData(node, attribute, points) == MS::kSuccess && points.length() == 1 &&
           points[0].x == 9);
    assert(!dommy::attribute::validArrayConversionBudget(std::numeric_limits<std::size_t>::max(),
                                                         sizeof(MMatrix)));
    return tested;
}
int timing(bool normalOnly) {
    auto data = configured();
    activeData = &data;
    DPData points;
    points.attributes["abc"] = {2, 4};
    MDoubleArray offsets, stretches;
    const MObject node = {"node"}, attr = {"array"}, name = {"name"};
    int tested = 0;
    for (int mode = 0; mode <= 2; ++mode) {
        data.inputs["array"].connected = true;
        data.inputs["array"].arrayType = MFn::kDoubleArrayData;
        data.inputs["array"].doubles.values = {1.0 / 3.0, 0, 2, 99};
        auto run = [&]() {
            offsets.values.clear();
            stretches.values.clear();
            return getTimeOffsetAndStretch(points, data, node, name, name, attr, attr, mode, mode,
                                           offsets, stretches);
        };
        resetRun();
        assert(run() == MS::kSuccess && offsets.length() == 3 && stretches.length() == 3);
        if (mode == 0)
            assert(offsets.values == std::vector<double>({0, 0, 0}) &&
                   stretches.values == std::vector<double>({1, 1, 1}));
        if (mode == 1)
            assert(offsets.values == std::vector<double>({2, 4, 0}) &&
                   stretches.values == std::vector<double>({2, 4, 1}));
        if (mode == 2)
            assert(offsets.values == std::vector<double>({double(float(1.0 / 3.0)), 0, 2}));
        if (!normalOnly)
            tested += failureCases(run);
        else {
            std::cout << "timing:" << mode;
            for (double value : offsets.values)
                std::cout << ':' << text(value);
            for (double value : stretches.values)
                std::cout << ':' << text(value);
            std::cout << '\n';
        }
    }
    if (normalOnly)
        return tested;
    points.count = 0;
    resetRun();
    assert(getTimeOffsetAndStretch(points, data, node, name, name, attr, attr, 0, 0, offsets,
                                   stretches) == MS::kSuccess &&
           offsets.length() == 0 && stretches.length() == 0);
    for (std::size_t count : {std::size_t(10000001), std::numeric_limits<std::size_t>::max()}) {
        points.count = count;
        resetRun();
        assert(getTimeOffsetAndStretch(points, data, node, name, name, attr, attr, 0, 0, offsets,
                                       stretches) == MS::kInvalidParameter &&
               callCount == 0);
    }
    points.count = 3;
    resetRun();
    assert(getTimeOffsetAndStretch(points, data, node, name, name, attr, attr, 0, 0, offsets,
                                   offsets) == MS::kInvalidParameter);
    resetRun();
    assert(getTimeOffsetAndStretch(points, data, node, name, name, attr, attr, 3, 0, offsets,
                                   stretches) == MS::kInvalidParameter);
    return tested;
}
int nodes(bool normalOnly) {
    int tested = 0;
    auto data = configured();
    activeData = &data;
    DPStringArrayGen strings;
    auto stringRun = [&]() { return strings.compute(MPlug(DPStringArrayGen::outDataArray), data); };
    for (const char* text : {"", "abc", "日本語"}) {
        data.inputs["value"].string = text;
        resetRun();
        assert(stringRun() == MS::kSuccess &&
               publishedStrings == std::vector<std::string>(3, text));
        if (normalOnly)
            dump("string");
        else
            tested += failureCases(stringRun);
    }
    DPVecCompare compare;
    data.inputs["value"].vector[0] = 3;
    data.inputs["value"].vector[1] = 2;
    data.inputs["value"].vector[2] = 1;
    for (int mode = 0; mode < DP_VEC_COMPARE_TYPE_COUNT; ++mode)
        for (bool connected : {false, true}) {
            data.inputs["compareType"].number = mode;
            data.inputs["inputArray2"].connected = connected;
            data.inputs["inputArray2"].vectors.values = {MVector(4, 0, 0), MVector(0, 3, 4),
                                                         MVector(2, 0, 0)};
            data.inputs["inputArray1"].arrayType = data.inputs["inputArray2"].arrayType =
                MFn::kVectorArrayData;
            auto run = [&]() { return compare.compute(MPlug(DPVecCompare::resultArray), data); };
            resetRun();
            assert(run() == MS::kSuccess && publications == 1 && published.size() == 3);
            // Magnitude/luminance ties pick array1 in scalar mode and array2 in array mode.
            const MVector a(1, 2, 3), b = connected ? MVector(4, 0, 0) : MVector(3, 2, 1);
            const bool chooseA =
                mode == 0 || (mode == 1   ? a.length() <= b.length()
                              : mode == 2 ? a.length() >= b.length()
                              : mode == 3 ? 3 <= std::max(b.x, std::max(b.y, b.z))
                                          : 3 >= std::max(b.x, std::max(b.y, b.z)));
            const auto expected = components(chooseA ? a : b);
            assert(published[0] == expected);
            if (connected)
                assert(published[1] == components(mode == 0 ? MVector(3, 4, 0) : MVector(0, 3, 4)));
            if (normalOnly)
                dump("compare");
            else
                tested += failureCases(run);
        }
    DPPointToFloat convert;
    for (int mode = 0; mode < 2; ++mode) {
        data.inputs["convertType"].number = mode;
        auto run = [&]() { return convert.compute(MPlug(DPPointToFloat::outDataArray), data); };
        resetRun();
        assert(run() == MS::kSuccess && publications == 1 && published.size() == 3);
        assert(published[1][0] == (mode == 0 ? 5 : 4));
        if (normalOnly)
            dump("point to float");
        else
            tested += failureCases(run);
    }
    DPArraySize size;
    auto sizeRun = [&]() { return size.compute(MPlug(DPArraySize::resultCount), data); };
    resetRun();
    assert(sizeRun() == MS::kSuccess && published[0][0] == 0);
    if (!normalOnly)
        tested += failureCases(sizeRun);
    data.inputs["inputPointArray"].connected = true;
    resetRun();
    assert(sizeRun() == MS::kSuccess && published[0][0] == 3);
    if (normalOnly)
        dump("point count");
    else
        tested += failureCases(sizeRun);
    data.inputs["inputPointArray"].connected = false;
    data.inputs["inObject"].connected = true;
    for (auto type : {MFnData::kMesh, MFnData::kNurbsCurve, MFnData::kNurbsSurface}) {
        data.inputs["inObject"].type = type;
        resetRun();
        assert(sizeRun() == MS::kSuccess &&
               published[0][0] == (type == MFnData::kMesh         ? 4
                                   : type == MFnData::kNurbsCurve ? 3
                                                                  : 6));
        if (normalOnly)
            dump("object count");
        else
            tested += failureCases(sizeRun);
    }
    data.inputs["inDD"].connected = true;
    for (const auto& id : {DPData::id, DCurveData::id, DMeshData::id})
        for (int type = 0; type < 2; ++type) {
            data.inputs["inDD"].typeId = id;
            data.inputs["CountType"].number = type;
            resetRun();
            assert(sizeRun() == MS::kSuccess &&
                   published[0][0] == (id == DPData::id || type == 0 ? 3 : 1));
            if (normalOnly)
                dump("custom count");
            else
                tested += failureCases(sizeRun);
        }
    if (normalOnly)
        return tested;
    for (int count : {0, -1, 1000001}) {
        data.inputs["valueSize"].number = count;
        resetRun();
        assert(stringRun() == MS::kInvalidParameter && publications == 0);
    }
    data.inputs["valueSize"].number = 1000000;
    data.inputs["value"].string = std::string(101, 'x').c_str();
    resetRun();
    assert(stringRun() == MS::kInvalidParameter && publications == 0 && callCount == 2);
    data.inputs["inDD"].typeId = DPData::id;
    pointData.nullData = true;
    resetRun();
    assert(sizeRun() == MS::kFailure && publications == 0);
    pointData.nullData = false;
    pointData.valid = false;
    resetRun();
    assert(sizeRun() == MS::kInvalidParameter && publications == 0);
    pointData.valid = true;
    data.inputs["inDD"].typeId = DCurveData::id;
    data.inputs["CountType"].number = 0;
    curveData.checkedValid = false;
    resetRun();
    assert(sizeRun() == MS::kInvalidParameter && publications == 0);
    curveData.checkedValid = true;
    data.inputs["inDD"].typeId = DMeshData::id;
    meshData.checkedValid = false;
    resetRun();
    assert(sizeRun() == MS::kInvalidParameter && publications == 0);
    meshData.checkedValid = true;
    data.inputs["CountType"].number = 2;
    resetRun();
    assert(sizeRun() == MS::kInvalidParameter && publications == 0);
    data.inputs["inputArray2"].vectors.values.resize(2);
    resetRun();
    assert(compare.compute(MPlug(DPVecCompare::resultArray), data) == MS::kInvalidParameter &&
           publications == 0);
    data.inputs["inputArray2"].connected = false;
    data.inputs["inputArray1"].vectors.values[0].x = 1e300;
    for (int mode : {DP_V_COMP_MAG_MIN, DP_V_COMP_LUM_MIN}) {
        data.inputs["compareType"].number = mode;
        resetRun();
        assert(compare.compute(MPlug(DPVecCompare::resultArray), data) == MS::kInvalidParameter &&
               publications == 0);
    }
    data.inputs["convertType"].number = DP_PTF_LUMINANCE;
    data.inputs["inputPointArray"].points.values[0].x = 1e100;
    resetRun();
    assert(convert.compute(MPlug(DPPointToFloat::outDataArray), data) == MS::kInvalidParameter &&
           publications == 0);
    return tested;
}
int blockReaders() {
    auto block = configured();
    auto outside = configured();
    activeData = &outside;
    const MObject attr{"array"};
    auto& source = block.inputs["array"];
    outside.inputs["array"].points.values = {MPoint(99, 99, 99)};
    outside.inputs["array"].vectors.values = {MVector(99, 99, 99)};
    outside.inputs["array"].floats.values = {99};
    outside.inputs["array"].doubles.values = {99};
    outside.inputs["array"].matrices.values = {MMatrix()};
    int tested = 0;
    for (auto type : {MFn::kPointArrayData, MFn::kVectorArrayData}) {
        source.arrayType = type;
        MPointArray points;
        const auto pointRun = [&]() {
            points.values = {MPoint(9, 8, 7)};
            return getPointArrayFromData(block, attr, points);
        };
        resetRun(); assert(pointRun() == MS::kSuccess && points.length() == 4 && points[1].x == 1);
        tested += failureCases(pointRun);
        MVectorArray vectors;
        const auto vectorRun = [&]() {
            vectors.values = {MVector(9, 8, 7)};
            return getVectorArrayFromData(block, attr, vectors);
        };
        resetRun(); assert(vectorRun() == MS::kSuccess && vectors.length() == 4 && vectors[1].x == 1);
        tested += failureCases(vectorRun);
    }
    source.arrayType = MFn::kMatrixArrayData;
    MMatrixArray matrices;
    const auto matrixRun = [&]() {
        matrices.values = {MMatrix()};
        return getMatrixArrayFromData(block, attr, matrices);
    };
    resetRun(); assert(matrixRun() == MS::kSuccess && matrices.length() == 4 && matrices[2][0][3] == 4);
    tested += failureCases(matrixRun);
    for (auto type : {MFn::kFloatArrayData, MFn::kDoubleArrayData}) {
        source.arrayType = type;
        MFloatArray floats;
        MDoubleArray doubles;
        DScalarDataList scalars;
        const auto floatRun = [&]() {floats.values = {9}; return getFloatArrayFromData(block, attr, floats);};
        const auto doubleRun = [&]() {doubles.values = {9}; return getDoubleArrayFromData(block, attr, doubles);};
        const auto scalarRun = [&]() {scalars = {9}; return getDoubleArrayFromData(block, attr, scalars);};
        resetRun(); assert(floatRun() == MS::kSuccess && floats.length() == 3 && floats[0] == 1.25f);
        resetRun(); assert(doubleRun() == MS::kSuccess && doubles.length() == (type == MFn::kDoubleArrayData ? 3U : 4U));
        assert(doubles[type == MFn::kDoubleArrayData ? 0U : 1U] == 1.25);
        resetRun(); assert(scalarRun() == MS::kSuccess && scalars.size() == 4 && scalars[1] == 1.25f);
        tested += failureCases(floatRun) + failureCases(doubleRun) + failureCases(scalarRun);
    }
    source.nullObject = true;
    MFloatArray unchanged; unchanged.values = {9};
    resetRun(); assert(getFloatArrayFromData(block, attr, unchanged) == MS::kFailure && unchanged.values[0] == 9);
    source.nullObject = false;
    source.arrayType = MFn::kInvalid;
    resetRun(); assert(getFloatArrayFromData(block, attr, unchanged) == MS::kInvalidParameter);

    // The graph's connection state is shared, but value objects must come from
    // the supplied block. The two stores intentionally disagree in this seam.
    DPVecCompare compare;
    block.inputs["inputArray1"].arrayType = MFn::kVectorArrayData;
    outside.inputs["inputArray1"].arrayType = MFn::kVectorArrayData;
    outside.inputs["inputArray1"].vectors.values = {MVector(99, 0, 0)};
    resetRun(); assert(compare.compute(MPlug(DPVecCompare::resultArray), block) == MS::kSuccess);
    assert(published.size() == 3 && published[0][0] == 1);
    DPArraySize size;
    outside.inputs["inputPointArray"].connected = true;
    outside.inputs["inputPointArray"].points.values.resize(1);
    resetRun(); assert(size.compute(MPlug(DPArraySize::resultCount), block) == MS::kSuccess && published[0][0] == 3);
    outside.inputs["inputPointArray"].connected = false;
    outside.inputs["inObject"].connected = true;
    outside.inputs["inObject"].nullObject = true;
    block.inputs["inObject"].type = MFnData::kMesh;
    resetRun(); assert(size.compute(MPlug(DPArraySize::resultCount), block) == MS::kSuccess && published[0][0] == 4);
    source.arrayType = MFn::kDoubleArrayData;
    source.doubles.values = {1.0 / 3.0, 2, 3};
    outside.inputs["array"].connected = true;
    DPData points;
    MDoubleArray offsets, stretches;
    resetRun(); assert(getTimeOffsetAndStretch(points, block, MObject{"node"}, attr, attr,
        attr, attr, 2, 2, offsets, stretches) == MS::kSuccess);
    assert(offsets.length() == 3 && offsets[0] == double(float(1.0 / 3.0)) && stretches[2] == 3);
    std::cout << "Block readers: independent input store, six overloads, legacy append/replace/rounding and "
              << tested << " failure cases passed\n";
    return tested;
}
} // namespace

int main(int argc, char** argv) {
    const std::string schema = schemaTrace("DPStringArrayGen", DPStringArrayGen::initialize) +
                               schemaTrace("DPArraySize", DPArraySize::initialize) +
                               schemaTrace("DPVecCompare", DPVecCompare::initialize) +
                               schemaTrace("DPPointToFloat", DPPointToFloat::initialize);
    if (argc == 2 && std::string(argv[1]) == "--schema") {
        std::cout << schema;
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--normal") {
        readers(true);
        timing(true);
        nodes(true);
        return 0;
    }
    assert(argc == 2);
    std::ifstream fixture(argv[1]);
    assert(fixture);
    const std::string expected((std::istreambuf_iterator<char>(fixture)),
                               std::istreambuf_iterator<char>());
    assert(schema == expected);
    const int initialization = testFailures("DPStringArrayGen", DPStringArrayGen::initialize, 17) +
                               testFailures("DPArraySize", DPArraySize::initialize, 37) +
                               testFailures("DPVecCompare", DPVecCompare::initialize, 52) +
                               testFailures("DPPointToFloat", DPPointToFloat::initialize, 19);
    const int readerFailures = readers(false), timingFailures = timing(false),
              nodeFailures = nodes(false);
    blockReaders();
    std::cout << "Array utilities: " << initialization << " initializer, " << readerFailures
              << " reader, " << timingFailures << " timing and " << nodeFailures
              << " compute failure cases passed\n";
}
