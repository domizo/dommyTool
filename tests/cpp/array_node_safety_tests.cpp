#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DIndexPattern.h"
#include "common/utility/DRandomValueGeneration.h"
#include "common/utility/DStatusOperation.h"
#include "math/DRandomGenerator.h"

#include <array>
#include <fstream>
#include <iostream>
#include <map>

// Logging compatibility for executing the archived implementations unchanged.
#define CHECK_MSTATUS(expression)                                                                  \
    do {                                                                                           \
        const MStatus result = (expression);                                                       \
        if (!result)                                                                               \
            result.pAPIerror(__FILE__, __LINE__);                                                  \
    } while (false)
#define CHECK_MSTATUS_AND_RETURN_IT(status) DOMMY_RETURN_IF_FAILURE(status)

namespace {
int shortAt = -1, throwAt = -1, readsAfterFault = 0;
bool faultReached = false;
int publications = 0, cleanCalls = 0, handleCleanCalls = 0;
double currentContext = 9.0;
std::vector<double> sampledTimes;
std::vector<std::vector<double>> published;
MStatus api(const std::string& name) {
    const int index = callCount;
    const MStatus status = operation(name);
    if (!status || index == shortAt || index == throwAt)
        faultReached = true;
    if (index == throwAt)
        throw std::runtime_error("injected API exception");
    return status;
}
void resetRun(int failure = -1) {
    resetCalls(failure);
    shortAt = throwAt = -1;
    faultReached = false;
    readsAfterFault = publications = cleanCalls = handleCleanCalls = 0;
    sampledTimes.clear();
    published.clear();
    currentContext = 9.0;
}
void readValue() {
    if (faultReached)
        ++readsAfterFault;
}
} // namespace

struct MFnStringData {
    MObject create(const char* value, MStatus* status = nullptr) {
        const MStatus result = operation(std::string("string.create ") + value);
        if (status)
            *status = result;
        return result ? MObject{"defaultString"} : MObject::kNullObj;
    }
};
struct MVector {
    double x, y, z;
    static const MVector zero;
    MVector(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
    double length() const {
        return std::sqrt(x * x + y * y + z * z);
    }
    MVector operator/(double value) const {
        return {x / value, y / value, z / value};
    }
    MVector operator+(const MVector& other) const {
        return {x + other.x, y + other.y, z + other.z};
    }
};
const MVector MVector::zero;
struct MPoint {
    double x, y, z, w;
    MPoint(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z), w(1) {}
    explicit MPoint(const MVector& value) : x(value.x), y(value.y), z(value.z), w(1) {}
    operator MVector() const {
        return {x, y, z};
    }
};
struct MMatrix {
    // Component transport only, NOT a real 4x4 transform representation.
    // Rows 0/1/2 record scale/rotation/translation passed through the Maya API.
    double values[4][4] = {{1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 1}};
    double* operator[](unsigned int row) {
        return values[row];
    }
    const double* operator[](unsigned int row) const {
        return values[row];
    }
};
template <class T> const char* arrayName();
template <> const char* arrayName<MVector>() {
    return "vector";
}
template <> const char* arrayName<MPoint>() {
    return "point";
}
template <> const char* arrayName<MMatrix>() {
    return "matrix";
}
template <> const char* arrayName<float>() {
    return "float";
}
template <> const char* arrayName<double>() {
    return "double";
}
template <class T> struct Array {
    std::vector<T> values;
    Array() = default;
    // Keep the original constructor paths failure-injectable for mutation tests.
    explicit Array(unsigned int count, const T& initial = T()) {
        if (setLength(count))
            std::fill(values.begin(), values.end(), initial);
    }
    Array(const Array& source) {
        copy(source);
    }
    MStatus setLength(unsigned int count) {
        const int index = callCount;
        const MStatus status = api(std::string(arrayName<T>()) + ".allocate");
        if (status)
            values.resize(index == shortAt && count ? count - 1 : count);
        return status;
    }
    MStatus copy(const Array& source) {
        const int index = callCount;
        const MStatus status = api(std::string(arrayName<T>()) + ".copy");
        if (status) {
            values = source.values;
            if (index == shortAt && !values.empty())
                values.pop_back();
        }
        return status;
    }
    MStatus set(const T& value, unsigned int index) {
        const MStatus status = api(std::string(arrayName<T>()) + ".set");
        if (status)
            values.at(index) = value;
        return status;
    }
    unsigned int length() const {
        return static_cast<unsigned int>(values.size());
    }
    T& operator[](unsigned int index) {
        readValue();
        return values.at(index);
    }
    const T& operator[](unsigned int index) const {
        readValue();
        return values.at(index);
    }
};
using MVectorArray = Array<MVector>;
using MPointArray = Array<MPoint>;
using MMatrixArray = Array<MMatrix>;
using MFloatArray = Array<float>;
using MDoubleArray = Array<double>;
using double3 = double[3];
using float3 = float[3];

struct Input {
    double number = 0;
    float3 vector = {0, 0, 0};
    double3 precise = {0, 0, 0};
    MMatrix matrix;
    MString string;
};
struct Edit {
    unsigned int logicalIndex;
    Input index, value;
};
namespace {
std::vector<Edit> edits;
std::vector<float> inputFloats = {1, 2, 3, 4};
bool parentConnected = false, childConnected = false;
Input sampledInput;
} // namespace

struct MDataHandle {
    bool valid = true;
    Input* input = nullptr;
    Edit* edit = nullptr;
    void check() const {
        assert(valid);
        readValue();
    }
    short asShort() const {
        check();
        return static_cast<short>(input->number);
    }
    int asInt() const {
        check();
        return static_cast<int>(input->number);
    }
    bool asBool() const {
        check();
        return input->number != 0;
    }
    double asDouble() const {
        check();
        return input->number;
    }
    float3& asFloat3() const {
        check();
        return input->vector;
    }
    double3& asDouble3() const {
        check();
        return input->precise;
    }
    MMatrix asMatrix() const {
        check();
        return input->matrix;
    }
    MString asString() const {
        check();
        return input->string;
    }
    MVector asVector() const {
        check();
        return {input->vector[0], input->vector[1], input->vector[2]};
    }
    MDataHandle child(const MObject& attribute) const {
        check();
        assert(edit);
        return MDataHandle(true, attribute.name == "dataIndex" ? &edit->index : &edit->value);
    }
    MDataHandle() = default;
    MDataHandle(bool valid, Input* input = nullptr, Edit* edit = nullptr)
        : valid(valid), input(input), edit(edit) {}
    MStatus set(const MObject&) {
        assert(valid);
        const auto status = api("output.set");
        if (status)
            ++publications;
        return status;
    }
    void setClean() {
        assert(valid);
        ++handleCleanCalls;
    }
};
struct MArrayDataHandle {
    bool valid;
    unsigned int index = 0;
    explicit MArrayDataHandle(bool valid) : valid(valid) {}
    unsigned int elementCount(MStatus* status) {
        assert(valid);
        *status = api("edits.count");
        return static_cast<unsigned int>(edits.size());
    }
    MStatus jumpToArrayElement(unsigned int physicalIndex) {
        assert(valid);
        index = physicalIndex;
        return api("edits.jump");
    }
    MDataHandle inputValue(MStatus* status) {
        assert(valid);
        *status = api("edits.input");
        return MDataHandle(bool(*status), nullptr, &edits.at(index));
    }
};
struct MPlug {
    std::string name;
    bool isChild = false;
    MPlug(const MObject&, const MObject& attr) : name(attr.name) {}
    bool operator==(const MObject& attr) const {
        return name == attr.name;
    }
    bool operator!=(const MObject& attr) const {
        return !(*this == attr);
    }
    unsigned int numChildren(MStatus* status) const {
        *status = api("plug.children");
        return 3;
    }
    MPlug child(unsigned int, MStatus* status) const {
        *status = api("plug.child");
        MPlug result = *this;
        result.isChild = true;
        return result;
    }
    bool isConnected(MStatus* status = nullptr) const {
        const MStatus result = api("connection " + name);
        if (status) *status = result;
        return isChild ? childConnected : parentConnected;
    }
};
struct MDataBlock {
    std::map<std::string, Input> inputs;
    MDataHandle inputValue(const MObject& attr, MStatus* status) {
        *status = api("input " + attr.name);
        return MDataHandle(bool(*status), &inputs.at(attr.name));
    }
    MArrayDataHandle inputArrayValue(const MObject&, MStatus* status) {
        *status = api("edits.handle");
        return MArrayDataHandle(bool(*status));
    }
    MDataHandle outputValue(const MObject&, MStatus* status) {
        *status = api("output.handle");
        return MDataHandle(bool(*status));
    }
    MStatus setClean(const MPlug&) {
        const auto status = api("output.clean");
        if (status)
            ++cleanCalls;
        return status;
    }
};
namespace MSpace {
enum Space { kObject, kWorld };
}
struct MTransformationMatrix {
    enum RotationOrder { kXYZ };
    MMatrix components;
    MTransformationMatrix() = default;
    explicit MTransformationMatrix(const MMatrix& matrix) : components(matrix) {}
    MStatus getScale(double3& values, MSpace::Space) const {
        const auto status = api("transform.getScale");
        if (status)
            std::copy(components[0], components[0] + 3, values);
        return status;
    }
    MStatus getRotation(double3& values, RotationOrder& order, MSpace::Space) const {
        const auto status = api("transform.getRotation");
        if (status) {
            std::copy(components[1], components[1] + 3, values);
            order = kXYZ;
        }
        return status;
    }
    MVector getTranslation(MSpace::Space, MStatus* status) const {
        *status = api("transform.getTranslation");
        return {components[2][0], components[2][1], components[2][2]};
    }
    MStatus setScale(const double3& values, MSpace::Space) {
        const auto status = api("transform.setScale");
        if (status)
            std::copy(values, values + 3, components[0]);
        return status;
    }
    MStatus setRotation(const double3& values, RotationOrder, MSpace::Space) {
        const auto status = api("transform.setRotation");
        if (status)
            std::copy(values, values + 3, components[1]);
        return status;
    }
    MStatus setTranslation(const MVector& value, MSpace::Space) {
        const auto status = api("transform.setTranslation");
        if (status) {
            components[2][0] = value.x;
            components[2][1] = value.y;
            components[2][2] = value.z;
        }
        return status;
    }
    MMatrix asMatrix() const {
        return components;
    }
};

struct ReviewTime : MTime {
    using MTime::MTime;
    static Unit uiUnit() {
        return kFilm;
    }
};
#define MTime ReviewTime
struct MDGContext {
    MTime time;
    explicit MDGContext(const MTime& time) : time(time) {}
};
struct MDGContextGuard {
    double previous;
    explicit MDGContextGuard(const MDGContext& context) : previous(currentContext) {
        currentContext = context.time.value();
    }
    ~MDGContextGuard() {
        currentContext = previous;
    }
};
namespace dommy {
namespace maya {
inline MStatus readTimeOffsetAndStretch(MDataBlock&, const MObject&, const MObject&, const MObject&,
                                        std::size_t count, MDoubleArray& offsets,
                                        MDoubleArray& stretches) {
    const auto status = api("time.arrays");
    if (status) {
        offsets.values.assign(count, 1.0);
        stretches.values.assign(count, 2.0);
    }
    return status;
}
inline MStatus getEvaluationTime(MDataBlock&, MTime& time) {
    const auto status = api("time.current");
    if (status)
        time = MTime(12);
    return status;
}
struct ScopedPlugDataHandle {
    MDataHandle value;
    MDataHandle& handle() {
        return value;
    }
    MStatus read(const MPlug&) {
        const auto status = api("time.sample");
        value = MDataHandle(bool(status), &sampledInput);
        if (status) {
            sampledTimes.push_back(currentContext);
            sampledInput.vector[0] = static_cast<float>(currentContext);
            sampledInput.vector[1] = 2;
            sampledInput.vector[2] = 3;
        }
        return status;
    }
};
} // namespace maya
} // namespace dommy

MStatus getFloatArrayFromData(MDataBlock&, const MObject&, MFloatArray& values) {
    const auto status = api("float.input");
    if (status)
        values.values = inputFloats;
    return status;
}
struct MFnFloatArrayData {
    MObject create(const MFloatArray& values, MStatus* status) {
        *status = api("float.create");
        if (*status)
            for (float value : values.values)
                published.push_back({value});
        return {"floatData"};
    }
};
struct MFnMatrixArrayData {
    MObject create(const MMatrixArray& values, MStatus* status) {
        *status = api("matrix.create");
        if (*status)
            for (const auto& matrix : values.values) {
                std::vector<double> components;
                for (unsigned int row = 0; row < 3; ++row)
                    components.insert(components.end(), matrix[row], matrix[row] + 3);
                published.push_back(components);
            }
        return {"matrixData"};
    }
};
template <class T>
MStatus publishTriples(MDataBlock& data, const MPlug& plug, const Array<T>& values) {
    const auto status = api("triples.publish");
    if (!status)
        return status;
    for (const auto& value : values.values)
        published.push_back({value.x, value.y, value.z});
    ++publications;
    return data.setClean(plug);
}
MStatus DOutputDataMayaPointArray(MDataBlock& data, const MPlug& plug, const MObject&,
                                  const MPointArray& values) {
    return publishTriples(data, plug, values);
}
MStatus DOutputDataMayaVectorArray(MDataBlock& data, const MPlug& plug, const MObject&,
                                   const MVectorArray& values) {
    return publishTriples(data, plug, values);
}

struct DPVectorArrayGen : AttributeNode {
    static MObject generateType, aValueType, value, valueSize, isNormalize, seed, minRange,
        maxRange, aGlobalOffset, aInOffsetArray, aInStretchArray, outDataArray;
    static MStatus initialize();
    MStatus compute(const MPlug&, MDataBlock&);
    MObject thisMObject() const {
        return {"vectorNode"};
    }
};
struct DPMatrix : AttributeNode {
    static MObject inMatrix, dataSize, seed, translateNoise, rotateNoise, scaleNoise,
        outMatrixArray;
    static MStatus initialize();
    MStatus compute(const MPlug&, MDataBlock&);
};
struct DPEditDouble : AttributeNode {
    static MObject inputDataArray, isUseExpression, expression, exValue, editDataArray, dataIndex,
        dataValue, outDataArray;
    static MStatus initialize();
    MStatus compute(const MPlug&, MDataBlock&);
    MObject thisMObject() const {
        return {"editNode"};
    }
};
#include "array_node_methods.inc"

namespace {
MStatus vectorCompute(MDataBlock& data) {
    DPVectorArrayGen node;
    return node.compute(MPlug(node.thisMObject(), DPVectorArrayGen::outDataArray), data);
}
MStatus matrixCompute(MDataBlock& data) {
    DPMatrix node;
    return node.compute(MPlug(MObject::kNullObj, DPMatrix::outMatrixArray), data);
}
MStatus editCompute(MDataBlock& data) {
    DPEditDouble node;
    return node.compute(MPlug(node.thisMObject(), DPEditDouble::outDataArray), data);
}
MDataBlock vectorData(int mode = 0, int type = 0) {
    MDataBlock data;
    for (const auto& name : {"generateType", "ValueType", "value", "valueSize", "isNormalize",
                             "seed", "minRange", "maxRange", "GlobalOffset"})
        data.inputs[name] = Input();
    data.inputs["generateType"].number = mode;
    data.inputs["ValueType"].number = type;
    data.inputs["valueSize"].number = 4;
    data.inputs["seed"].number = 7;
    data.inputs["value"].vector[0] = 3;
    data.inputs["value"].vector[1] = 4;
    for (int axis = 0; axis < 3; ++axis) {
        data.inputs["minRange"].vector[axis] = -1;
        data.inputs["maxRange"].vector[axis] = 2;
    }
    data.inputs["GlobalOffset"].number = 0.5;
    return data;
}
MDataBlock matrixData() {
    MDataBlock data;
    for (const auto& name :
         {"dataSize", "seed", "inMatrix", "translateNoise", "rotateNoise", "scaleNoise"})
        data.inputs[name] = Input();
    data.inputs["dataSize"].number = 3;
    data.inputs["seed"].number = 19;
    for (int axis = 0; axis < 3; ++axis) {
        data.inputs["inMatrix"].matrix[0][axis] = 2 + axis;
        data.inputs["inMatrix"].matrix[1][axis] = 0.1 * axis;
        data.inputs["inMatrix"].matrix[2][axis] = 5 + axis;
        data.inputs["translateNoise"].precise[axis] = axis == 1 ? 0 : -(axis + 1.0);
        data.inputs["rotateNoise"].precise[axis] = 10 * (axis + 1.0);
        data.inputs["scaleNoise"].precise[axis] = 0.25 * (axis + 1.0);
    }
    return data;
}
MDataBlock editData(bool expression = true) {
    MDataBlock data;
    data.inputs["isUseExpression"].number = expression;
    data.inputs["expression"].string = MString("0;2-4");
    data.inputs["exValue"].number = 8;
    edits.clear();
    Edit edit;
    edit.logicalIndex = 100;
    edit.index.number = 2;
    edit.value.number = 9;
    edits.push_back(edit);
    edit.logicalIndex = 200;
    edit.index.number = 2;
    edit.value.number = 10;
    edits.push_back(edit);
    inputFloats = {1, 2, 3, 4};
    return data;
}

void successful(MStatus (*compute)(MDataBlock&), MDataBlock& data) {
    resetRun();
    assert(compute(data) == MS::kSuccess);
    assert(publications == 1 && cleanCalls == 1 && readsAfterFault == 0 && currentContext == 9.0);
}

int testFailures(MStatus (*compute)(MDataBlock&), MDataBlock data) {
    successful(compute, data);
    const auto baseline = operations;
    const auto expected = published;
    int arraySites = 0;
    for (MS::MStatusCode failureStatus : {MS::kFailure, MS::kInvalidParameter}) {
        injectedStatus = failureStatus;
        for (int failure = 0; failure < static_cast<int>(baseline.size()); ++failure) {
            resetRun(failure);
            assert(compute(data) == failureStatus);
            assert(callCount == failure + 1 && cleanCalls == 0 && readsAfterFault == 0 &&
                   currentContext == 9.0);
            if (baseline[failure] != "output.clean")
                assert(publications == 0);
            successful(compute, data);
            assert(published == expected);
        }
    }
    for (int failure = 0; failure < static_cast<int>(baseline.size()); ++failure) {
        if (baseline[failure].find(".allocate") == std::string::npos &&
            baseline[failure] != "float.copy")
            continue;
        ++arraySites;
        resetRun();
        shortAt = failure;
        assert(compute(data) == MS::kFailure);
        assert(callCount == failure + 1 && publications == 0 && cleanCalls == 0 &&
               readsAfterFault == 0);
        resetRun();
        throwAt = failure;
        assert(compute(data) == MS::kFailure);
        assert(callCount == failure + 1 && publications == 0 && cleanCalls == 0 &&
               readsAfterFault == 0);
        successful(compute, data);
        assert(published == expected);
    }
    assert(arraySites > 0);
    return static_cast<int>(baseline.size()) * 2;
}

void testNormalPaths() {
    parentConnected = childConnected = false;
    for (int type : {0, 1}) {
        auto data = vectorData(0, type);
        successful(vectorCompute, data);
        assert(published == std::vector<std::vector<double>>(4, {3, 4, 0}));
        data.inputs["isNormalize"].number = 1;
        successful(vectorCompute, data);
        assert(published == std::vector<std::vector<double>>(4, {.6, .8, 0}));
        for (int seed : {7, -1}) {
            data = vectorData(1, type);
            data.inputs["seed"].number = seed;
            successful(vectorCompute, data);
            for (unsigned int axis = 0; axis < 3; ++axis) {
                std::mt19937 engine(static_cast<std::uint32_t>(seed) + axis);
                std::uniform_real_distribution<float> distribution(-1, 2);
                for (unsigned int i = 0; i < 4; ++i)
                    assert(published[i][axis] == distribution(engine));
            }
        }
        data = vectorData(2, type);
        successful(vectorCompute, data);
        assert(sampledTimes.empty() && published[0][0] == 3);
        for (bool child : {false, true}) {
            parentConnected = !child;
            childConnected = child;
            successful(vectorCompute, data);
            assert(sampledTimes == std::vector<double>(4, 5.5));
            assert(published == std::vector<std::vector<double>>(4, {5.5, 2, 3}));
        }
        parentConnected = childConnected = false;
        data.inputs["valueSize"].number = 0;
        successful(vectorCompute, data);
        assert(published.empty());
    }
    for (int seed : {19, -1}) {
        auto data = matrixData();
        data.inputs["seed"].number = seed;
        successful(matrixCompute, data);
        std::mt19937 engine(static_cast<std::uint32_t>(seed));
        for (unsigned int i = 0; i < 3; ++i) {
            double noise[3][3];
            for (unsigned int component = 0; component < 3; ++component) {
                const char* name = component == 0   ? "translateNoise"
                                   : component == 1 ? "rotateNoise"
                                                    : "scaleNoise";
                for (unsigned int axis = 0; axis < 3; ++axis) {
                    double amplitude = std::fabs(data.inputs[name].precise[axis]);
                    noise[component][axis] =
                        amplitude == 0
                            ? 0
                            : std::uniform_real_distribution<double>(-amplitude, amplitude)(engine);
                }
            }
            for (unsigned int axis = 0; axis < 3; ++axis) {
                assert(published[i][axis] == 2 + axis + noise[2][axis]);
                assert(published[i][3 + axis] ==
                       0.1 * axis + dommy::math::degreesToRadians(noise[1][axis]));
                assert(published[i][6 + axis] == 5 + axis + noise[0][axis]);
            }
        }
        data.inputs["dataSize"].number = 0;
        successful(matrixCompute, data);
        assert(published.empty());
    }
    auto data = editData();
    successful(editCompute, data);
    assert(published == std::vector<std::vector<double>>({{8}, {2}, {10}, {8}}));
    assert(inputFloats == std::vector<float>({1, 2, 3, 4}));
    data = editData(false);
    edits.clear();
    successful(editCompute, data);
    assert(published == std::vector<std::vector<double>>({{1}, {2}, {3}, {4}}));
}

void testInvalidInputs() {
    auto data = vectorData();
    data.inputs["valueSize"].number = -1;
    resetRun();
    assert(vectorCompute(data) == MS::kInvalidParameter && publications == 0);
    data.inputs["valueSize"].number = 10000001;
    resetRun();
    assert(vectorCompute(data) == MS::kInvalidParameter && publications == 0);
    data = vectorData();
    data.inputs["value"].vector[0] = std::numeric_limits<float>::infinity();
    resetRun();
    assert(vectorCompute(data) == MS::kInvalidParameter && publications == 0);
    data = vectorData(2);
    parentConnected = true;
    data.inputs["GlobalOffset"].number = std::numeric_limits<double>::quiet_NaN();
    resetRun();
    assert(vectorCompute(data) == MS::kInvalidParameter && publications == 0);
    parentConnected = false;
    data = matrixData();
    data.inputs["dataSize"].number = 1000001;
    resetRun();
    assert(matrixCompute(data) == MS::kInvalidParameter && publications == 0);
    data = matrixData();
    data.inputs["rotateNoise"].precise[1] = std::numeric_limits<double>::infinity();
    resetRun();
    assert(matrixCompute(data) == MS::kInvalidParameter && publications == 0);
    data = editData();
    edits[0].index.number = -1;
    resetRun();
    assert(editCompute(data) == MS::kInvalidParameter && publications == 0);
    data = editData();
    data.inputs["exValue"].number = std::numeric_limits<double>::max();
    resetRun();
    assert(editCompute(data) == MS::kInvalidParameter && publications == 0);
    data = editData();
    data.inputs["expression"].string = MString("!");
    resetRun();
    assert(editCompute(data) == MS::kInvalidParameter && publications == 0);
    data = editData();
    inputFloats.clear();
    resetRun();
    assert(editCompute(data) == MS::kFailure && publications == 0);
    DPVectorArrayGen vectorNode;
    DPMatrix matrixNode;
    DPEditDouble editNode;
    const MPlug unknown(MObject::kNullObj, MObject{"unknown"});
    resetRun();
    assert(vectorNode.compute(unknown, data) == MS::kUnknownParameter);
    assert(matrixNode.compute(unknown, data) == MS::kUnknownParameter);
    assert(editNode.compute(unknown, data) == MS::kUnknownParameter && callCount == 0);
}
} // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string trace = schemaTrace("DPVectorArrayGen", DPVectorArrayGen::initialize) +
                              schemaTrace("DPMatrix", DPMatrix::initialize) +
                              schemaTrace("DPEditDouble", DPEditDouble::initialize);
    if (std::string(argv[1]) == "--print-schema") {
        std::cout << trace;
        return 0;
    }
    testNormalPaths();
    if (std::string(argv[1]) == "--normal-only") {
        std::cout << "Normal array-node paths: OK\n";
        return 0;
    }
    std::ifstream fixture(argv[1]);
    assert(fixture);
    const std::string expected((std::istreambuf_iterator<char>(fixture)),
                               std::istreambuf_iterator<char>());
    assert(trace == expected);
    const int initializerFailures =
        testFailures("DPVectorArrayGen", DPVectorArrayGen::initialize, 64) +
        testFailures("DPMatrix", DPMatrix::initialize, 40) +
        testFailures("DPEditDouble", DPEditDouble::initialize, 45);
    int computeFailures = 0;
    for (int type : {0, 1}) {
        computeFailures += testFailures(vectorCompute, vectorData(0, type));
        computeFailures += testFailures(vectorCompute, vectorData(1, type));
        parentConnected = true;
        computeFailures += testFailures(vectorCompute, vectorData(2, type));
        parentConnected = false;
        childConnected = true;
        computeFailures += testFailures(vectorCompute, vectorData(2, type));
        childConnected = false;
    }
    computeFailures += testFailures(matrixCompute, matrixData());
    computeFailures += testFailures(editCompute, editData());
    auto noEdits = editData(false);
    edits.clear();
    computeFailures += testFailures(editCompute, noEdits);
    testInvalidInputs();
    testNormalPaths();
    std::cout << initializerFailures << " initializer failures; " << computeFailures
              << " compute failures; checked arrays, unchanged normal paths and recovery: OK\n";
}
