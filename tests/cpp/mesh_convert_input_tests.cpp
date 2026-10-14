// Actual input-admission prefix and helpers; injectable host boundaries only.
// Native mesh creation, DAG instancing and parallel DG evaluation are not emulated.
#include "common/utility/DComputeGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMeshMergePlan.h"
#include "common/utility/DStatusOperation.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

static int calls = 0, failAt = -1, throwAt = -1, publications = 0;
static MS::MStatusCode failureCode = MS::kFailure;
static std::vector<std::string> operations;
static MStatus api(const std::string& name) {
    operations.push_back(name);
    const int current = calls++;
    if (current == throwAt) throw std::runtime_error("injected host exception");
    return current == failAt ? failureCode : MS::kSuccess;
}
namespace MFn {enum Type {kDagNode};}
namespace MFnData {enum Type {kMesh, kOther};}
struct MObject {
    std::string name;
    unsigned int index = 0;
    MObject() = default;
    explicit MObject(const std::string& value, unsigned int id = 0) : name(value), index(id) {}
    bool isNull() const {return name.empty();}
    bool hasFn(MFn::Type) const {return name == "dag";}
};
struct MMatrix {
    double values[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    double* operator[](int i) {return values[i];}
    const double* operator[](int i) const {return values[i];}
    static const MMatrix identity;
};
const MMatrix MMatrix::identity;
namespace dommy {namespace maya {
bool isFiniteMatrix(const MMatrix& matrix) {
    for (int row = 0; row < 4; ++row) for (int col = 0; col < 4; ++col)
        if (!std::isfinite(matrix[row][col])) return false;
    return true;
}
}}
struct Element {
    unsigned int logical;
    MObject object;
    MMatrix matrix;
    MFnData::Type type = MFnData::kMesh;
    explicit Element(unsigned int index, const char* name = "mesh") : logical(index), object(name) {}
};
struct ArrayInput {
    std::vector<Element> elements;
    unsigned int reportedCount = 0;
};
struct Source {
    MObject node;
    MMatrix matrix;
    explicit Source(unsigned int index = 0, const char* kind = "dag") : node(kind, index) {}
};
static ArrayInput meshValues, matrixValues;
static std::map<unsigned int, Source> sources;
static short mode = 0;
struct MDataHandle {
    bool valid = false;
    const Element* element = nullptr;
    void check() const {assert(valid);}
    MObject data() {check(); assert(element); return element->object;}
    MFnData::Type type() const {check(); assert(element); return element->type;}
    MMatrix asMatrix() const {check(); assert(element); return element->matrix;}
    MTime asTime() const {check(); return MTime();}
    short asShort() const {check(); return mode;}
    bool asBool() const {check(); return false;}
    MString asString() const {check(); return MString("N");}
};
struct MArrayDataHandle {
    ArrayInput* input = nullptr;
    std::string name;
    unsigned int position = 0;
    bool valid = false, positioned = false;
    unsigned int elementCount(MStatus* status) {
        assert(valid); *status = api(name + ".count");
        return input->reportedCount ? input->reportedCount : static_cast<unsigned int>(input->elements.size());
    }
    MStatus jumpToArrayElement(unsigned int physical) {
        assert(valid); const auto status = api(name + ".jump " + std::to_string(physical));
        positioned = bool(status) && physical < input->elements.size();
        if (positioned) position = physical;
        return status && !positioned ? MStatus(MS::kFailure) : status;
    }
    unsigned int elementIndex(MStatus* status) {
        assert(valid && positioned); *status = api(name + ".index");
        return input->elements[position].logical;
    }
    MDataHandle inputValue(MStatus* status) {
        assert(valid && positioned); *status = api(name + ".value " + std::to_string(position));
        MDataHandle result; result.valid = bool(*status); result.element = &input->elements[position];
        return result;
    }
};
struct MDataBlock {
    MDataHandle inputValue(const MObject& attribute, MStatus* status) {
        *status = api("control " + attribute.name);
        MDataHandle result; result.valid = bool(*status); return result;
    }
    MArrayDataHandle inputArrayValue(const MObject& attribute, MStatus* status) {
        *status = api("array " + attribute.name);
        MArrayDataHandle result; result.valid = bool(*status);
        result.name = attribute.name; result.input = attribute.name == "inMeshes" ? &meshValues : &matrixValues;
        return result;
    }
};
struct MPlug {
    MObject attribute;
    unsigned int logical = 0;
    bool null = false;
    explicit MPlug(const MObject& value) : attribute(value) {}
    MPlug(const MObject&, const MObject& value) : attribute(value) {}
    bool operator!=(const MObject& value) const {return attribute.name != value.name;}
    bool isNull() const {return null;}
    MPlug elementByLogicalIndex(unsigned int index, MStatus* status) {
        *status = api("plug.element"); MPlug result(*this); result.logical = index; return result;
    }
    MPlug source(MStatus* status) {
        *status = api("plug.source"); MPlug result(*this); result.null = !sources.count(logical); return result;
    }
    MObject node(MStatus* status) {
        *status = api("plug.node"); return sources.at(logical).node;
    }
};
struct MDagPath {
    unsigned int index = 0;
    static MStatus getAPathTo(const MObject& object, MDagPath& path) {
        const auto status = api("dag.path"); if (status) path.index = object.index; return status;
    }
    MMatrix inclusiveMatrix(MStatus* status) {
        *status = api("dag.matrix"); return sources.at(index).matrix;
    }
};
#include "mesh_convert_input.inc"

static void reset() {
    calls = 0; failAt = throwAt = -1; publications = 0; operations.clear(); failureCode = MS::kFailure;
    meshValues = ArrayInput(); matrixValues = ArrayInput(); sources.clear(); mode = 0;
    admittedInputs.assign(1, MeshInput()); admittedInputs[0].meshObject = MObject("previous");
}
static void fixture(int kind) {
    reset();
    meshValues.elements.emplace_back(40);
    meshValues.elements.emplace_back(2);
    meshValues.elements.emplace_back(std::numeric_limits<unsigned int>::max());
    // Missing source for UINT_MAX represents an unconnected data element.
    Source first(40); first.matrix[3][0] = 40; sources.emplace(40, first);
    Source second(2); second.matrix[3][0] = 2; sources.emplace(2, second);
    if (kind != 0) {
        matrixValues.elements.emplace_back(999); // unused non-finite values are not read through inputValue
        matrixValues.elements[0].matrix[0][0] = NAN;
        matrixValues.elements.emplace_back(2); matrixValues.elements[1].matrix[3][0] = 102;
    }
    if (kind == 2) {
        matrixValues.elements.emplace_back(40); matrixValues.elements[2].matrix[3][0] = 140;
        matrixValues.elements.emplace_back(std::numeric_limits<unsigned int>::max());
        matrixValues.elements[3].matrix[3][0] = 199;
    }
}
static MStatus compute() {
    MDataBlock data;
    return DMConvertFromMeshes().compute(MPlug(DMConvertFromMeshes::outDM), data);
}
static void expectFailure() {
    assert(!compute() && publications == 0 && admittedInputs.size() == 1);
    assert(admittedInputs[0].meshObject.name == "previous");
}
int main() {
    int injections = 0;
    for (int kind = 0; kind < 3; ++kind) for (short conversion : {0, 1, 3}) {
        fixture(kind); mode = conversion; assert(compute() && publications == 1);
        assert(admittedInputs.size() == 3);
        assert(admittedInputs[0].worldMatrix[3][0] == (kind == 2 ? 140 : 40));
        assert(admittedInputs[1].worldMatrix[3][0] == (kind == 0 ? 2 : 102));
        assert(admittedInputs[2].hasWorldMatrix == (kind == 2));
        if (kind == 2) assert(admittedInputs[2].worldMatrix[3][0] == 199);
        assert(std::find(operations.begin(), operations.end(), "aInMatrices.value 0") == operations.end());
        const auto trace = operations;
        for (std::size_t i = 0; i < trace.size(); ++i) for (int failure = 0; failure < 3; ++failure) {
            fixture(kind); mode = conversion;
            if (failure == 2) throwAt = int(i);
            else {failAt = int(i); failureCode = failure ? MS::kInvalidParameter : MS::kFailure;}
            const auto status = compute();
            assert(status == (failure == 2 ? MS::kFailure : failureCode));
            assert(calls == int(i) + 1 && publications == 0 && admittedInputs[0].meshObject.name == "previous");
            ++injections;
        }
    }
    fixture(0); sources.at(40).matrix[0][0] = NAN; expectFailure();
    fixture(0); sources.at(40).matrix[1][2] = INFINITY; expectFailure();
    fixture(0); sources.at(40).node = MObject(); expectFailure();
    fixture(0); sources.at(40).node = MObject("dependency");
    assert(compute() && !admittedInputs[0].hasWorldMatrix && admittedInputs[1].hasWorldMatrix);
    fixture(1); matrixValues.elements[1].matrix[1][1] = NAN; expectFailure();
    fixture(2); sources.at(40).matrix[0][0] = NAN;
    assert(compute() && admittedInputs[0].worldMatrix[3][0] == 140); // Explicit matrix wins.
    fixture(2); matrixValues.elements.emplace_back(40); expectFailure();
    fixture(0); meshValues.elements[0].object = MObject();
    assert(compute() && admittedInputs.size() == 2);
    fixture(0); meshValues.elements[0].type = MFnData::kOther; expectFailure();
    reset(); expectFailure(); // Preserve the existing no-mesh-input policy.
    fixture(0); mode = 2; expectFailure();
    fixture(0); meshValues.reportedCount = dommy::generation::kMaxConnectedGeometryCount + 1U; expectFailure();
    fixture(0); matrixValues.reportedCount = dommy::generation::kMaxConnectedGeometryCount + 1U;
    expectFailure(); assert(std::find(operations.begin(), operations.end(), "aInMatrices.jump 0") == operations.end());
    // Helpers also retain an existing staged result if a later input fails.
    fixture(0); sources.at(2).matrix[0][0] = NAN;
    MDataBlock data; std::vector<MeshInput> before = admittedInputs;
    assert(!collectMeshInputs(MObject("node"), data, DMConvertFromMeshes::inMeshes,
                             DMConvertFromMeshes::aInMatrices, before));
    assert(before.size() == 1 && before[0].meshObject.name == "previous");
    reset(); MMatrix matrix; matrix[3][0] = 123; bool found = true;
    assert(getLegacyWorldMatrix(MObject("node"), DMConvertFromMeshes::inMeshes, 40, matrix, found));
    assert(!found && matrix[3][0] == 123);
    reset();
    assert(DMConvertFromMeshes().compute(MPlug(MObject("other")), data) == MS::kUnknownParameter && calls == 0);
    std::cout << "DMConvertFromMeshes admission: sparse indices, explicit priority, absent/error distinction; "
              << injections << " status/exception injections passed\n";
}
