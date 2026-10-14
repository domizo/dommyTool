#pragma once

#include "maya_attribute_doubles.h"
#include "point_input_fixture.h"
#include "common/utility/DAttributeArrayConversion.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaCheckedArray.h"
#include "common/utility/DParticleFieldMath.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <map>

// Real typed attributes and math; no Maya DG/array ownership or native solver.
struct MVector {
    double x, y, z;
    MVector(double a = 0, double b = 0, double c = 0) : x(a), y(b), z(c) {}
    double& operator[](unsigned int i) { assert(i < 3); return i == 0 ? x : i == 1 ? y : z; }
    double operator[](unsigned int i) const { assert(i < 3); return i == 0 ? x : i == 1 ? y : z; }
    MVector operator-(const MVector& v) const { return MVector(x-v.x, y-v.y, z-v.z); }
    double length() const { return std::sqrt(x*x + y*y + z*z); }
    bool operator==(const MVector& v) const { return x == v.x && y == v.y && z == v.z; }
    static const MVector zero;
};
const MVector MVector::zero;
static int shortArrayAt = -1;
static int excessiveAllocations = 0, invalidArrayAccesses = 0;

template <typename Value> struct FieldArray {
    std::vector<Value> values;
    unsigned int reportedLength = 0;
    FieldArray() = default;
    // Kept for mutations of the original unchecked constructor.
    FieldArray(unsigned int count, const Value& initial) {
        operation("array.constructor");
        checkTestAllocation(count);
        values.assign(count, initial);
    }
    static void checkTestAllocation(unsigned int count) {
        if (count > 64U) {
            ++excessiveAllocations;
            throw std::length_error("test intercepted oversized allocation");
        }
    }
    unsigned int length() const { return reportedLength ? reportedLength : values.size(); }
    Value& operator[](unsigned int i) {
        if (i >= values.size()) ++invalidArrayAccesses;
        return values.at(i);
    }
    const Value& operator[](unsigned int i) const {
        if (i >= values.size()) ++invalidArrayAccesses;
        return values.at(i);
    }
    MStatus setLength(unsigned int count) {
        const int index = callCount;
        const MStatus status = operation("array.setLength");
        checkTestAllocation(count);
        if (status) {
            values.resize(count);
            if (index == shortArrayAt && count) values.pop_back();
        }
        return status;
    }
    MStatus copy(const FieldArray& other) {
        const int index = callCount;
        const MStatus status = operation("array.copy");
        // A failed host copy may already have modified its destination.
        values = other.values;
        if ((!status || index == shortArrayAt) && !values.empty()) values.pop_back();
        return status;
    }
};
using MVectorArray = FieldArray<MVector>;
using MDoubleArray = FieldArray<double>;
using DPoint = Imath::V3f;
using DPointList = std::vector<DPoint>;
using DScalarDataList = std::vector<float>;

struct DPData {
    static const int id=22;
    bool topology=true;
    bool hasValidPointTopology()const{return topology;}
    DPointList points = {DPoint(2,1,0), DPoint(3,1,0)};
    std::map<std::string, DPAttributeData::Ptr> attributes;
    std::size_t numberOfPoints() const { return points.size(); }
    const DPointList* getPositionPtrConst() const { return &points; }
    bool isExistAttr(const std::string& name) const { return bool(getAttr(name)); }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        const auto found = attributes.find(name);
        return found == attributes.end() ? nullptr : found->second;
    }
};
static DPData input;
static bool hasInput = true;
static std::map<std::string, double> numericInputs;
static std::map<std::string, std::string> stringInputs;
static MVectorArray positions, velocities, publishedForces, stagedForces;
static MDoubleArray masses;
static int publications = 0, cleanCalls = 0, falloffReads = 0;
static unsigned int publishedIndex = 0, inputIndex = 0;
static std::vector<double> falloffValues;
static std::vector<double> sampledRatios;

const MObject mInputData{"mInputData"}, mInputPositions{"mInputPositions"},
    mInputVelocities{"mInputVelocities"}, mInputMass{"mInputMass"},
    mOutputForce{"mOutputForce"}, mMagnitude{"mMagnitude"},
    mUseMaxDistance{"mUseMaxDistance"}, mMaxDistance{"mMaxDistance"}, mAttenuation{"mAttenuation"};
struct MPlug {
    MObject attribute;
    int index;
    explicit MPlug(const MObject& a, int i = 42) : attribute(a), index(i) {}
    MPlug(const MObject&,const MObject& a):attribute(a),index(42){}
    bool isConnected(MStatus* status)const{return pointInputConnection(status);}
    bool operator!=(const MObject& a) const { return attribute.name != a.name; }
    int logicalIndex(MStatus* status) const { *status = operation("plug.index"); return index; }
};
struct MDataHandle {
    MObject attribute;
    bool valid;
    MDataHandle(const MObject& a = MObject(), bool v = true) : attribute(a), valid(v) {}
    MDataHandle child(const MObject& a) const { assert(valid); return MDataHandle(a); }
    MObject data() const {
        assert(valid);
        if(attribute.name=="inDP"){
            ++pointInputFixture.dataReads;
            return hasInput?attribute:MObject::kNullObj;
        }
        return attribute;
    }
    double asDouble() const { assert(valid); return numericInputs.at(attribute.name); }
    bool asBool() const { return asDouble() != 0; }
    MString asString() const { assert(valid); return stringInputs.at(attribute.name).c_str(); }
    MStatus set(const MObject& object) {
        assert(valid && object.name == "staged");
        const MStatus status = operation("output.set");
        if (status) { publishedForces = stagedForces; ++publications; }
        return status;
    }
};
struct MArrayDataBuilder {
    MDataHandle addElement(unsigned int index, MStatus* status) {
        *status = operation("builder.addElement");
        publishedIndex = index;
        return MDataHandle(mOutputForce, bool(*status));
    }
};
struct MArrayDataHandle {
    bool valid;
    explicit MArrayDataHandle(bool v) : valid(v) {}
    MStatus jumpToElement(unsigned int index) {
        assert(valid); inputIndex = index; return operation("array.jump");
    }
    MDataHandle inputValue(MStatus* status) {
        assert(valid); *status = operation("array.input"); return MDataHandle(mInputData, bool(*status));
    }
    MArrayDataBuilder builder(MStatus* status) {
        assert(valid); *status = operation("array.builder"); return {};
    }
};
struct MDataBlock {
    MDataHandle inputValue(const MObject& a, MStatus* status) {
        *status = operation("input " + a.name); return MDataHandle(a,a.name=="inDP"||bool(*status));
    }
    MArrayDataHandle outputArrayValue(const MObject& a, MStatus* status) {
        *status = operation("block.array " + a.name); return MArrayDataHandle(bool(*status));
    }
    MStatus setClean(const MPlug&) {
        const MStatus status = operation("output.clean");
        if (status) ++cleanCalls;
        return status;
    }
};
#define DOMMY_POINT_INPUT_DATA input
#define DOMMY_POINT_INPUT_PRESENT hasInput
#include "point_input_binding_fixture.h"
struct MFnVectorArrayData {
    MObject source;
    bool valid = true;
    MFnVectorArrayData() = default;
    MFnVectorArrayData(const MObject& a, MStatus* status) : source(a) {
        *status = operation("vector.bind " + a.name); valid = bool(*status);
    }
    MVectorArray array(MStatus* status) const {
        assert(valid); *status = operation("vector.read " + source.name);
        return source.name == mInputPositions.name ? positions : velocities;
    }
    MObject create(const MVectorArray& values, MStatus* status) {
        *status = operation("output.create");
        if (*status) stagedForces = values;
        return {"staged"};
    }
};
struct MFnDoubleArrayData {
    bool valid;
    MFnDoubleArrayData(const MObject&, MStatus* status) {
        *status = operation("mass.bind"); valid = bool(*status);
    }
    MDoubleArray array(MStatus* status) const {
        assert(valid); *status = operation("mass.read"); return masses;
    }
};
struct FieldBoundary {
    MObject thisMObject()const{return {"field"};}
    double falloffCurve(double ratio, MStatus* status) {
        assert(std::isfinite(ratio) && ratio >= 0 && ratio <= 1);
        *status = operation("falloff.read");
        sampledRatios.push_back(ratio);
        return falloffValues.at(falloffReads++);
    }
};
