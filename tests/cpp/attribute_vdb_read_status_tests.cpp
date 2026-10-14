#include <maya/MStatus.h>
#include <maya/MTime.h>
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/file/DSequencePath.h"
#include "common/utility/DNumericValidation.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// API/IO seams. No native Maya or OpenVDB serialization is performed.
int callCount = 0, failAt = -1, throwAt = -1, invalidAccesses = 0;
int publications = 0, handleCleans = 0, blockCleans = 0, splitCalls = 0;
bool exists = true, sizeAllowed = true, rejectData = false, insertThrows = false;
bool nullCreate = false, wrongCreateType = false;
int decodeMode = 0;
std::size_t forcedNameCount = 0;
MS::MStatusCode injectedStatus = MS::kFailure;
std::vector<std::string> operations;
MStatus operation(const std::string& name) {
    operations.push_back(name);
    const int index = callCount++;
    if (index == throwAt) throw std::runtime_error("injected API exception");
    return index == failAt ? injectedStatus : MS::kSuccess;
}
void hostVoid(const char* name) { if (!operation(name)) throw std::runtime_error(name); }
// These geometry values implement only the APIs exercised by the production
// finite/normalization helpers. Native Maya signatures are checked separately.
struct MVector {
    double x, y, z;
    MVector(double a = 0, double b = 0, double c = 0) : x(a), y(b), z(c) {}
    double length() const { return std::sqrt(x*x + y*y + z*z); }
    MVector operator/(double scale) const { return {x/scale, y/scale, z/scale}; }
    MVector operator*(double scale) const { return {x*scale, y*scale, z*scale}; }
    double operator*(const MVector& right) const { return x*right.x + y*right.y + z*right.z; }
    static const MVector zero;
};
const MVector MVector::zero;
struct MPoint {
    double x, y, z, w;
    MPoint(double a = 0, double b = 0, double c = 0) : x(a), y(b), z(c), w(1) {}
    MVector operator-(const MPoint& right) const { return {x-right.x, y-right.y, z-right.z}; }
    MPoint& operator+=(const MVector& right) { x += right.x; y += right.y; z += right.z; return *this; }
};
struct MPointArray : std::vector<MPoint> {
    using std::vector<MPoint>::operator=;
    unsigned int length() const { return size(); }
};
// The duplicater admission prefix only constructs empty local scratch arrays.
using MFloatPointArray = std::vector<MPoint>;
using MFloatArray = std::vector<float>;
namespace MFnData { enum Type { kInvalid, kMesh }; }
namespace MSpace { enum Space { kObject }; }
const double kMFnMeshPointTolerance = 1e-6;
using double3 = double[3];
struct MStringArray;
struct MString {
    std::string value;
    MString(const char* text = "") : value(text) {}
    const char* asChar() const { return value.c_str(); }
    unsigned int length() const { return static_cast<unsigned int>(value.size()); }
    void clear() { value.clear(); }
    MString operator+(const char* right) const { return MString((value + right).c_str()); }
    MStatus split(char delimiter, MStringArray& result) const;
};
struct MStringArray {
    std::vector<MString> values;
    unsigned int length() const { return static_cast<unsigned int>(forcedNameCount ? forcedNameCount : values.size()); }
    const MString& operator[](std::size_t index) const { return values.at(index); }
};
MStatus MString::split(char delimiter, MStringArray& result) const {
    ++splitCalls;
    const auto status = operation("names.split");
    if (!status) return status;
    std::istringstream input(value);
    std::string token;
    while (std::getline(input, token, delimiter)) if (!token.empty()) result.values.emplace_back(token.c_str());
    return status;
}
struct MGlobal {
    static void displayError(const MString&) {}
    static void displayWarning(const MString&) {}
};
struct MFloatVector { float x = 0, y = 0, z = 0; };
struct Payload { virtual ~Payload() = default; };
using MTypeId = int;
struct MObject {
    std::string name;
    std::shared_ptr<Payload> payload;
    explicit MObject(const char* text = "") : name(text) {}
    bool isNull() const { return !payload; }
    bool operator==(const MObject& right) const { return name == right.name; }
};
struct MPlug {
    MObject attr;
    MPlug(const MObject& value) : attr(value) {}
    MObject attribute() const { return attr; }
    bool operator==(const MObject& value) const { return attr == value; }
    bool operator!=(const MObject& value) const { return !(*this == value); }
};
struct DPData : Payload {
    static const int id = 1;
    bool valid = true;
    std::map<std::string, int> attributes;
    std::map<std::string, int> groups;
    bool hasValidPointTopology() const { return valid; }
    void setDPData(const DPData& input) { hostVoid("point.copy"); attributes = input.attributes; groups = input.groups; valid = input.valid; }
    bool isExistAttr(const std::string& name) const { return attributes.count(name) != 0; }
    void removeAttr(const std::string& name) { hostVoid("attribute.remove"); attributes.erase(name); }
    bool isExistGrp(const std::string& name) const { return groups.count(name) != 0; }
    void removeGrp(const std::string& name) { hostVoid("group.remove"); groups.erase(name); }
};
// Geometry queries are explicit seams, not a model of Maya intersection math.
MPointArray meshHits;
MVector meshNormal;
struct MFnMesh {
    MFnMesh(const MObject&, MStatus* status) { *status = operation("mesh.bind"); }
    bool intersect(const MPoint&, const MVector&, MPointArray& result, double,
                   MSpace::Space, void*, MStatus* status) {
        *status = operation("mesh.intersect"); result = meshHits; return !result.empty();
    }
    MStatus getClosestNormal(const MPoint&, MVector& result, MSpace::Space) {
        result = meshNormal; return operation("mesh.normal");
    }
};
namespace openvdb {
using GridPtrVec = std::vector<int>;
using GridPtrVecPtr = std::shared_ptr<GridPtrVec>;
struct Exception : std::runtime_error { using std::runtime_error::runtime_error; };
namespace io {
struct Stream {
    explicit Stream(std::istream&) {}
    GridPtrVecPtr getGrids() const {
        if (decodeMode == 1) throw Exception("decoder error");
        if (decodeMode == 2) throw std::runtime_error("decoder error");
        if (decodeMode == 3) throw 42;
        if (decodeMode == 4) return {};
        return std::make_shared<GridPtrVec>(GridPtrVec{7, 8});
    }
};
}
}
struct DOpenVDBData : Payload {
    static const int id = 2;
    std::vector<int> grids;
    bool hasValidData() const { return !rejectData; }
    void insert(const openvdb::GridPtrVec& input) {
        if (insertThrows) throw std::length_error("grid budget");
        grids = input;
    }
};
const int DPData::id;
const int DOpenVDBData::id;
struct Input {
    double number = 0;
    MString text;
    MVector vector;
    MFloatVector color;
    MObject object;
    double3 triple = {0, 0, 0};
    MFnData::Type type = MFnData::kMesh;
};
struct Output { MObject object; };
struct MDataHandle {
    Input* input = nullptr;
    Output* output = nullptr;
    bool valid = false;
    MVector* numericOutput = nullptr;
    MDataHandle(Input* in = nullptr, Output* out = nullptr, bool available = false)
        : input(in), output(out), valid(available) {}
    void check() const { if (!valid) ++invalidAccesses; }
    MString asString() const { check(); return input->text; }
    bool asBool() const { check(); return input->number != 0; }
    double asDouble() const { check(); return input->number; }
    float asFloat() const { check(); return static_cast<float>(input->number); }
    const double3& asDouble3() const { check(); return input->triple; }
    MFnData::Type type() const { check(); return input->type; }
    MObject data() const { check(); return input->object; }
    MObject asMesh() const { return data(); }
    int asInt() const { check(); return static_cast<int>(input->number); }
    short asShort() const { check(); return static_cast<short>(input->number); }
    MTime asTime() const { check(); return MTime(input->number); }
    MVector asVector() const { check(); return input->vector; }
    MFloatVector asFloatVector() const { check(); return input->color; }
    MStatus set(const MObject& object) {
        check(); const auto status = operation("output.set");
        if (status) { output->object = object; ++publications; }
        return status;
    }
    void set(double x, double y, double z) { check(); *numericOutput = MVector(x, y, z); ++publications; }
    void setClean() { check(); ++handleCleans; }
};
struct MDataBlock {
    std::map<std::string, Input> inputs;
    Output output;
    std::map<std::string, MVector> numericOutputs;
    MDataHandle inputValue(const MObject& attribute, MStatus* status) {
        *status = operation("input " + attribute.name);
        return {&inputs[attribute.name], nullptr, bool(*status)};
    }
    MDataHandle outputValue(const MObject& attribute, MStatus* status) {
        if (attribute.name == "outputPosition" || attribute.name == "outputNormal") {
            *status = operation("output " + attribute.name);
            MDataHandle handle(nullptr, nullptr, bool(*status));
            handle.numericOutput = &numericOutputs[attribute.name];
            return handle;
        }
        *status = operation("output.handle");
        return {nullptr, &output, bool(*status)};
    }
    MStatus setClean(const MPlug&) { const auto status = operation("output.clean"); if (status) ++blockCleans; return status; }
};
struct MFnPluginData {
    MObject data;
    MObject create(int id, MStatus* status) {
        *status = operation("payload.create");
        if (*status && !nullCreate) {
            if (id == DPData::id && !wrongCreateType) data.payload = std::make_shared<DPData>();
            else data.payload = std::make_shared<DOpenVDBData>();
        }
        return data;
    }
};
struct MFileObject {
    MStatus setRawFullName(const MString&) { return operation("file.name"); }
    bool exists() const { return ::exists; }
};
const DPData* getInputDP(const MObject& attr, MDataBlock& block) {
    MStatus status; auto handle = block.inputValue(attr, &status);
    return status ? dynamic_cast<DPData*>(handle.input->object.payload.get()) : nullptr;
}
namespace dommy {
namespace maya {
template<class Data> MStatus getPluginData(MFnPluginData& fn, Data*& data) {
    const auto status = operation("payload.bind");
    if (!status) return status;
    data = dynamic_cast<Data*>(fn.data.payload.get());
    return data ? MS::kSuccess : MS::kFailure;
}
template<class Data> MStatus getPluginDataFromHandle(const MDataHandle& handle, int, const Data*& data) {
    handle.check(); const auto status = operation("input.bind");
    if (!status) return status;
    data = dynamic_cast<const Data*>(handle.input->object.payload.get());
    return data ? MS::kSuccess : MS::kFailure;
}
template<class Data> MStatus getPluginDataFromObject(const MObject& object, int, const Data*& data) {
    const auto status = operation("output.bind");
    if (!status) return status;
    data = dynamic_cast<const Data*>(object.payload.get());
    return data ? MS::kSuccess : MS::kFailure;
}
bool validatePluginData(const DPData& data) { return data.hasValidPointTopology(); }
bool validatePluginData(const DOpenVDBData& data) { return data.hasValidData(); }
}
namespace attribute { bool validAttributeName(const std::string& name) { return !name.empty(); } }
namespace file { bool fileSizeWithinLimit(const std::string&, unsigned long long) { return sizeAllowed; } }
namespace vdb { const std::size_t kMaxVdbPayloadBytes = 1024; }
}
#define CHECK_MSTATUS_AND_RETURN_IT(status) DOMMY_RETURN_IF_FAILURE(status)
#define DOMMY_COMPUTE_CATCH(name) catch (...) { return MS::kFailure; }
#include "attribute_vdb_read_status.inc"

void reset(int failure = -1) {
    callCount = invalidAccesses = publications = handleCleans = blockCleans = splitCalls = 0;
    failAt = failure; throwAt = -1;
    exists = sizeAllowed = true; rejectData = insertThrows = false; decodeMode = 0; forcedNameCount = 0;
    nullCreate = wrongCreateType = false;
    operations.clear();
    meshHits = {MPoint(0, 0, 5), MPoint(0, 0, 2)}; meshNormal = MVector(0, 0, 2);
}
MDataBlock deletionBlock(bool enabled = true) {
    MDataBlock block;
    auto input = std::make_shared<DPData>();
    input->attributes = {{"keep", 1}, {"remove", 2}, {"remove2", 3}};
    block.inputs["inDP"].object.payload = input;
    block.inputs["attributeName"].text = "remove missing remove2 remove";
    block.inputs["isEnable"].number = enabled;
    auto previous = std::make_shared<DPData>(); previous->attributes["previous"] = 42;
    block.output.object.payload = previous;
    return block;
}
MDataBlock readerBlock(const char* filename) {
    MDataBlock block;
    block.inputs["aVdbFilePath"].text = filename;
    auto previous = std::make_shared<DOpenVDBData>(); previous->grids.push_back(42);
    block.output.object.payload = previous;
    return block;
}
template<class Node> void failures(Node& node, const MObject& plug, const MDataBlock& baseline) {
    auto block = baseline; reset(); assert(node.compute(plug, block));
    assert(invalidAccesses == 0 && publications == 1 && handleCleans == 1 && blockCleans == 1);
    const auto trace = operations;
    for (std::size_t index = 0; index < trace.size(); ++index) {
        for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
            block = baseline; const auto previous = block.output.object.payload;
            reset(static_cast<int>(index)); injectedStatus = code;
            const auto result = node.compute(plug, block);
            // Legacy void mutations cannot propagate an API status; they throw.
            const bool voidMutation = trace[index] == "point.copy" || trace[index] == "attribute.remove" || trace[index] == "group.remove";
            assert(result == (voidMutation ? MS::kFailure : code));
            assert(invalidAccesses == 0 && callCount == static_cast<int>(index) + 1);
            if (trace[index] != "output.clean") {
                assert(block.output.object.payload == previous && publications == 0 && handleCleans == 0);
            } else { assert(publications == 1); } // No rollback promise after commit.
        }
        block = baseline; const auto previous = block.output.object.payload;
        reset(); throwAt = static_cast<int>(index);
        assert(node.compute(plug, block) == MS::kFailure && invalidAccesses == 0);
        if (trace[index] != "output.clean") assert(block.output.object.payload == previous && publications == 0);
    }
    block = baseline; reset(); assert(node.compute(plug, block));
    reset(); assert(node.compute(MObject{"other"}, block) == MS::kUnknownParameter && callCount == 0);
}
template<class Node> void admission(Node& node, const MObject& plug, MDataBlock block, int expected, int firstFailure = 1) {
    reset(); assert(node.compute(plug, block)); assert(callCount == expected && publications == 0);
    for (int index = firstFailure; index < expected; ++index) {
        for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
            reset(index); injectedStatus = code;
            assert(node.compute(plug, block) == code);
            assert(invalidAccesses == 0 && publications == 0 && callCount == index + 1);
        }
        reset(); throwAt = index;
        assert(node.compute(plug, block) == MS::kFailure && invalidAccesses == 0 && publications == 0);
    }
}
MDataBlock groupDeletionBlock(bool enabled = true) {
    auto block = deletionBlock();
    auto input = std::dynamic_pointer_cast<DPData>(block.inputs["inDP"].object.payload);
    input->groups = {{"keep", 1}, {"remove", 2}, {"remove2", 3}};
    block.inputs["aGroupName"].text = "remove missing remove2 remove";
    block.inputs["aIsEnable"].number = enabled;
    return block;
}
void groupDeletionTests() {
    DPGroupDelete node;
    failures(node, DPGroupDelete::outDP, groupDeletionBlock());
    failures(node, DPGroupDelete::outDP, groupDeletionBlock(false));
    auto block = groupDeletionBlock(); reset();
    assert(node.compute(DPGroupDelete::outDP, block));
    const auto output = std::dynamic_pointer_cast<DPData>(block.output.object.payload);
    const auto source = std::dynamic_pointer_cast<DPData>(block.inputs["inDP"].object.payload);
    assert(output->groups.size() == 1 && output->isExistGrp("keep"));
    assert(source->groups.size() == 3 && output->attributes == source->attributes);
    for (bool enabled : {false, true}) {
        block = groupDeletionBlock(enabled);
        if (enabled) block.inputs["aGroupName"].text = "";
        reset(); assert(node.compute(DPGroupDelete::outDP, block));
        assert(splitCalls == 0 && std::dynamic_pointer_cast<DPData>(block.output.object.payload)->groups.size() == 3);
    }
    for (int mode = 0; mode < 5; ++mode) {
        block = groupDeletionBlock(); const auto previous = block.output.object.payload; reset();
        if (mode == 0) forcedNameCount = dommy::generation::kMaxGeneratedStringCount + 1;
        if (mode == 1) std::dynamic_pointer_cast<DPData>(block.inputs["inDP"].object.payload)->valid = false;
        if (mode == 2) block.inputs["inDP"].object.payload.reset();
        if (mode == 3) nullCreate = true;
        if (mode == 4) wrongCreateType = true;
        assert(node.compute(DPGroupDelete::outDP, block) == (mode < 2 ? MS::kInvalidParameter : MS::kFailure));
        assert(block.output.object.payload == previous && publications == 0 && handleCleans == 0 && invalidAccesses == 0);
    }
}
MDataBlock surfaceBlock() {
    MDataBlock block;
    block.inputs["inputMesh"].object.payload = std::make_shared<Payload>();
    block.inputs["targetDirection"].triple[2] = 10;
    block.inputs["offset"].number = 0.5;
    block.numericOutputs["outputPosition"] = MVector(91, 92, 93);
    block.numericOutputs["outputNormal"] = MVector(81, 82, 83);
    return block;
}
bool sameVector(const MVector& first, const MVector& second) {
    return first.x == second.x && first.y == second.y && first.z == second.z;
}
void assertSurfaceUnchanged(const MDataBlock& block) {
    assert(sameVector(block.numericOutputs.at("outputPosition"), MVector(91, 92, 93)));
    assert(sameVector(block.numericOutputs.at("outputNormal"), MVector(81, 82, 83)));
    assert(publications == 0 && handleCleans == 0 && invalidAccesses == 0);
}
void surfaceTests() {
    DPointToSurfaceTransformNode node;
    const MObject requests[] = {DPointToSurfaceTransformNode::outputPosition,
        DPointToSurfaceTransformNode::outputPositionX, DPointToSurfaceTransformNode::outputPositionY,
        DPointToSurfaceTransformNode::outputPositionZ, DPointToSurfaceTransformNode::outputNormal,
        DPointToSurfaceTransformNode::outputNormalX, DPointToSurfaceTransformNode::outputNormalY,
        DPointToSurfaceTransformNode::outputNormalZ};
    for (const auto& request : requests) {
        auto block = surfaceBlock(); reset(); assert(node.compute(request, block));
        assert(sameVector(block.numericOutputs.at("outputPosition"), MVector(0, 0, 2.5)));
        assert(sameVector(block.numericOutputs.at("outputNormal"), MVector(0, 0, 1)));
        assert(publications == 2 && handleCleans == 2 && invalidAccesses == 0);
        const int count = callCount;
        // All operations in this trace precede the void output setters. The
        // contract covers handle acquisition, not rollback of native setters.
        for (int index = 0; index < count; ++index) {
            for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
                block = surfaceBlock(); reset(index); injectedStatus = code;
                assert(node.compute(request, block) == code && callCount == index + 1);
                assertSurfaceUnchanged(block);
            }
            block = surfaceBlock(); reset(); throwAt = index;
            assert(node.compute(request, block) == MS::kFailure); assertSurfaceUnchanged(block);
        }
        block = surfaceBlock(); reset(); assert(node.compute(request, block));
    }
    for (int mode = 0; mode < 7; ++mode) {
        auto block = surfaceBlock(); reset();
        const double nan = std::numeric_limits<double>::quiet_NaN();
        if (mode == 0) block.inputs["inputMesh"].type = MFnData::kInvalid;
        if (mode == 1) block.inputs["inputMesh"].object.payload.reset();
        if (mode == 2) block.inputs["inputPosition"].triple[0] = nan;
        if (mode == 3) block.inputs["targetDirection"].triple[1] = nan;
        if (mode == 4) block.inputs["offset"].number = std::numeric_limits<float>::infinity();
        if (mode == 5) meshHits = {MPoint(nan, 0, 0)};
        if (mode == 6) meshNormal = MVector::zero;
        assert(node.compute(requests[0], block) == (mode < 5 ? MS::kInvalidParameter : MS::kFailure));
        assertSurfaceUnchanged(block);
    }
    for (bool zeroRay : {false, true}) {
        auto block = surfaceBlock(); reset();
        if (zeroRay) block.inputs["targetDirection"].triple[2] = 0;
        else meshHits.clear();
        assert(node.compute(requests[0], block));
        assert(sameVector(block.numericOutputs.at("outputPosition"), MVector::zero));
        assert(sameVector(block.numericOutputs.at("outputNormal"), MVector::zero));
    }
    auto block = surfaceBlock(); reset();
    assert(node.compute(MObject("other"), block) == MS::kUnknownParameter && callCount == 0);
    assertSurfaceUnchanged(block);
    polyBlastNode blast;
    block = MDataBlock(); block.inputs["inMesh"].object.payload = std::make_shared<Payload>();
    admission(blast, polyBlastNode::outMesh, block, 3, 0);
    surfaceDuplicater duplicater;
    admission(duplicater, surfaceDuplicater::outPutMesh, MDataBlock(), 32, 0);
}
int main(int argc, char** argv) {
    assert(argc == 2);
    DPAttributeDelete deletion; DReadOpenVDB reader;
    failures(deletion, DPAttributeDelete::outDP, deletionBlock());
    failures(deletion, DPAttributeDelete::outDP, deletionBlock(false));
    failures(reader, DReadOpenVDB::aVdbOutput, readerBlock(argv[1]));
    auto block = deletionBlock(); reset(); assert(deletion.compute(DPAttributeDelete::outDP, block));
    const auto output = std::dynamic_pointer_cast<DPData>(block.output.object.payload);
    const auto source = std::dynamic_pointer_cast<DPData>(block.inputs["inDP"].object.payload);
    assert(output->attributes.size() == 1 && output->isExistAttr("keep"));
    assert(source->attributes.size() == 3);
    for (bool empty : {false, true}) {
        block = deletionBlock(false); if (empty) block.inputs["attributeName"].text = "";
        reset(); assert(deletion.compute(DPAttributeDelete::outDP, block));
        assert(splitCalls == 0 && std::dynamic_pointer_cast<DPData>(block.output.object.payload)->attributes.size() == 3);
    }
    block = deletionBlock(); auto previous = block.output.object.payload;
    reset(); forcedNameCount = dommy::generation::kMaxGeneratedStringCount + 1;
    assert(deletion.compute(DPAttributeDelete::outDP, block) == MS::kInvalidParameter);
    assert(block.output.object.payload == previous && publications == 0);
    block = deletionBlock(); std::dynamic_pointer_cast<DPData>(block.inputs["inDP"].object.payload)->valid = false;
    reset(); assert(deletion.compute(DPAttributeDelete::outDP, block) == MS::kInvalidParameter && publications == 0);
    block = deletionBlock(); block.inputs["inDP"].object.payload.reset();
    reset(); assert(deletion.compute(DPAttributeDelete::outDP, block) == MS::kFailure && publications == 0);
    for (bool wrongType : {false, true}) {
        block = deletionBlock(); previous = block.output.object.payload;
        reset(); nullCreate = !wrongType; wrongCreateType = wrongType;
        assert(deletion.compute(DPAttributeDelete::outDP, block) == MS::kFailure);
        assert(block.output.object.payload == previous && publications == 0);
    }

    block = readerBlock(argv[1]); reset(); assert(reader.compute(DReadOpenVDB::aVdbOutput, block));
    assert(std::dynamic_pointer_cast<DOpenVDBData>(block.output.object.payload)->grids == std::vector<int>({7, 8}));
    for (const char* bad : {"archive.$F33.vdb", "archive.$F9999999999999999999.vdb"}) {
        block = readerBlock(bad); previous = block.output.object.payload;
        reset(); assert(reader.compute(DReadOpenVDB::aVdbOutput, block) == MS::kFailure);
        assert(block.output.object.payload == previous && publications == 0 && handleCleans == 0);
    }
    for (int mode = 0; mode < 9; ++mode) {
        block = readerBlock(argv[1]); reset();
        if (mode == 0) block.inputs["aVdbFilePath"].text = "";
        if (mode == 1) exists = false;
        if (mode == 2) sizeAllowed = false;
        if (mode == 3) block.inputs["aVdbFilePath"].text = "test-seam-missing-reader-file.vdb";
        if (mode >= 4 && mode <= 7) decodeMode = mode - 3;
        if (mode == 8) insertThrows = true;
        assert(reader.compute(DReadOpenVDB::aVdbOutput, block));
        assert(publications == 1 && handleCleans == 1 && blockCleans == 1 && invalidAccesses == 0);
        assert(std::dynamic_pointer_cast<DOpenVDBData>(block.output.object.payload)->grids.empty());
    }
    for (double time : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        block = readerBlock(argv[1]); previous = block.output.object.payload;
        block.inputs["aTime"].number = time; reset();
        assert(reader.compute(DReadOpenVDB::aVdbOutput, block) == MS::kFailure);
        assert(block.output.object.payload == previous && publications == 0 && handleCleans == 0);
    }
    block = readerBlock(argv[1]); previous = block.output.object.payload;
    reset(); rejectData = true;
    assert(reader.compute(DReadOpenVDB::aVdbOutput, block) == MS::kInvalidParameter);
    assert(block.output.object.payload == previous && publications == 0);
    DPAttributeBlend blend; block = deletionBlock(); admission(blend, DPAttributeBlend::outDP, block, 8, 0);
    DPAttributeSeExpr expression; MDataBlock controls;
    controls.inputs["aAttributeName"].text = "result"; controls.inputs["aExpression"].text = "1";
    admission(expression, DPAttributeSeExpr::outDD, controls, 15);
    reset(0); assert(expression.compute(DPAttributeSeExpr::outDD, controls) == injectedStatus && invalidAccesses == 0);
    groupDeletionTests(); surfaceTests();
    std::cout << "Attribute/group Delete, VDB reader and surface complete computes: admission, publication, API failures and recovery; Blend/SeExpr/polyBlast/surfaceDuplicater control reads passed (Maya/OpenVDB/mesh seams)\n";
}
