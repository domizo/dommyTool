#pragma once

// Failure-injectable API doubles; these do not model Maya's DG or ownership internals.
#include "noise/PerlinNoise.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DScaledProduct.h"
#include "modules/point/DPoint/math/DVectorMath.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

using MStatus = int;
namespace MS {
const MStatus kSuccess = 1, kFailure = 0, kInvalidParameter = -1, kUnknownParameter = -2;
}
#define CHECK_MSTATUS_AND_RETURN_IT(status)                                                        \
    do {                                                                                           \
        if ((status) != MS::kSuccess)                                                              \
            return status;                                                                         \
    } while (false)
#define DOMMY_RETURN_IF_FAILURE(expression)                                                        \
    do {                                                                                           \
        const MStatus s = (expression);                                                            \
        CHECK_MSTATUS_AND_RETURN_IT(s);                                                            \
    } while (false)
#define McheckErr(status, message) CHECK_MSTATUS_AND_RETURN_IT(status)

int failAt = -1, callCount = 0, invalidReads = 0, publications = 0, cleanCalls = 0;
std::vector<std::string> operations;
bool operation(const std::string& name, MStatus* status) {
    const bool ok = callCount++ != failAt;
    operations.push_back(name);
    if (status)
        *status = ok ? MS::kSuccess : MS::kFailure;
    return ok;
}
MStatus operation(const std::string& name) {
    MStatus status;
    operation(name, &status);
    return status;
}
void resetCalls(int failure = -1) {
    failAt = failure;
    callCount = invalidReads = publications = cleanCalls = 0;
    operations.clear();
}

struct MVector {
    double x, y, z;
    MVector(double a = 0, double b = 0, double c = 0) : x(a), y(b), z(c) {}
    double& operator[](unsigned int i) {
        assert(i < 3);
        return i == 0 ? x : (i == 1 ? y : z);
    }
    double operator[](unsigned int i) const {
        assert(i < 3);
        return i == 0 ? x : (i == 1 ? y : z);
    }
    MVector operator-(const MVector& value) const {
        return MVector(x - value.x, y - value.y, z - value.z);
    }
    MVector operator+(const MVector& value) const {
        return MVector(x + value.x, y + value.y, z + value.z);
    }
    MVector operator*(double scale) const {
        return MVector(x * scale, y * scale, z * scale);
    }
    MVector operator/(double scale) const {
        return MVector(x / scale, y / scale, z / scale);
    }
    static const MVector zero;
};
const MVector MVector::zero;
struct MPoint : MVector {
    double w = 1.0;
    MPoint(double x = 0.0, double y = 0.0, double z = 0.0) : MVector(x, y, z) {}
};
struct MMatrix {
    double values[4][4] = {};
    double operator()(unsigned int row, unsigned int column) const {
        return values[row][column];
    }
};
namespace dommy {
namespace maya {
bool isFiniteVector(const MVector& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
bool isFinitePoint(const MPoint& value) {
    return isFiniteVector(value) && std::isfinite(value.w);
}
bool isFiniteMatrix(const MMatrix& m) {
    for (const auto& row : m.values)
        for (double value : row)
            if (!std::isfinite(value))
                return false;
    return true;
}
} // namespace maya
} // namespace dommy
template <typename T> struct Array {
    std::vector<T> values;
    unsigned int length() const {
        return static_cast<unsigned int>(values.size());
    }
    const T& operator[](unsigned int i) const {
        return values.at(i);
    }
    T& operator[](unsigned int i) {
        return values.at(i);
    }
    MStatus setLength(unsigned int count) {
        const MStatus status = operation("array.setLength");
        if (status == MS::kSuccess)
            values.resize(count);
        return status;
    }
    MStatus append(const T& value) {
        const MStatus status = operation("array.append");
        if (status == MS::kSuccess)
            values.push_back(value);
        return status;
    }
    MStatus set(const T& value, unsigned int index) {
        const MStatus status = operation("array.set");
        if (status == MS::kSuccess)
            values.at(index) = value;
        return status;
    }
    MStatus copy(const Array& source) {
        const MStatus status = operation("array.copy");
        if (status == MS::kSuccess)
            values = source.values;
        return status;
    }
};
using MVectorArray = Array<MVector>;
using MDoubleArray = Array<double>;
struct Payload {
    MVectorArray vectors;
    MDoubleArray doubles;
};
enum Attribute {
    mMagnitude = 1,
    curlSpeed,
    curlAmplitude,
    curlAgeSpeed,
    noiseFrequencyX,
    noiseFrequencyY,
    noiseFrequencyZ,
    phaseRateX,
    phaseRateY,
    phaseRateZ,
    phaseOffsetX,
    phaseOffsetY,
    phaseOffsetZ,
    positionNoiseScaleX,
    positionNoiseScaleY,
    positionNoiseScaleZ,
    positionNoiseFrequency,
    mUseMaxDistance,
    mMaxDistance,
    mAttenuation,
    mApplyPerVertex,
    mOwnerCentroidX,
    mOwnerCentroidY,
    mOwnerCentroidZ,
    mOwnerPosData,
    mInputData,
    mInputPositions,
    mInputVelocities,
    mInputMass,
    mInputPPData,
    mOutputForce,
    worldMatrix,
    inputTargetCurve,
    tangentAffectDistance,
    aDamping,
    aAttraction,
    aForwardScale,
    aToCurveScale,
    aBlend,
    aIsAffectTip,
    aInnerMethod,
    aRadiusRamp,
    aForwardRamp,
    aToCurveRamp,
    aBlendRamp
};
namespace MFnData {
enum Type { kInvalid, kNurbsCurve };
}
struct MObject {
    int id;
    MFnData::Type kind = MFnData::kInvalid;
    std::shared_ptr<Payload> payload;
    MObject(int value = 0) : id(value) {}
    bool isNull() const {
        return !payload;
    }
};
struct MPlug {
    int id;
    unsigned int index;
    explicit MPlug(int value = 0, unsigned int element = 5) : id(value), index(element) {}
    MPlug(const MObject&, const MObject& attribute) : id(attribute.id), index(0) {}
    bool operator==(const MObject& value) const {
        return id == value.id;
    }
    bool operator!=(const MObject& value) const {
        return !(*this == value);
    }
    unsigned int logicalIndex(MStatus* status) const {
        operation("plug.logicalIndex", status);
        return index;
    }
    MPlug elementByLogicalIndex(unsigned int element, MStatus* status) {
        operation("plug.element", status);
        return MPlug(id, element);
    }
};
std::map<int, MObject> objects;
MObject published;
MObject pendingFieldPayload;
bool deferFieldPublication = false;
MMatrix world;
struct MString : std::string {
    using std::string::string;
    const char* asChar() const { return c_str(); }
};
struct MDataHandle {
    bool valid = true;
    double number = 0;
    MObject object;
    MString text = "*";
    void check() const {
        if (!valid)
            ++invalidReads;
    }
    double asDouble() const {
        check();
        return number;
    }
    MString asString() const { check(); return text; }
    int asInt() const {
        check();
        return static_cast<int>(number);
    }
    short asShort() const {
        check();
        return static_cast<short>(number);
    }
    MFnData::Type type() const {
        check();
        return object.kind;
    }
    bool asBool() const {
        check();
        return number != 0;
    }
    MObject data() const {
        check();
        return object;
    }
    MMatrix asMatrix() const {
        check();
        return world;
    }
    MDataHandle child(int attribute) const {
        check();
        MDataHandle h;
        h.object = objects.at(attribute);
        return h;
    }
    MStatus set(const MObject& value) {
        check();
        const MStatus status = operation("output.set");
        if (status == MS::kSuccess) {
            if (deferFieldPublication) pendingFieldPayload = value;
            else { published = value; ++publications; }
        }
        return status;
    }
};
unsigned int lastOutputIndex = 0;
struct MArrayDataBuilder {
    MDataHandle addElement(unsigned int index, MStatus* status) {
        MDataHandle h;
        h.valid = operation("builder.addElement", status);
        lastOutputIndex = index;
        return h;
    }
};
struct MArrayDataHandle {
    int attribute;
    MStatus jumpToElement(unsigned int) {
        return operation("array.jump");
    }
    MDataHandle inputValue(MStatus* status) {
        MDataHandle h;
        h.valid = operation("array.input", status);
        if (attribute == mInputPPData)
            h.object = objects.at(mInputPPData);
        return h;
    }
    MArrayDataBuilder builder(MStatus* status) {
        operation("array.builder", status);
        return {};
    }
    MStatus set(const MArrayDataBuilder&) {
        const MStatus status = operation("array.setBuilder");
        if (status == MS::kSuccess && deferFieldPublication) {
            published = pendingFieldPayload;
            ++publications;
        }
        return status;
    }
};
struct MDataBlock {
    std::map<int, double> values;
    std::map<int, MString> strings;
    MDataHandle inputValue(const MObject& attribute, MStatus* status) {
        MDataHandle h;
        h.valid = operation("input." + std::to_string(attribute.id), status);
        h.number = values[attribute.id];
        const auto stringValue = strings.find(attribute.id);
        if (stringValue != strings.end()) h.text = stringValue->second;
        const auto found = objects.find(attribute.id);
        if (found != objects.end())
            h.object = found->second;
        return h;
    }
    MDataHandle inputValue(const MPlug& plug, MStatus* status) {
        return inputValue(MObject(plug.id), status);
    }
    MArrayDataHandle outputArrayValue(int attribute, MStatus* status) {
        operation("block.outputArray", status);
        return {attribute};
    }
    MArrayDataHandle inputArrayValue(int attribute, MStatus* status) {
        operation("block.inputArray", status);
        return {attribute};
    }
    MStatus setClean(const MPlug&) {
        const MStatus status = operation("block.setClean");
        if (status == MS::kSuccess)
            ++cleanCalls;
        return status;
    }
};
struct MFnVectorArrayData {
    MObject object;
    MFnVectorArrayData() = default;
    MFnVectorArrayData(const MObject& value, MStatus* status) : object(value) {
        operation("vector.bind", status);
    }
    MVectorArray array(MStatus* status) const {
        operation("vector.array", status);
        return object.payload->vectors;
    }
    MObject create(const MVectorArray& value, MStatus* status) {
        MObject result;
        if (operation("vector.create", status)) {
            result.payload.reset(new Payload);
            result.payload->vectors = value;
        }
        return result;
    }
};
struct MFnDoubleArrayData {
    MObject object;
    MFnDoubleArrayData(const MObject& value, MStatus* status) : object(value) {
        operation("double.bind", status);
    }
    MDoubleArray array(MStatus* status) const {
        operation("double.array", status);
        return object.payload->doubles;
    }
};
bool ageExists = true, wrongAgeType = false;
struct MFnArrayAttrsData {
    enum Type { kDoubleArray, kVectorArray };
    MObject object;
    MFnArrayAttrsData(const MObject& value, MStatus* status) : object(value) {
        operation("pp.bind", status);
    }
    bool checkArrayExist(const MString&, Type& type, MStatus* status) {
        operation("pp.exists", status);
        type = wrongAgeType ? kVectorArray : kDoubleArray;
        return ageExists;
    }
    MDoubleArray getDoubleData(const MString&, MStatus* status) {
        operation("pp.age", status);
        return object.payload->doubles;
    }
};
struct MFnDependencyNode {
    MFnDependencyNode(const MObject&, MStatus* status) {
        operation("node.bind", status);
    }
    MObject attribute(const char*, MStatus* status) {
        operation("node.attribute", status);
        return MObject(worldMatrix);
    }
};
