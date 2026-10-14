#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributeValidation.h"
#include "common/utility/DAttributeValueConversion.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DAttributePublication.h"
#include "common/utility/DAttributeCreatePlan.h"
#include <maya/MTime.h>
#include <algorithm>
#include <iostream>
#include <map>
#include <set>

using float3 = float[3];
using DStringList = std::vector<std::string>;
using MVector = Imath::V3d;
using MPoint = Imath::V3d;
using DPoint = Imath::V3f;
template <typename T> struct HostArray : std::vector<T> {
    using std::vector<T>::vector;
    unsigned int length() const { return static_cast<unsigned int>(this->size()); }
    MStatus setLength(unsigned int count) {
        const auto status = operation("array.resize"); if (status) this->resize(count); return status;
    }
};
using MVectorArray = HostArray<MVector>;
using MPointArray = HostArray<MPoint>;
using MFloatArray = HostArray<float>;
using MDoubleArray = HostArray<double>;
using MIntArray = HostArray<int>;
using MStringArray = HostArray<MString>;
using MMatrixArray = HostArray<MMatrix>;
int publications = 0, cleans = 0, numericWrites = 0;
std::set<std::string> connections;
bool nullCurve = false, rejectFinal = false, rejectAdd = false;
bool nullTypedInput = false, wrongTypedInput = false, invalidTypedTopology = false;
bool invalidExpression = false, nullNativeArray = false;
std::size_t reportedArrayCount = 0U;
void hostVoid(const char* name) {
    if (!operation(name)) throw std::runtime_error("injected host failure");
}
struct CreateGroup { const std::vector<bool>& getGroupIds() const { return ids; } std::vector<bool> ids; };
using CreateGroups = std::map<std::string, std::shared_ptr<CreateGroup>>;
struct Owner {
    DPAttrList attributes;
    std::size_t count = 2;
    std::size_t numberOfPoints() const { return count; }
    DPAttrIter beginAttr() { return attributes.begin(); }
    DPAttrIter endAttr() { return attributes.end(); }
    ConstDPAttrIter beginAttr() const { return attributes.begin(); }
    ConstDPAttrIter endAttr() const { return attributes.end(); }
    CreateGroups::const_iterator beginGrp() const { return groups.begin(); }
    CreateGroups::const_iterator endGrp() const { return groups.end(); }
    CreateGroups groups;
    bool isExistAttr(const std::string& name) const { return attributes.count(name) != 0; }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        const auto it = attributes.find(name);
        return it == attributes.end() ? nullptr : it->second;
    }
    void removeAttr(const std::string& name) { hostVoid("attribute.remove"); attributes.erase(name); }
    void addAttr(const std::string& name, const DPAttributeData& field) {
        hostVoid("attribute.add");
        const auto previous = getAttr(name);
        if (!rejectAdd && (!previous || previous->typeName() == field.typeName()))
            attributes[name] = field.copy();
    }
    void copyAttributes(const Owner& other) {
        attributes.clear(); count = other.count;
        for (const auto& item : other.attributes) attributes[item.first] = item.second->copy();
    }
    bool valid(std::size_t expected) const {
        dommy::attribute::ValidationBudget budget;
        return dommy::attribute::validateAttributeList(attributes, expected, budget);
    }
};
struct DPData : Owner {
    static const int id = 22;
    bool hasValidPointTopology() const { return !invalidTypedTopology && valid(count); }
    void setDPData(const DPData& other) { hostVoid("point.copy"); copyAttributes(other); }
};
struct Primitive : Owner {
    std::size_t numberOfKnots() const { return 0U; }
    int numFace() const { return 0; }
    std::size_t numberOfConnections() const { return 0U; }
    std::size_t numberOfUVs() const { return 0U; }
    DPAttrList faceAttributes, cornerAttributes;
    ConstDPAttrIter beginFaceAttr() const { return faceAttributes.begin(); }
    ConstDPAttrIter endFaceAttr() const { return faceAttributes.end(); }
    ConstDPAttrIter beginPVtxAttr() const { return cornerAttributes.begin(); }
    ConstDPAttrIter endPVtxAttr() const { return cornerAttributes.end(); }
    CreateGroups::const_iterator beginFaceGrp() const { return groups.begin(); }
    CreateGroups::const_iterator endFaceGrp() const { return groups.end(); }
    CreateGroups::const_iterator beginEdgeGrp() const { return groups.begin(); }
    CreateGroups::const_iterator endEdgeGrp() const { return groups.end(); }
};
template <int Id> struct Collection : Owner {
    static const int id = Id;
    std::vector<std::shared_ptr<Primitive>> primitives;
    auto beginCurves() -> decltype(primitives.begin()) { return primitives.begin(); }
    auto endCurves() -> decltype(primitives.end()) { return primitives.end(); }
    auto beginMeshes() -> decltype(primitives.begin()) { return primitives.begin(); }
    auto endMeshes() -> decltype(primitives.end()) { return primitives.end(); }
    auto beginCurves() const -> decltype(primitives.begin()) { return primitives.begin(); }
    auto endCurves() const -> decltype(primitives.end()) { return primitives.end(); }
    auto beginMeshes() const -> decltype(primitives.begin()) { return primitives.begin(); }
    auto endMeshes() const -> decltype(primitives.end()) { return primitives.end(); }
    auto beginPrims() const -> decltype(primitives.begin()) { return primitives.begin(); }
    auto endPrims() const -> decltype(primitives.end()) { return primitives.end(); }
    auto beginPrims() -> decltype(primitives.begin()) { return primitives.begin(); }
    auto endPrims() -> decltype(primitives.end()) { return primitives.end(); }
    std::size_t numberOfCurves() const { return primitives.size(); }
    std::size_t numberOfMesh() const { return primitives.size(); }
    std::size_t getTotalCvs() const {
        std::size_t result = 0;
        for (const auto& primitive : primitives) result += primitive->numberOfPoints();
        return result;
    }
    void getTotalVertex(std::size_t& result) const { result = getTotalCvs(); }
    MStatus copyFrom(const Collection& other) {
        const auto status = operation("collection.copy");
        if (!status) return status;
        copyAttributes(other); primitives.clear();
        for (const auto& primitive : other.primitives) {
            auto copy = std::make_shared<Primitive>();
            copy->copyAttributes(*primitive); primitives.push_back(copy);
        }
        return status;
    }
    bool valid() const {
        if (!Owner::valid(primitives.size())) return false;
        for (const auto& primitive : primitives)
            if (!primitive || !primitive->valid(primitive->numberOfPoints())) return false;
        return true;
    }
    bool hasValidCurveTopology() const { return !invalidTypedTopology && valid(); }
    bool hasValidMeshTopology() const { return !invalidTypedTopology && valid(); }
};
using DCurveData = Collection<23>;
using DMeshData = Collection<24>;
DPData inputPoints, stagedPoints, outputPoints;
DCurveData inputCurves, stagedCurves, outputCurves;
DMeshData inputMeshes, stagedMeshes, outputMeshes;
template <int Id> Collection<Id>& publishedCollection();
template <> DCurveData& publishedCollection<23>() { return outputCurves; }
template <> DMeshData& publishedCollection<24>() { return outputMeshes; }
struct Input {
    double number = 0;
    float3 triple = {0, 0, 0};
    std::string text = "created";
    MFnData::Type type = MFnData::kInvalid;
    MFnNumericData::Type numericType = MFnNumericData::kDouble;
    bool generic = false;
    int typedId = DPData::id;
    std::map<std::string, Input> children;
};
struct MDataHandle {
    Input* input = nullptr;
    bool valid = false;
    MDataHandle(Input* value = nullptr, bool available = false) : input(value), valid(available) {}
    void check() const { assert(valid); }
    MDataHandle child(const MObject& attr) { check(); return {&input->children[attr.name], true}; }
    short asShort() const { check(); return static_cast<short>(input->number); }
    int asInt() const { check(); return static_cast<int>(input->number); }
    float3& asFloat3() const { check(); return input->triple; }
    MString asString() const { check(); return input->text.c_str(); }
    double asDouble() const { check(); assert(!input->generic); return input->number; }
    double asGenericDouble() const { check(); assert(input->generic); return input->number; }
    bool asBool() const { check(); return input->number != 0; }
    unsigned char asUChar() const { check(); return static_cast<unsigned char>(input->number); }
    char asChar() const { check(); return static_cast<char>(input->number); }
    std::int64_t asInt64() const { check(); return static_cast<std::int64_t>(input->number); }
    float asFloat() const { check(); return static_cast<float>(input->number); }
    MMatrix asMatrix() const { check(); return {}; }
    MVector asVector() const { check(); return {input->triple[0], input->triple[1], input->triple[2]}; }
    Imath::V3f asFloatVector() const { check(); return {input->triple[0], input->triple[1], input->triple[2]}; }
    int typeId() const { check(); return input->typedId; }
    MFnData::Type type() const { check(); return input->type; }
    MFnNumericData::Type numericType() const { check(); return input->numericType; }
    bool isGeneric(bool& numeric, bool& null) const {
        check(); numeric = true; null = false; return input->generic;
    }
    MObject data() const { check(); return nullCurve ? MObject::kNullObj : MObject{input->text}; }
    void set(bool) { check(); ++numericWrites; }
    MStatus set(const MObject&) {
        check(); const auto status = operation("array.publish");
        if (status) ++publications; return status;
    }
    void setClean() { check(); hostVoid("array.clean"); }
};
struct MPlug {
    MObject attr;
    MPlug(const MObject& value) : attr(value) {}
    MPlug(const MObject&, const MObject& value) : attr(value) {}
    bool operator==(const MObject& other) const { return attr.name == other.name; }
    bool operator!=(const MObject& other) const { return !(*this == other); }
    operator const MObject&() const { return attr; }
    bool isConnected(MStatus* status = nullptr) const {
        const auto result = operation("connection.query");
        if (status) *status = result;
        return connections.count(attr.name) != 0;
    }
};
struct MArrayDataHandle {
    std::vector<Input>* elements = nullptr;
    unsigned int index = 0;
    bool valid = false;
    MArrayDataHandle(std::vector<Input>* values, unsigned int ordinal, bool available)
        : elements(values), index(ordinal), valid(available) {}
    unsigned int elementCount(MStatus* status = nullptr) {
        assert(valid); const auto result = operation("array.count");
        if (status) *status = result;
        return static_cast<unsigned int>(elements->size());
    }
    MStatus jumpToArrayElement(unsigned int physical) {
        assert(valid); const auto result = operation("array.jump");
        if (result) { assert(physical < elements->size()); index = physical; }
        return result;
    }
    MDataHandle inputValue(MStatus* status) {
        assert(valid); *status = operation("array.input");
        return {&elements->at(index), bool(*status)};
    }
};
struct MDataBlock {
    std::map<std::string, Input> inputs;
    std::vector<Input> elements;
    MDataHandle inputValue(const MObject& attr, MStatus* status = nullptr) {
        const auto result = operation("input " + attr.name);
        if (status) *status = result;
        return {&inputs[attr.name], bool(result)};
    }
    MDataHandle outputValue(const MObject& attr, MStatus* status = nullptr) {
        const auto result = operation("output.handle");
        if (status) *status = result;
        return {&inputs[attr.name], bool(result)};
    }
    MArrayDataHandle inputArrayValue(const MObject&, MStatus* status) {
        *status = operation("array.handle"); return {&elements, 0, bool(*status)};
    }
    MStatus setClean(const MPlug&) {
        const auto status = operation("output.clean"); if (status) ++cleans; return status;
    }
};
MVectorArray vectors;
MPointArray points;
MFloatArray floats;
MDoubleArray doubles;
MIntArray integers;
MStringArray strings;
MMatrixArray matrices;
template <typename Array, Array* Values> struct HostFunctionSet {
    bool valid;
    static Array produced;
    HostFunctionSet() : valid(true) {}
    HostFunctionSet(const MObject&, MStatus* status) {
        *status = operation("array.bind"); valid = bool(*status);
    }
    Array array(MStatus* status) const {
        assert(valid); *status = operation("array.read"); return *Values;
    }
    unsigned int length(MStatus* status) const {
        assert(valid); *status = operation("array.length");
        return static_cast<unsigned int>(reportedArrayCount ? reportedArrayCount : Values->size());
    }
    const typename Array::value_type& operator[](unsigned int index) const {
        assert(valid); hostVoid("array.element"); return Values->at(index);
    }
    MObject create(const Array& values, MStatus* status) {
        *status = operation("array.create");
        if (*status) produced = values;
        return nullNativeArray ? MObject::kNullObj : MObject{"array"};
    }
};
template <typename Array, Array* Values> Array HostFunctionSet<Array, Values>::produced;
using MFnVectorArrayData = HostFunctionSet<MVectorArray, &vectors>;
using MFnPointArrayData = HostFunctionSet<MPointArray, &points>;
using MFnFloatArrayData = HostFunctionSet<MFloatArray, &floats>;
using MFnDoubleArrayData = HostFunctionSet<MDoubleArray, &doubles>;
using MFnIntArrayData = HostFunctionSet<MIntArray, &integers>;
using MFnStringArrayData = HostFunctionSet<MStringArray, &strings>;
using MFnMatrixArrayData = HostFunctionSet<MMatrixArray, &matrices>;
template <typename Array, typename T> void DScalarMayaArrayToVectorList(const Array& in, std::vector<T>& out) {
    out.assign(in.begin(), in.end());
}
struct MFnPluginData { MObject object() const { return {"staged"}; } };
namespace dommy { namespace maya {
inline bool isFiniteVector(const MVector& value) {
    return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}
inline bool isFloatRepresentableVector(const MVector& value) {
    return isFiniteVector(value) && std::fabs(value[0]) <= std::numeric_limits<float>::max() &&
        std::fabs(value[1]) <= std::numeric_limits<float>::max() &&
        std::fabs(value[2]) <= std::numeric_limits<float>::max();
}
inline MStatus getEvaluationTime(MDataBlock&, MTime&) { return operation("time.read"); }
template <typename DataType> const DataType* createInput();
template <> const DPData* createInput<DPData>() { return &inputPoints; }
template <> const DCurveData* createInput<DCurveData>() { return &inputCurves; }
template <> const DMeshData* createInput<DMeshData>() { return &inputMeshes; }
template <typename DataType>
MStatus getPluginDataFromHandle(const MDataHandle& handle, int, const DataType*& out) {
    out = nullptr; handle.check();
    const auto status = operation("input.bind");
    if (!status) return status;
    if (wrongTypedInput) return MS::kFailure;
    out = nullTypedInput ? nullptr : createInput<DataType>();
    return MS::kSuccess;
}
inline MStatus createPluginData(MFnPluginData&, int, DPData*& out) {
    const auto status = operation("output.create"); if (status) out = &stagedPoints; return status;
}
inline MStatus createPluginData(MFnPluginData&, int, DCurveData*& out) {
    const auto status = operation("output.create"); if (status) out = &stagedCurves; return status;
}
inline MStatus createPluginData(MFnPluginData&, int, DMeshData*& out) {
    const auto status = operation("output.create"); if (status) out = &stagedMeshes; return status;
}
inline MStatus publishPluginDataObject(MDataHandle& handle, const MObject&, const DPData& data) {
    handle.check(); if (rejectFinal || !data.valid(data.numberOfPoints())) return MS::kInvalidParameter;
    const auto status = operation("output.publish");
    if (status) { outputPoints = data; ++publications; } return status;
}
template <int Id> MStatus publishPluginDataObject(MDataHandle& handle, const MObject&, const Collection<Id>& data) {
    handle.check(); if (rejectFinal || !data.valid()) return MS::kInvalidParameter;
    const auto status = operation("output.publish");
    if (status) { publishedCollection<Id>() = data; ++publications; } return status;
}
template <typename DataType, typename Populate>
MStatus setPluginDataOutput(MDataBlock& block, const MObject& attr, int id, Populate populate) {
    MFnPluginData creator; DataType* output = nullptr;
    DOMMY_RETURN_IF_FAILURE(createPluginData(creator, id, output));
    DOMMY_RETURN_IF_FAILURE(populate(*output));
    MStatus status; auto handle = block.outputValue(attr, &status);
    DOMMY_RETURN_IF_FAILURE(status);
    return publishPluginDataObject(handle, creator.object(), *output);
}
}}
using MFloatVector = Imath::V3f;
using MTypeId = int;
struct DPointSeExpr {
    explicit DPointSeExpr(const std::string&) {}
    bool isValid() const { return !invalidExpression; }
};
const DPData* getInputDP(const MObject& attr, MDataBlock& block) {
    MStatus status; block.inputValue(attr, &status); return status ? &inputPoints : nullptr;
}
const DCurveData* getInputDC(const MObject& attr, MDataBlock& block) {
    MStatus status; block.inputValue(attr, &status); return status ? &inputCurves : nullptr;
}
const DMeshData* getInputDM(const MObject& attr, MDataBlock& block) {
    MStatus status; block.inputValue(attr, &status); return status ? &inputMeshes : nullptr;
}
void EchoWarning(const char*, const char*) {}
#define CHECK_MSTATUS_AND_RETURN_IT(status) DOMMY_RETURN_IF_FAILURE(status)
#include "attribute_curve_constraint.inc"

void reset(int failure = -1) {
    resetCalls(failure); injectedStatus = MS::kFailure;
    publications = cleans = numericWrites = 0;
    rejectFinal = rejectAdd = nullCurve = false;
    nullTypedInput = wrongTypedInput = invalidTypedTopology = false;
    invalidExpression = nullNativeArray = false;
}
Input element(short kind = DA_CREATE_INT, short partition = DP_ATTR_CLASS_PRIMITIVE, int width = 1) {
    Input result;
    result.children["attributeType"].number = kind;
    result.children["aAttributeClass"].number = partition;
    result.children["valueSize"].number = width;
    result.children["attributeName"].text = "created";
    result.children["aSetValue"].triple[0] = 12.75f;
    return result;
}
template <typename T> T first(const DPAttributeData::ConstPtr& attribute) {
    const auto typed = std::dynamic_pointer_cast<const TypedDPAttributeData<T>>(attribute);
    assert(typed && !typed->value().empty()); return typed->value()[0];
}
const Owner& resultOwner(const MObject& output, short partition) {
    if (output.name == "outDP") return outputPoints;
    if (output.name == "outDC") return partition == DP_ATTR_CLASS_PRIMITIVE
        ? static_cast<const Owner&>(outputCurves) : *outputCurves.primitives.front();
    return partition == DP_ATTR_CLASS_PRIMITIVE
        ? static_cast<const Owner&>(outputMeshes) : *outputMeshes.primitives.front();
}
void numericCases() {
    inputCurves.primitives = {std::make_shared<Primitive>(), std::make_shared<Primitive>()};
    inputMeshes.primitives = {std::make_shared<Primitive>(), std::make_shared<Primitive>()};
    for (Owner* owner : {static_cast<Owner*>(&inputPoints), static_cast<Owner*>(&inputCurves),
                         static_cast<Owner*>(&inputMeshes)})
        owner->attributes["created"] = std::make_shared<DPFloatAttr>(5.0f, 0.0f, 2);
    for (const auto& primitive : inputCurves.primitives)
        primitive->attributes["created"] = std::make_shared<DPFloatAttr>(5.0f, 0.0f, 2);
    for (const auto& primitive : inputMeshes.primitives)
        primitive->attributes["created"] = std::make_shared<DPFloatAttr>(5.0f, 0.0f, 2);
    DPAttributeCreate node;
    for (const auto& output : {DPAttributeCreate::outDP, DPAttributeCreate::outDC, DPAttributeCreate::outDM}) {
        for (short partition : {short(DP_ATTR_CLASS_POINT), short(DP_ATTR_CLASS_PRIMITIVE)}) {
            MDataBlock block; block.elements = {element(DA_CREATE_INT, partition)};
            connections.clear(); reset(); assert(node.compute(output, block));
            assert(publications == 1 && cleans == 1);
            assert(first<int>(resultOwner(output, partition).getAttr("created")) == 12);
            for (int width : {1, 2, 3}) {
                for (int axis = 0; axis < width; ++axis) {
                    for (const char* field : {"aSetValue", "defaultValue"}) {
                        for (float invalid : {1.0e30f, -1.0e30f, NAN, INFINITY}) {
                            block.elements = {element(DA_CREATE_INT, partition, width)};
                            block.elements[0].children[field].triple[axis] = invalid;
                            reset(); assert(node.compute(output, block) == MS::kInvalidParameter);
                            assert(publications == 0 && cleans == 0);
                        }
                    }
                }
            }
            block.elements = {element(DA_CREATE_INT, partition)};
            auto& connected = block.elements[0].children["inputData"];
            connected.type = MFnData::kNumeric; connected.number = 15.5;
            connections.insert("inputData"); reset(); assert(node.compute(output, block));
            assert(first<int>(resultOwner(output, partition).getAttr("created")) == 15);
            for (double invalid : {1.0e100, -1.0e100, double(NAN), double(INFINITY)}) {
                connected.number = invalid; reset();
                assert(node.compute(output, block) == MS::kInvalidParameter);
                assert(publications == 0 && cleans == 0);
            }
            connected.number = 15.5; connected.generic = true;
            reset(); assert(node.compute(output, block));
            assert(first<int>(resultOwner(output, partition).getAttr("created")) == 15);
            connections.clear();
        }
    }
    std::cout << "Attribute creation: DP/DC/DM, point/primitive, scalar/vector/default/generic numeric rejection\n";
}
void arrayAndStatusCases() {
    DPAttributeCreate node;
    for (const auto& output : {DPAttributeCreate::outDP, DPAttributeCreate::outDC, DPAttributeCreate::outDM}) {
        for (short partition : {short(DP_ATTR_CLASS_POINT), short(DP_ATTR_CLASS_PRIMITIVE)}) {
            MDataBlock block; block.elements = {element(DA_CREATE_INT, partition)};
            auto& connected = block.elements[0].children["inputData"];
            connected.type = MFnData::kIntArray; connections = {"inputData"};
            const std::size_t count = output.name != "outDP" && partition == DP_ATTR_CLASS_POINT ? 4 : 2;
            integers.assign(count, 17); reset(); assert(node.compute(output, block));
            assert(first<int>(resultOwner(output, partition).getAttr("created")) == 17);
            const auto trace = operations;
            for (std::size_t i = 0; i < trace.size(); ++i) {
                for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
                    reset(static_cast<int>(i)); injectedStatus = code;
                    const auto status = node.compute(output, block);
                    // Void copy/add/remove APIs remain exception-only boundaries.
                    const bool lossy = trace[i] == "point.copy" ||
                                       trace[i] == "attribute.add" || trace[i] == "attribute.remove";
                    assert(status == (lossy ? MS::kFailure : code));
                    assert(callCount == static_cast<int>(i) + 1);
                    assert(publications == (trace[i] == "output.clean" ? 1 : 0));
                    assert(cleans == 0);
                }
                reset(); attributeThrowAt = static_cast<int>(i);
                assert(node.compute(output, block) == MS::kFailure);
                assert(publications == (trace[i] == "output.clean" ? 1 : 0));
                assert(cleans == 0);
            }
            for (float invalid : {1.0e30f, -1.0e30f, NAN, INFINITY}) {
                block.elements[0].children["defaultValue"].triple[0] = invalid;
                reset(); assert(node.compute(output, block) == MS::kInvalidParameter);
                assert(publications == 0 && cleans == 0);
            }
            block.elements = {element(DA_CREATE_VECTOR, partition)};
            block.elements[0].children["inputData"].type = MFnData::kVectorArray;
            vectors.assign(count, Imath::V3d(1.5, 2.5, 3.5));
            reset(); assert(node.compute(output, block));
            assert(first<Imath::V3f>(resultOwner(output, partition).getAttr("created"))[1] == 2.5f);
            for (unsigned axis = 0; axis < 3; ++axis) {
                vectors[0][axis] = 1.0e100; reset();
                assert(node.compute(output, block) == MS::kInvalidParameter);
                assert(publications == 0 && cleans == 0);
                vectors[0][axis] = 1.5;
            }
            points.assign(count, Imath::V3d(1.5, 2.5, 3.5));
            block.elements[0].children["inputData"].type = MFnData::kPointArray;
            reset(); assert(node.compute(output, block));
            points.back()[2] = INFINITY; reset();
            assert(node.compute(output, block) == MS::kInvalidParameter);
            assert(publications == 0 && cleans == 0);
            connections.clear();
            block.elements = {element(DA_CREATE_BOOL, partition)};
            block.elements[0].children["aSetValue"].triple[0] = -12.0f;
            reset(); assert(node.compute(output, block));
            assert(first<bool>(resultOwner(output, partition).getAttr("created")));
            block.elements[0].children["aSetValue"].triple[0] = NAN;
            reset(); assert(node.compute(output, block) == MS::kInvalidParameter);
            assert(publications == 0 && cleans == 0);
            block.elements = {element(DA_CREATE_FLOAT, partition)};
            auto& scalar = block.elements[0].children["inputData"];
            scalar.type = MFnData::kNumeric; scalar.number = 1.0e100;
            connections = {"inputData"}; reset();
            assert(node.compute(output, block) == MS::kInvalidParameter);
            assert(publications == 0 && cleans == 0);
            block.elements[0].children["aPrecision"].number = 1;
            reset(); assert(node.compute(output, block));
            assert(first<double>(resultOwner(output, partition).getAttr("created")) == 1.0e100);
            connections.clear();
            // Not Add must retain the original type/value and skip unused controls.
            block.elements = {element(DA_CREATE_INT, partition)};
            block.elements[0].children["existActionType"].number = 1;
            block.elements[0].children["aSetValue"].triple[0] = 1.0e30f;
            reset(); assert(node.compute(output, block));
            assert(first<float>(resultOwner(output, partition).getAttr("created")) == 5.0f);
        }
    }
    // Source owners must never be mutated by a successful or failed output.
    for (const Owner* owner : {static_cast<const Owner*>(&inputPoints), static_cast<const Owner*>(&inputCurves),
                               static_cast<const Owner*>(&inputMeshes)}) {
        assert(owner->attributes.size() == 1 && first<float>(owner->getAttr("created")) == 5.0f);
    }
    for (const auto& primitive : inputCurves.primitives)
        assert(primitive->attributes.size() == 1 && first<float>(primitive->getAttr("created")) == 5.0f);
    for (const auto& primitive : inputMeshes.primitives)
        assert(primitive->attributes.size() == 1 && first<float>(primitive->getAttr("created")) == 5.0f);
    // Checked publication detects a legacy void add that silently does nothing.
    MDataBlock block; block.elements = {element()}; reset(); rejectAdd = true;
    block.elements[0].children["attributeName"].text = "newField";
    assert(node.compute(DPAttributeCreate::outDP, block) == MS::kFailure);
    assert(publications == 0 && cleans == 0);
    // Empty attribute lists preserve successful empty/no-op output handling.
    block.elements.clear(); reset(); assert(node.compute(DPAttributeCreate::outDP, block));
    assert(publications == 1 && cleans == 1);
    reset();
    const auto previous = inputPoints.getAttr("created");
    assert(dommy::attribute::createNumericAttribute<int>(inputPoints, "created", 1, 0,
        dommy::generation::kMaxGeneratedPointCount + 1) == MS::kInvalidParameter);
    assert(inputPoints.getAttr("created") == previous && callCount == 0);
    std::cout << "Attribute arrays/status: checked count, acquisition, conversion, publication, exceptions and source isolation\n";
}
void curveCases() {
    MDataBlock block; block.inputs["curve"].type = MFnData::kNurbsCurve;
    block.inputs["curve"].text = "curveObject";
    const MObject node{"node"}, input{"curve"};
    connections = {"curve"}; reset(); MObject out{"previous"};
    assert(getMayaNurbsCurve(node, input, block, out)); assert(out.name == "curveObject");
    const auto trace = operations;
    for (std::size_t i = 0; i < trace.size(); ++i) {
        for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
            reset(static_cast<int>(i)); injectedStatus = code; out = {"previous"};
            assert(getMayaNurbsCurve(node, input, block, out) == code);
            assert(out.name == "previous" && callCount == static_cast<int>(i) + 1);
        }
    }
    reset(); nullCurve = true; out = {"previous"};
    assert(!getMayaNurbsCurve(node, input, block, out)); assert(out.name == "previous");
    reset(); block.inputs["curve"].type = MFnData::kMesh; out = {"previous"};
    assert(!getMayaNurbsCurve(node, input, block, out)); assert(out.name == "previous");
    reset(); connections.clear(); out = {"previous"};
    assert(!getMayaNurbsCurve(node, input, block, out)); assert(out.name == "previous");
    std::cout << "Curve input: connection/read failures and null results preserve caller state\n";
}
template <typename Node> void constraintCase() {
    Node node; MDataBlock block; reset(); assert(node.compute(Node::outConst, block));
    assert(numericWrites == 0 && cleans == 1 && callCount == 1);
    for (auto code : {MS::kFailure, MS::kInvalidParameter}) {
        reset(0); injectedStatus = code; assert(node.compute(Node::outConst, block) == code);
        assert(numericWrites == 0 && cleans == 0 && callCount == 1);
    }
    reset(); assert(node.compute(MObject{"other"}, block) == MS::kUnknownParameter);
    assert(callCount == 0 && numericWrites == 0);
    reset(); attributeThrowAt = 0; assert(node.compute(Node::outConst, block) == MS::kFailure);
    assert(numericWrites == 0 && cleans == 0);
}
void constraintCases() {
    constraintCase<DBLinkConstraint>(); constraintCase<DBPointConstraint>();
    constraintCase<BTFixConstraint>(); constraintCase<BTP2PConstraint>();
    constraintCase<BTConeTwistConstraint>(); constraintCase<BT6DofConstraint>();
    std::cout << "Six message constraints: no numeric handle writes, checked clean and exception boundary\n";
}
template <typename CollectionType>
void attributeScopeCase(CollectionType& input, CollectionType& output, const MObject& plug) {
    DPAttributeCreate node;
    input.attributes.clear();
    input.primitives = {std::make_shared<Primitive>(), std::make_shared<Primitive>()};
    input.primitives[1]->count = 3;
    input.primitives[0]->attributes["created"] = std::make_shared<DPFloatAttr>(5.0f, 0.0f, 2);
    MDataBlock block; block.elements = {element(DA_CREATE_INT, DP_ATTR_CLASS_POINT)};
    block.elements[0].children["existActionType"].number = 1;
    connections.clear(); reset(); assert(node.compute(plug, block));
    assert(first<float>(output.primitives[0]->getAttr("created")) == 5.0f);
    assert(first<int>(output.primitives[1]->getAttr("created")) == 12);
    assert(!input.primitives[1]->getAttr("created"));

    // A same-named primitive field must neither block nor be deleted by point creation.
    input.attributes["created"] = std::make_shared<DPFloatAttr>(9.0f, 0.0f, 2);
    for (const auto& primitive : input.primitives) primitive->attributes.clear();
    for (int action : {0, 1}) {
        block.elements = {element(DA_CREATE_INT, DP_ATTR_CLASS_POINT)};
        block.elements[0].children["existActionType"].number = action;
        reset(); assert(node.compute(plug, block));
        assert(first<float>(output.getAttr("created")) == 9.0f);
        assert(first<int>(output.primitives[0]->getAttr("created")) == 12);
    }

    // Not Add skips unused invalid controls when every point owner already has the field.
    input.attributes.clear();
    for (const auto& primitive : input.primitives)
        primitive->attributes["created"] = std::make_shared<DPFloatAttr>(5.0f, 0.0f, primitive->count);
    block.elements[0].children["existActionType"].number = 1;
    block.elements[0].children["aSetValue"].triple[0] = INFINITY;
    reset(); assert(node.compute(plug, block));
    assert(first<float>(output.primitives[0]->getAttr("created")) == 5.0f);

    // Skipping an existing point field must not shift the flattened input-array partition.
    input.primitives[1]->attributes.clear();
    block.elements = {element(DA_CREATE_FLOAT, DP_ATTR_CLASS_POINT)};
    block.elements[0].children["existActionType"].number = 1;
    block.elements[0].children["inputData"].type = MFnData::kFloatArray;
    floats = {10, 11, 20, 21, 22}; connections = {"inputData"};
    reset(); assert(node.compute(plug, block));
    assert(first<float>(output.primitives[0]->getAttr("created")) == 5.0f);
    const auto second = std::dynamic_pointer_cast<const DPFloatAttr>(output.primitives[1]->getAttr("created"));
    assert(second && second->value() == std::vector<float>({20, 21, 22}));
    connections.clear();
    assert(first<float>(input.primitives[0]->getAttr("created")) == 5.0f);
    // Exercise every other flattened native array route with the same skipped prefix.
    input.attributes["created"] = std::make_shared<DPFloatAttr>(9.0f, 0.0f, 2);
    doubles = {10, 11, 20, 21, 22}; integers = {10, 11, 20, 21, 22};
    vectors.clear(); points.clear(); strings.clear(); matrices.clear();
    for (int index = 0; index < 5; ++index) {
        vectors.push_back(Imath::V3d(index, index + 1, index + 2));
        points.push_back(Imath::V3d(index, index + 1, index + 2));
        strings.push_back(MString(std::to_string(index).c_str()));
        MMatrix matrix; matrix[0][0] = index + 1; matrices.push_back(matrix);
    }
    for (auto type : {MFnData::kDoubleArray, MFnData::kIntArray, MFnData::kVectorArray,
                     MFnData::kPointArray, MFnData::kStringArray, MFnData::kMatrixArray}) {
        block.elements[0].children["inputData"].type = type;
        connections = {"inputData"}; reset(); assert(node.compute(plug, block));
        assert(first<float>(output.getAttr("created")) == 9.0f);
        assert(first<float>(output.primitives[0]->getAttr("created")) == 5.0f);
        const auto field = output.primitives[1]->getAttr("created");
        assert(field && field->attrSize() == 3);
        if (type == MFnData::kDoubleArray) assert(first<double>(field) == 20);
        else if (type == MFnData::kIntArray) assert(first<int>(field) == 20);
        else if (type == MFnData::kStringArray) assert(first<std::string>(field) == "2");
        else if (type == MFnData::kMatrixArray) assert(first<MMatrix>(field)[0][0] == 3);
        else assert(first<Imath::V3f>(field)[0] == 2);
    }
    connections.clear();
    // String/matrix creation uses the same policy and checked stored-type replacement.
    for (short kind : {short(DA_CREATE_STRING), short(DA_CREATE_MATRIX)}) {
        input.attributes["created"] = std::make_shared<DPFloatAttr>(9.0f, 0.0f, 2);
        for (const auto& primitive : input.primitives)
            primitive->attributes["created"] = std::make_shared<DPFloatAttr>(5.0f, 0.0f, primitive->count);
        for (int action : {0, 1}) {
            block.elements = {element(kind, DP_ATTR_CLASS_POINT)};
            block.elements[0].children["existActionType"].number = action;
            reset(); assert(node.compute(plug, block));
            assert(first<float>(output.getAttr("created")) == 9.0f);
            for (const auto& primitive : output.primitives) {
                const auto field = primitive->getAttr("created");
                assert(field && field->attrSize() == primitive->count);
                if (action == 1) assert(first<float>(field) == 5.0f);
                else if (kind == DA_CREATE_STRING) assert(std::dynamic_pointer_cast<const DPStringAttr>(field));
                else assert(std::dynamic_pointer_cast<const DPMat4Attr>(field));
            }
        }
    }
    block.elements = {element(DA_CREATE_INT, -1)};
    reset(); assert(node.compute(plug, block) == MS::kInvalidParameter);
    assert(publications == 0 && cleans == 0);
    std::cout << plug.name << ": point/primitive scope, mixed Not Add and flattened offsets passed\n";
}
void attributeScopeCases() {
    attributeScopeCase(inputCurves, outputCurves, DPAttributeCreate::outDC);
    attributeScopeCase(inputMeshes, outputMeshes, DPAttributeCreate::outDM);
}

void noPayloadCopy() {
    assert(publications == 0 && cleans == 0);
    for (const char* name : {"output.create", "point.copy", "collection.copy", "array.read"})
        assert(std::find(operations.begin(), operations.end(), name) == operations.end());
}
void attributeBudgetCases() {
    DPAttributeCreate node;
    connections.clear();
    inputPoints = DPData(); inputPoints.attributes.clear();
    inputCurves = DCurveData(); inputMeshes = DMeshData();
    inputCurves.attributes.clear(); inputMeshes.attributes.clear();
    inputCurves.primitives = {std::make_shared<Primitive>()};
    inputMeshes.primitives = {std::make_shared<Primitive>()};
    MDataBlock block;
#ifdef DOMMY_ATTRIBUTE_CREATE_SMALL_BUDGET
    inputPoints.count = 3U;
    // Two individually admissible fields exceed the shared 4096-byte estimate.
    block.elements = {element(DA_CREATE_MATRIX), element(DA_CREATE_MATRIX)};
    block.elements[0].children["attributeName"].text = "firstMatrix";
    block.elements[1].children["attributeName"].text = "secondMatrix";
    reset(); assert(!node.compute(DPAttributeCreate::outDP, block)); noPayloadCopy();
    // Many replacements of one field use its maximum retained cost, not a sum.
    block.elements.assign(50U, element(DA_CREATE_MATRIX));
    reset(); assert(node.compute(DPAttributeCreate::outDP, block));
    assert(outputPoints.getAttr("created")->getAttrType() == DP_ATTR_MATRIX);
    // A previously planned row makes a later Not Add row a genuine no-op.
    block.elements = {element(DA_CREATE_INT), element(DA_CREATE_MATRIX)};
    block.elements[1].children["existActionType"].number = 1;
    reset(); assert(node.compute(DPAttributeCreate::outDP, block));
    assert(first<int>(outputPoints.getAttr("created")) == 12);
    block.elements[1].children["inputData"].type = MFnData::kMatrixArray;
    connections = {"inputData"}; reportedArrayCount = 1000000U;
    reset(); assert(node.compute(DPAttributeCreate::outDP, block));
    assert(first<int>(outputPoints.getAttr("created")) == 12);
    connections.clear(); reportedArrayCount = 0U;
    // Existing payload participates in the same plan as new fields.
    inputPoints.count = 2U;
    inputPoints.attributes["one"] = std::make_shared<DPMat4Attr>(MMatrix(), MMatrix(), 2U);
    inputPoints.attributes["two"] = std::make_shared<DPMat4Attr>(MMatrix(), MMatrix(), 2U);
    block.elements = {element(DA_CREATE_MATRIX)};
    reset(); assert(!node.compute(DPAttributeCreate::outDP, block)); noPayloadCopy();
    inputPoints.attributes.clear();
    // Aggregate point owners, including defaults on empty owners, are charged.
    inputCurves.primitives = {std::make_shared<Primitive>(), std::make_shared<Primitive>(),
                              std::make_shared<Primitive>()};
    inputMeshes.primitives = {std::make_shared<Primitive>(), std::make_shared<Primitive>(),
                              std::make_shared<Primitive>()};
    block.elements = {element(DA_CREATE_MATRIX, DP_ATTR_CLASS_POINT)};
    for (const auto& plug : {DPAttributeCreate::outDC, DPAttributeCreate::outDM}) {
        reset(); assert(!node.compute(plug, block)); noPayloadCopy();
    }
#else
    // Synthetic cardinality reaches only borrowed planning; no giant vector.
    inputPoints.count = 1000000U;
    inputCurves.primitives[0]->count = inputMeshes.primitives[0]->count = 1000000U;
    block.elements = {element(DA_CREATE_MATRIX, DP_ATTR_CLASS_POINT)};
    for (const auto& plug : {DPAttributeCreate::outDP, DPAttributeCreate::outDC, DPAttributeCreate::outDM}) {
        reset(); assert(!node.compute(plug, block)); noPayloadCopy();
    }
    // Repeated string bytes, not just the vector's object widths, are admitted.
    inputPoints.count = 6000U;
    block.elements = {element(DA_CREATE_STRING)};
    block.elements[0].children["inputData"].type = MFnData::kString;
    block.elements[0].children["inputData"].text.assign(20000U, 'x');
    connections = {"inputData"};
    reset(); assert(!node.compute(DPAttributeCreate::outDP, block)); noPayloadCopy();
#endif
    inputPoints.count = 2U;
    block.elements = {element(DA_CREATE_MATRIX)};
    block.elements[0].children["inputData"].type = MFnData::kMatrixArray;
    connections = {"inputData"}; reportedArrayCount = 1000000U;
    reset(); assert(!node.compute(DPAttributeCreate::outDP, block)); noPayloadCopy();
    reportedArrayCount = 0U; connections.clear();
    block.elements = {element(DA_CREATE_INT)};
    reset(); assert(node.compute(DPAttributeCreate::outDP, block));
    assert(first<int>(outputPoints.getAttr("created")) == 12);
    for (int malformed = 0; malformed < 3; ++malformed) {
        reset(); nullTypedInput = malformed == 0; wrongTypedInput = malformed == 1;
        invalidTypedTopology = malformed == 2;
        assert(!node.compute(DPAttributeCreate::outDP, block)); noPayloadCopy();
    }
    std::cout << "Attribute creation: pre-copy aggregate/array/string admission, replacement and retry passed\n";
}

template <typename Node>
void requiredAdmission(Node& node, const MObject& output, MDataBlock& block, bool full) {
    reset(); assert(node.compute(output, block));
    const auto baseline = operations;
    for (std::size_t at = 0U; at < baseline.size(); ++at) {
        for (int mode = 0; mode < 3; ++mode) {
            reset(static_cast<int>(at)); injectedStatus = mode == 0 ? MS::kFailure : MS::kInvalidParameter;
            if (mode == 2) { failAt = -1; attributeThrowAt = static_cast<int>(at); }
            const auto result = node.compute(output, block);
            const bool voidCall = baseline[at] == "array.clean" || baseline[at] == "point.copy";
            assert(result == (mode == 2 || voidCall ? MS::kFailure : injectedStatus));
            assert(callCount == static_cast<int>(at) + 1);
            if (baseline[at] != "array.clean" && baseline[at] != "output.clean") assert(publications == 0);
            reset(); assert(node.compute(output, block));
        }
    }
    for (int malformed = 0; malformed < 3; ++malformed) {
        reset(); nullTypedInput = malformed == 0; wrongTypedInput = malformed == 1;
        invalidTypedTopology = malformed == 2;
        assert(!node.compute(output, block)); noPayloadCopy();
        reset(); assert(node.compute(output, block));
    }
    reset(); assert(node.compute(MObject{"other"}, block) == MS::kUnknownParameter);
    assert(callCount == 0);
    std::cout << output.name << ": " << baseline.size() << " required input "
              << (full ? "complete callback" : "admission prefix") << " status/exception sites passed\n";
}
void requiredCallbackCases() {
    inputPoints = DPData(); inputCurves = DCurveData(); inputMeshes = DMeshData();
    inputCurves.primitives = {std::make_shared<Primitive>()};
    inputMeshes.primitives = {std::make_shared<Primitive>()};
    inputPoints.attributes["number"] = std::make_shared<DPDoubleAttr>(std::vector<double>{1.25, 2.5});
    MDataBlock block;
    DPAttributeToDataArray array;
    block.inputs["aAttributeName"].text = "number";
    requiredAdmission(array, DPAttributeToDataArray::outDataArray, block, true);
    assert(MFnDoubleArrayData::produced == MDoubleArray({1.25, 2.5}));
    reset(); nullNativeArray = true; assert(!array.compute(DPAttributeToDataArray::outDataArray, block));
    assert(publications == 0 && cleans == 0);
    DPGroup pointGroup; DCGroup curveGroup; DMGroup meshGroup;
    requiredAdmission(pointGroup, DPGroup::outDP, block, false);
    requiredAdmission(curveGroup, DCGroup::outDC, block, false);
    requiredAdmission(meshGroup, DMGroup::outDM, block, false);
    DPAttributeSeExpr expression;
    block.inputs["aAttributeName"].text = "expressionResult";
    block.inputs["aExpression"].text = "$P";
    for (int id : {DPData::id, DCurveData::id, DMeshData::id}) {
        block.inputs["inDD"].typedId = id;
        requiredAdmission(expression, DPAttributeSeExpr::outDD, block, true);
    }
    reset(); invalidExpression = true;
    assert(!expression.compute(DPAttributeSeExpr::outDD, block)); noPayloadCopy();
    reset(); block.inputs["inDD"].typedId = -1;
    assert(!expression.compute(DPAttributeSeExpr::outDD, block)); noPayloadCopy();
}
void generatorInputBoundaries() {
    inputPoints = DPData(); inputCurves = DCurveData(); inputMeshes = DMeshData();
    inputCurves.primitives = {std::make_shared<Primitive>()};
    inputMeshes.primitives = {std::make_shared<Primitive>()};
    MDataBlock block;
    connections = {"inDM"};
    using Reader = MStatus(*)(MDataBlock&);
    for (Reader reader : {readPointCurveInput, readWireCurveInput, readDuplicatorPoints,
                          readDuplicatorMesh, readVoronoiPoints}) {
        reset(); assert(reader(block)); const auto baseline = operations;
        for (std::size_t at = 0; at < baseline.size(); ++at) {
            for (int mode = 0; mode < 3; ++mode) {
                reset(static_cast<int>(at)); injectedStatus = mode == 0 ? MS::kFailure : MS::kInvalidParameter;
                if (mode == 2) { failAt = -1; attributeThrowAt = static_cast<int>(at); }
                assert(reader(block) == (mode == 2 ? MS::kFailure : injectedStatus));
                assert(callCount == static_cast<int>(at) + 1); noPayloadCopy();
                reset(); assert(reader(block));
            }
        }
        for (int malformed = 0; malformed < 3; ++malformed) {
            reset(); nullTypedInput = malformed == 0; wrongTypedInput = malformed == 1;
            invalidTypedTopology = malformed == 2;
            assert(reader(block) == (malformed == 1 ? MS::kFailure : MS::kInvalidParameter)); noPayloadCopy();
        }
    }
    // A disconnected mesh remains optional; no typed data acquisition occurs.
    connections.clear(); reset(); nullTypedInput = true;
    assert(readDuplicatorMesh(block)); assert(operations == std::vector<std::string>{"connection.query"});
    std::cout << "Five generator input boundaries: exact status, null/type/topology and disconnected mesh passed\n";
}
int main(int argc, char** argv) {
    const std::string selection = argc == 2 ? argv[1] : "all";
    assert(argc <= 2 && (selection == "all" || selection == "attribute" ||
                        selection == "curve" || selection == "constraint" || selection == "budget" ||
                        selection == "required"));
    if (selection == "all" || selection == "attribute") {
        numericCases(); arrayAndStatusCases(); attributeScopeCases();
    }
    if (selection == "all" || selection == "curve") curveCases();
    if (selection == "all" || selection == "constraint") constraintCases();
    if (selection == "all" || selection == "budget") attributeBudgetCases();
    if (selection == "all" || selection == "required") {
        requiredCallbackCases(); generatorInputBoundaries();
    }
}
