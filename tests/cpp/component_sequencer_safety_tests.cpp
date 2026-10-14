#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DIndexPattern.h"
#include "common/utility/DStatusOperation.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

bool operator!(MStatus status) {
    return status != MS::kSuccess;
}
#define CHECK_MSTATUS_AND_RETURN_IT(status) DOMMY_RETURN_IF_FAILURE(status)

void EchoWarning(const char*, const char*) {}

namespace MFn {
enum Type { kMeshVertComponent, kMeshEdgeComponent, kMeshPolygonComponent, kMeshMapComponent };
}

// Model an internal-data array reference so the input-data branch is tested
// without assuming that Maya assignment creates an independently owned copy.
struct MIntArray {
    std::shared_ptr<std::vector<int>> values = std::make_shared<std::vector<int>>();
    unsigned int reportedLength = 0;
    unsigned int length() const {
        return reportedLength ? reportedLength : static_cast<unsigned int>(values->size());
    }
    int operator[](unsigned int index) const {
        return values->at(index);
    }
    MStatus append(int value) {
        const MStatus status = operation("array.append");
        if (status == MS::kSuccess)
            values->push_back(value);
        return status;
    }
};

namespace {
MIntArray sourceIndices;
bool connected = true, throwCount = false;
std::string nullFactory, copiedSource, copiedKind;
std::vector<int> componentIndices;
MFn::Type componentType = MFn::kMeshVertComponent;
int copies = 0, componentAdds = 0;

MObject factory(const std::string& name, MStatus* status) {
    *status = operation(name);
    return *status == MS::kSuccess && nullFactory != name ? MObject{name} : MObject::kNullObj;
}
} // namespace

struct MFnStringData {
    MObject create(const char* value, MStatus* status) {
        assert(std::string(value).empty());
        return factory("string.create", status);
    }
};
struct MFnIntArrayData {
    bool valid;
    MFnIntArrayData(const MObject& object, MStatus* status) {
        assert(object.name == "input.indices");
        *status = operation("array.bind");
        valid = *status == MS::kSuccess;
    }
    MIntArray array(MStatus* status) const {
        assert(valid);
        *status = operation("array.read");
        return sourceIndices;
    }
};
struct MFnSingleIndexedComponent {
    bool valid = false;
    MObject create(MFn::Type type, MStatus* status) {
        const MObject object = factory("component.create", status);
        valid = !object.isNull();
        if (valid)
            componentType = type;
        return object;
    }
    MStatus addElements(const MIntArray& elements) {
        assert(valid);
        const MStatus status = operation("component.elements");
        if (status == MS::kSuccess)
            componentIndices = *elements.values;
        return status;
    }
};
struct MFnComponentListData {
    bool valid = false;
    MObject create(MStatus* status) {
        const MObject object = factory("componentList.create", status);
        valid = !object.isNull();
        return object;
    }
    MStatus add(const MObject& object) {
        assert(valid && object.name == "component.create");
        const MStatus status = operation("componentList.add");
        if (status == MS::kSuccess)
            ++componentAdds;
        return status;
    }
};

struct GeometryData {
    std::string kind;
    explicit GeometryData(const char* kind) : kind(kind) {}
    MObject create(MStatus* status) {
        return factory(kind + ".data", status);
    }
};
struct GeometryCopy {
    std::string kind;
    explicit GeometryCopy(const char* kind) : kind(kind) {}
    MObject copy(const MObject& source, MObject& parent, MStatus* status) {
        assert(!source.isNull() && parent.name == kind + ".data");
        const MObject result = factory(kind + ".copy", status);
        if (*status == MS::kSuccess) {
            copiedSource = source.name;
            copiedKind = kind;
            ++copies;
        }
        return result;
    }
};
struct MFnMeshData : GeometryData {
    MFnMeshData() : GeometryData("mesh") {}
};
struct MFnNurbsCurveData : GeometryData {
    MFnNurbsCurveData() : GeometryData("curve") {}
};
struct MFnNurbsSurfaceData : GeometryData {
    MFnNurbsSurfaceData() : GeometryData("surface") {}
};
struct MFnMesh : GeometryCopy {
    MFnMesh() : GeometryCopy("mesh") {}
};
struct MFnNurbsCurve : GeometryCopy {
    MFnNurbsCurve() : GeometryCopy("curve") {}
};
struct MFnNurbsSurface : GeometryCopy {
    MFnNurbsSurface() : GeometryCopy("surface") {}
};

struct MPlug {
    std::string name;
    MPlug(const char* name) : name(name) {}
    MPlug(const MObject&, const MObject& attribute) : name(attribute.name) {}
    bool operator==(const MObject& attribute) const {
        return name == attribute.name;
    }
    bool operator!=(const MObject& attribute) const {
        return name != attribute.name;
    }
    bool isConnected(MStatus* status = nullptr) const {
        assert(name == "inputDataArray");
        const MStatus result = operation("connected");
        if (status) *status = result;
        return result == MS::kSuccess && connected;
    }
};
struct MDataBlock;
struct MDataHandle {
    bool valid;
    double value;
    MString string;
    MObject object;
    MFnData::Type dataType;
    MDataBlock* output;
    short asShort() const {
        assert(valid);
        return static_cast<short>(value);
    }
    int asInt() const {
        assert(valid);
        return static_cast<int>(value);
    }
    double asDouble() const {
        assert(valid);
        return value;
    }
    MString asString() const {
        assert(valid);
        return string;
    }
    MObject data() const {
        assert(valid);
        return object;
    }
    MFnData::Type type() const {
        assert(valid);
        return dataType;
    }
    MStatus set(const MObject& value);
    void setClean();
};
struct Entry {
    unsigned int logicalIndex;
    MObject object;
    MFnData::Type type;
};
struct MArrayDataHandle {
    bool valid;
    const std::vector<Entry>* entries;
    unsigned int countOverride;
    unsigned int physicalIndex = 0;
    bool positioned = false;
    MArrayDataHandle(bool valid, const std::vector<Entry>* entries, unsigned int countOverride)
        : valid(valid), entries(entries), countOverride(countOverride) {}
    unsigned int elementCount(MStatus* status = nullptr) const {
        assert(valid);
        if (throwCount)
            throw std::runtime_error("injected count exception");
        const MStatus result = operation("input.count");
        if (status)
            *status = result;
        // A failed result must not be used, irrespective of the returned count.
        // This nonzero sentinel does not predict Maya's actual failure value.
        return countOverride ? countOverride : static_cast<unsigned int>(entries->size());
    }
    MStatus jumpToArrayElement(unsigned int index) {
        assert(valid && index < entries->size());
        const MStatus status = operation("input.jump " + text(index));
        positioned = status == MS::kSuccess;
        if (positioned)
            physicalIndex = index;
        return status;
    }
    MDataHandle inputValue(MStatus* status) const {
        assert(valid && positioned);
        const Entry& entry = entries->at(physicalIndex);
        *status = operation("input.element " + text(entry.logicalIndex));
        return {*status == MS::kSuccess, 0, {}, entry.object, entry.type, nullptr};
    }
};
struct MDataBlock {
    short component = 0, mode = 0;
    int size = 4;
    MString pattern = "4;2-5 7";
    double sequence = 0.5;
    MFnData::Type arrayType = MFnData::kIntArray;
    std::vector<Entry> entries = {{2, {"first"}, MFnData::kMesh},
                                  {9, {"second"}, MFnData::kMesh},
                                  {101, {"third"}, MFnData::kMesh},
                                  {10000, {"fourth"}, MFnData::kMesh}};
    unsigned int countOverride = 0;
    MObject published = {"oldOutput"};
    int publications = 0;
    bool clean = false, handleClean = false;
    MDataHandle inputValue(const MObject& attribute, MStatus* status) {
        *status = operation("input " + attribute.name);
        double value = 0;
        if (attribute.name == "ComponentType")
            value = component;
        else if (attribute.name == "Mode")
            value = mode;
        else if (attribute.name == "Size")
            value = size;
        else if (attribute.name == "seaquence")
            value = sequence;
        else
            assert(attribute.name == "Pattern" || attribute.name == "inputDataArray");
        return {*status == MS::kSuccess, value, pattern, {"input.indices"}, arrayType, nullptr};
    }
    MArrayDataHandle inputArrayValue(const MObject& attribute, MStatus* status) {
        assert(attribute.name == "inputObjects");
        *status = operation("input.array");
        return {*status == MS::kSuccess, &entries, countOverride};
    }
    MDataHandle outputValue(const MObject& attribute, MStatus* status) {
        assert(attribute.name == "outComponentList" || attribute.name == "outputObject");
        *status = operation("output.handle");
        return {*status == MS::kSuccess, 0, {}, {}, MFnData::kInvalid, this};
    }
    MStatus setClean(const MPlug&) {
        assert(handleClean && publications == 1);
        const MStatus status = operation("output.clean");
        if (status == MS::kSuccess)
            clean = true;
        return status;
    }
};
MStatus MDataHandle::set(const MObject& value) {
    assert(valid && output && !value.isNull());
    const MStatus status = operation("output.set");
    if (status == MS::kSuccess) {
        output->published = value;
        ++output->publications;
    }
    return status;
}
void MDataHandle::setClean() {
    assert(valid && output && output->publications == 1);
    output->handleClean = true;
}

struct DComponentListGen : AttributeNode {
    static MObject aComponentType, aMode, aSize, aPattern, inputDataArray, outComponentList;
    static MStatus initialize();
    MStatus compute(const MPlug&, MDataBlock&);
    MObject thisMObject() const {
        return {"componentNode"};
    }
    MString name() const {
        return "componentNode";
    }
};
struct DShapeSequencer : AttributeNode {
    static MObject inputObjects, seaquence, outputObject;
    static MStatus initialize();
    MStatus compute(const MPlug&, MDataBlock&);
};

#include "component_sequencer_methods.inc"

namespace {
void testInitialization(const char* fixture) {
    std::ifstream input(fixture);
    assert(input.good());
    std::ostringstream expected;
    expected << input.rdbuf();
    const std::string actual = schemaTrace("DComponentListGen", DComponentListGen::initialize) +
                               schemaTrace("DShapeSequencer", DShapeSequencer::initialize);
    if (actual != expected.str()) {
        std::cerr << "Component/Sequencer declaration trace changed:\n" << actual;
        std::abort();
    }
    assert(testFailures("DComponentListGen", DComponentListGen::initialize, 38) == 76);
    assert(testFailures("DShapeSequencer", DShapeSequencer::initialize, 24) == 48);
}
void resetCompute(int failure = -1) {
    resetCalls(failure);
    sourceIndices = MIntArray();
    *sourceIndices.values = {8, 2, 8, 0};
    connected = true;
    throwCount = false;
    nullFactory.clear();
    copiedSource.clear();
    copiedKind.clear();
    componentIndices.clear();
    copies = componentAdds = 0;
}
void assertUnpublished(const MDataBlock& block) {
    assert(block.publications == 0 && block.published.name == "oldOutput" && !block.clean &&
           !block.handleClean);
}
template <typename Node>
int testFailuresAndRecovery(const char* output, const MDataBlock& initial) {
    Node node;
    resetCompute();
    auto block = initial;
    assert(node.compute({"unrelated"}, block) == MS::kUnknownParameter && callCount == 0);
    assertUnpublished(block);
    assert(node.compute({output}, block) == MS::kSuccess && block.clean && block.publications == 1);
    const auto baseline = operations;
    const int count = callCount;
    for (MS::MStatusCode status : {MS::kFailure, MS::kInvalidParameter}) {
        injectedStatus = status;
        for (int failure = 0; failure < count; ++failure) {
            resetCompute(failure);
            block = initial;
            assert(node.compute({output}, block) == status);
            assert(callCount == failure + 1);
            assert(std::equal(operations.begin(), operations.end(), baseline.begin()));
            if (failure == count - 1)
                assert(block.publications == 1 && block.handleClean && !block.clean);
            else
                assertUnpublished(block);
            resetCompute();
            block = initial;
            assert(node.compute({output}, block) == MS::kSuccess && block.clean);
            assert(operations == baseline);
        }
    }
    return count * 2;
}
void testComponentSelections() {
    DComponentListGen node;
    const MFn::Type expectedTypes[] = {MFn::kMeshVertComponent, MFn::kMeshEdgeComponent,
                                       MFn::kMeshPolygonComponent, MFn::kMeshMapComponent};
    const std::vector<std::vector<int>> expected = {{0, 1, 2, 3}, {4, 2, 3, 4, 7}, {8, 2, 8, 0}};
    for (short type = 0; type < 4; ++type) {
        for (short mode = 0; mode < 3; ++mode) {
            resetCompute();
            MDataBlock block;
            block.component = type;
            block.mode = mode;
            const auto originalIndices = *sourceIndices.values;
            assert(node.compute({"outComponentList"}, block) == MS::kSuccess);
            assert(componentType == expectedTypes[type] && componentIndices == expected[mode]);
            assert(componentAdds == 1 && block.clean && block.publications == 1);
            assert(block.published.name == "componentList.create");
            assert(*sourceIndices.values == originalIndices);
        }
    }
    for (const auto& pattern : std::vector<std::pair<const char*, std::vector<int>>>{
             {"", {}},
             {"5-2; 7 7", {2, 3, 4, 7, 7}},
             {"2147483646-2147483649 2147483648", {2147483646, 2147483647}}}) {
        resetCompute();
        MDataBlock block;
        block.mode = 1;
        block.pattern = pattern.first;
        assert(node.compute({"outComponentList"}, block) == MS::kSuccess);
        assert(componentIndices == pattern.second);
    }
    for (short mode : {0, 2}) {
        resetCompute();
        MDataBlock block;
        block.mode = mode;
        block.size = 0;
        sourceIndices.values->clear();
        assert(node.compute({"outComponentList"}, block) == MS::kSuccess);
        assert(componentIndices.empty() && componentAdds == 1);
    }
}
void testComponentInvalidInputs() {
    DComponentListGen node;
    for (int scenario = 0; scenario < 13; ++scenario) {
        resetCompute();
        MDataBlock block;
        if (scenario == 0)
            block.component = -1;
        if (scenario == 1)
            block.component = 4;
        if (scenario == 2)
            block.mode = -1;
        if (scenario == 3)
            block.mode = 3;
        if (scenario == 4)
            block.size = -1;
        if (scenario == 5)
            block.size = dommy::generation::kMaxGeneratedPointCount + 1;
        if (scenario >= 6 && scenario <= 9)
            block.mode = 2;
        if (scenario == 6)
            connected = false;
        if (scenario == 7)
            block.arrayType = MFnData::kMesh;
        if (scenario == 8)
            sourceIndices.reportedLength = dommy::generation::kMaxGeneratedPointCount + 1;
        if (scenario == 9)
            sourceIndices.values->at(2) = -1;
        if (scenario == 10)
            nullFactory = "componentList.create";
        if (scenario == 11)
            nullFactory = "component.create";
        if (scenario == 12) {
            block.mode = 1;
            block.pattern = "0-10000001";
        }
        const MS::MStatusCode expected = scenario == 6 || scenario == 10 || scenario == 11
                                             ? MS::kFailure
                                             : MS::kInvalidParameter;
        assert(node.compute({"outComponentList"}, block) == expected);
        assertUnpublished(block);
        assert(componentAdds == 0);
    }
    for (const char* pattern : {"! 1", "-1", "1-", "1-2-3", "x", "184467440737095516160"}) {
        resetCompute();
        MDataBlock block;
        block.mode = 1;
        block.pattern = pattern;
        assert(node.compute({"outComponentList"}, block) == MS::kInvalidParameter);
        assertUnpublished(block);
        assert(componentAdds == 0);
    }
}
void testSequencerSelections() {
    DShapeSequencer node;
    // Explicit expectations cover the two-input half threshold and the
    // three/four-input nearest-index behavior, including tie rounding.
    const std::vector<double> controls = {0, 0.125, 0.25, 0.499, 0.5, 0.75, 0.875, 1};
    const int expected[][8] = {{0, 0, 0, 0, 0, 0, 0, 0},
                               {0, 0, 0, 0, 1, 1, 1, 1},
                               {0, 0, 1, 1, 1, 2, 2, 2},
                               {0, 0, 1, 1, 2, 2, 3, 3}};
    for (const auto& type :
         {std::make_pair(MFnData::kMesh, "mesh"), std::make_pair(MFnData::kNurbsCurve, "curve"),
          std::make_pair(MFnData::kNurbsSurface, "surface")}) {
        for (std::size_t count = 1; count <= 4; ++count) {
            for (std::size_t index = 0; index < controls.size(); ++index) {
                resetCompute();
                MDataBlock block;
                block.entries.resize(count);
                for (auto& entry : block.entries)
                    entry.type = type.first;
                block.sequence = controls[index];
                assert(node.compute({"outputObject"}, block) == MS::kSuccess);
                assert(copiedSource == block.entries[expected[count - 1][index]].object.name);
                assert(copiedKind == type.second && copies == 1);
                assert(block.published.name == copiedKind + ".data" && block.clean);
            }
        }
    }
}
void testSequencerInvalidInputs() {
    DShapeSequencer node;
    for (double value :
         {-0.01, 1.01, std::numeric_limits<double>::quiet_NaN(),
          std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        resetCompute();
        MDataBlock block;
        block.sequence = value;
        assert(node.compute({"outputObject"}, block) == MS::kFailure);
        assert(callCount == 1);
        assertUnpublished(block);
    }
    for (int scenario = 0; scenario < 6; ++scenario) {
        resetCompute();
        MDataBlock block;
        if (scenario == 0)
            block.entries.clear();
        if (scenario == 1)
            block.countOverride = dommy::generation::kMaxConnectedGeometryCount + 1;
        if (scenario == 2)
            block.entries[2].object = MObject::kNullObj;
        if (scenario == 3)
            block.entries[2].type = MFnData::kNurbsCurve;
        if (scenario == 4)
            for (auto& entry : block.entries)
                entry.type = MFnData::kIntArray;
        if (scenario == 5)
            throwCount = true;
        assert(node.compute({"outputObject"}, block) ==
               (scenario == 2 || scenario == 3 ? MS::kInvalidParameter : MS::kFailure));
        assertUnpublished(block);
        assert(copies == 0);
    }
    resetCompute();
    MDataBlock block;
    assert(node.compute({"outputObject"}, block) == MS::kSuccess && block.clean);
}
} // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    testInitialization(argv[1]);
    int failures = 0;
    for (short type = 0; type < 4; ++type) {
        for (short mode = 0; mode < 3; ++mode) {
            MDataBlock block;
            block.component = type;
            block.mode = mode;
            failures += testFailuresAndRecovery<DComponentListGen>("outComponentList", block);
        }
    }
    for (MFnData::Type type : {MFnData::kMesh, MFnData::kNurbsCurve, MFnData::kNurbsSurface}) {
        MDataBlock block;
        for (auto& entry : block.entries)
            entry.type = type;
        failures += testFailuresAndRecovery<DShapeSequencer>("outputObject", block);
    }
    testComponentSelections();
    testComponentInvalidInputs();
    testSequencerSelections();
    testSequencerInvalidInputs();
    std::cout << "Component/Sequencer safety passed: 124 initializer failures, " << failures
              << " compute failures, selection contracts, invalid inputs and recovery\n";
}
