#pragma once

#include <maya/MStatus.h>
#include <maya/MString.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace command_host {
enum Failure { returnedFailure, standardException, unknownException, invalidParameter };
struct Fault { std::string operation; unsigned int occurrence; Failure failure; };
static std::vector<Fault> faults;
static std::map<std::string, unsigned int> calls;
static bool dagPresent, dgPresent, membership, attributes, originalAttributes;
static int colors, createdCount, reports, meshCount, geometryCount, geometryLength;
static bool stalledGeometry;
static int particleCount=2, sampledColorCount=-1, originalColorCount=2;
static std::map<int, bool> particles;
static std::vector<float> weights;

inline void reset() {
    faults.clear(); calls.clear(); particles.clear(); weights.assign(2, 0.0F);
    dagPresent = dgPresent = membership = attributes = originalAttributes = false;
    stalledGeometry = false;
    colors = createdCount = reports = 0;
    meshCount = geometryCount = geometryLength = 2;
    particleCount=originalColorCount=2; sampledColorCount=-1;
}
inline void fail(const char* operation, Failure failure, unsigned int occurrence = 1U) {
    faults.push_back(Fault{operation, occurrence, failure});
}
inline MStatus step(const std::string& operation) {
    const unsigned int occurrence = ++calls[operation];
    for (const Fault& fault : faults) {
        if (fault.operation != operation || fault.occurrence != occurrence) continue;
        if (fault.failure == standardException) throw std::bad_alloc();
        if (fault.failure == unknownException) throw 42;
        return fault.failure == invalidParameter ? MS::kInvalidParameter : MS::kFailure;
    }
    return MS::kSuccess;
}
inline unsigned int aliveParticles() {
    unsigned int result = 0;
    for (const auto& particle : particles) if (particle.second) ++result;
    return result;
}
}

namespace MFn { enum Type { kMesh, kTransform, kNurbsSurface, kCamera, kMeshVertComponent, kParticle }; }
namespace MSpace { enum Space { kWorld }; }
struct MArgList {};
struct MObject {
    int id;
    explicit MObject(int value = 0) : id(value) {}
    bool isNull() const { return id == 0; }
    bool hasFn(MFn::Type type) const { return type != MFn::kTransform; }
    static const MObject kNullObj;
};
const MObject MObject::kNullObj;
struct MObjectHandle {
    MObject value;
    MObjectHandle() : value(1) {}
    explicit MObjectHandle(MObject object) : value(object) {}
    bool isValid() const { return !value.isNull(); }
    bool isAlive() const { return isValid(); }
    MObject object() const { return value; }
};
struct MVector {
    double x, y, z;
    MVector(double a = 0, double b = 0, double c = 0) : x(a), y(b), z(c) {}
    double operator*(const MVector& other) const { return x*other.x + y*other.y + z*other.z; }
};
using MFloatVector = MVector;
struct MPoint {
    double x, y, z;
    MPoint(double a = 0, double b = 0, double c = 0) : x(a), y(b), z(c) {}
    MVector operator-(const MPoint& other) const { return MVector(x-other.x, y-other.y, z-other.z); }
    double distanceTo(const MPoint& other) const {
        const MVector delta = *this - other;
        return std::sqrt(delta * delta);
    }
};
using MFloatPoint = MPoint;

template <typename T> struct HostArray {
    std::vector<T> values;
    std::string tag;
    explicit HostArray(const char* name) : tag(name) {}
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
    T& operator[](unsigned int index) { return values.at(index); }
    const T& operator[](unsigned int index) const { return values.at(index); }
    MStatus append(const T& value) {
        const MStatus status = command_host::step(tag + ".append");
        if (status) values.push_back(value);
        return status;
    }
    MStatus clear() {
        const MStatus status = command_host::step(tag + ".clear");
        if (status) values.clear();
        return status;
    }
};
struct MObjectArray : HostArray<MObject> { MObjectArray() : HostArray("tracking") {} };
struct MStringArray : HostArray<MString> { MStringArray() : HostArray("names") {} };
struct MPointArray : HostArray<MPoint> { MPointArray() : HostArray("points") {} };
struct MFloatPointArray : HostArray<MPoint> { MFloatPointArray() : HostArray("shading.points") {} };
struct MVectorArray : HostArray<MVector> { explicit MVectorArray(const char* name = "vectors") : HostArray(name) {} };
struct MFloatVectorArray : HostArray<MVector> { MFloatVectorArray() : HostArray("shading.vectors") {} };
struct MFloatArray : HostArray<float> { explicit MFloatArray(const char* name = "floats") : HostArray(name) {} };
struct MIntArray : HostArray<int> { MIntArray() : HostArray("ids") {} };

struct MMatrix { double matrix[4][4] = {}; };
struct MFloatMatrix { MFloatMatrix(const double (*)[4]) {} };
struct MDagPath {
    int id = 1;
    bool hasFn(MFn::Type type) const { return type == MFn::kMesh || type == MFn::kCamera || type == MFn::kNurbsSurface; }
    MStatus extendToShape() { return MS::kSuccess; }
    MObject node(MStatus* status=nullptr) const {
        if (status) *status=command_host::step("dag.node");
        return MObject(id);
    }
    unsigned int instanceNumber(MStatus* status) const {
        *status=command_host::step("dag.instance"); return 0;
    }
    MMatrix inclusiveMatrix(MStatus* status) const { *status = command_host::step("matrix"); return MMatrix(); }
};
struct MPlug { MPlug elementByLogicalIndex(unsigned int, MStatus* status) { *status = command_host::step("plug.element"); return MPlug(); } };
struct MBoundingBox {
    MPoint min() const { return MPoint(); }
    MPoint max() const { return MPoint(); }
    void transformUsing(const MMatrix&) {}
};
struct MSelectionList {
    unsigned int count=0;
    MStatus add(const MString&) {
        const MStatus status=command_host::step("selection.add");
        if (status) ++count;
        return status;
    }
    MStatus clear() {
        const MStatus status=command_host::step("selection.clear");
        if (status) count=0;
        return status;
    }
    unsigned int length() const { return count; }
    MStatus getDagPath(unsigned int index, MDagPath& path, MObject&) { path.id = static_cast<int>(index)+1; return command_host::step("selection.path"); }
    MStatus getDagPath(unsigned int index, MDagPath& path) { MObject component; return getDagPath(index, path, component); }
    MStatus getDependNode(unsigned int, MObject& object) { object = MObject(1); return command_host::step("selection.node"); }
};
struct MGlobal {
    static void displayError(const MString&) { ++command_host::reports; command_host::step("report"); }
    static void displayWarning(const MString&) {}
    static MStatus getActiveSelectionList(MSelectionList& selection) {
        selection.count=2;return command_host::step("selection.active");
    }
    static MStatus getSelectionListByName(const MString&, MSelectionList& selection) {
        selection.count=2;return command_host::step("selection.name");
    }
};
struct MItSelectionList {
    int index = 0;
    MItSelectionList(const MSelectionList&, MFn::Type, MStatus* status) { *status = command_host::step("selection.iterator"); }
    bool isDone() const { return index >= command_host::meshCount; }
    MStatus getDagPath(MDagPath& path, MObject&) { path.id = index+1; return command_host::step("mesh.path"); }
    MStatus next() { const MStatus status = command_host::step("selection.next"); if (status) ++index; return status; }
};
struct M3dView {
    static M3dView active3dView() { return M3dView(); }
    MStatus getCamera(MDagPath&) { return command_host::step("camera"); }
};
struct MFnDependencyNode {
    explicit MFnDependencyNode(MObject, MStatus* status) { *status = command_host::step("dependency.bind"); }
    MPlug findPlug(const char*, bool, MStatus* status) { *status = command_host::step("plug.find"); return MPlug(); }
    MString name(MStatus* status = nullptr) { const MStatus result = command_host::step("node.name"); if (status) *status = result; return MString("node"); }
    bool hasAttribute(const char*,MStatus* status) const {
        *status=command_host::step("attribute.exists");return command_host::attributes;
    }
};
struct MFnMesh : MFnDependencyNode {
    MFnMesh(const MDagPath&, MStatus* status) : MFnDependencyNode(MObject(1), status) {}
    MBoundingBox boundingBox(MStatus* status) { *status = command_host::step("mesh.bounds"); return MBoundingBox(); }
    MStatus getClosestPointAndNormal(const MPoint& point, MPoint& closest, MVector& normal, MSpace::Space) {
        closest = point; normal = MVector(0,0,1); return command_host::step("mesh.closest");
    }
    MStatus getUVAtPoint(const MPoint&, float uv[2], MSpace::Space) { uv[0] = uv[1] = 0; return command_host::step("mesh.uv"); }
    MStatus getConnectedSetsAndMembers(unsigned int,MObjectArray& sets,MObjectArray&,bool) {
        const MStatus status=command_host::step("mesh.sets");
        if (status) sets.values={MObject(1)};
        return status;
    }
};
struct MFnNurbsSurface : MFnDependencyNode {
    MFnNurbsSurface(const MDagPath&, MStatus* status) : MFnDependencyNode(MObject(1), status) {}
};
struct MFnDagNode {
    MObject value;
    MFnDagNode(MObject object, MStatus* status) : value(object) { *status = command_host::step("particle.bind"); }
    MObject parent(unsigned int, MStatus* status) { *status = command_host::step("particle.parent"); return value; }
};
struct MDGModifier {
    std::string role;
    std::vector<MObject> deletions;
    explicit MDGModifier(const char* name = "connection") : role(name) {}
    MObject createNode(const char*, MStatus* status) { *status = command_host::step("node.queue"); return MObject(1); }
    MObject createNode(int, MStatus* status) { return createNode("node", status); }
    MStatus connect(const MPlug&, const MPlug&) { return command_host::step("connection.queue"); }
    MStatus renameNode(MObject, const char*) { return command_host::step("node.rename"); }
    MStatus deleteNode(MObject value) {
        const MStatus status = command_host::step("delete.queue");
        if (status) deletions.push_back(value);
        return status;
    }
    MStatus doIt() {
        if (role == "delete") for (MObject value : deletions) command_host::particles[value.id] = false;
        if (role == "dag") command_host::dagPresent = true;
        if (role == "dg") command_host::dgPresent = true;
        if (role == "attribute") command_host::attributes = true;
        return command_host::step(role + ".do");
    }
    MStatus undoIt() {
        if (role == "delete") for (MObject value : deletions) command_host::particles[value.id] = true;
        if (role == "dag") { command_host::dagPresent = false; command_host::membership = false; }
        if (role == "dg") command_host::dgPresent = false;
        if (role == "attribute") command_host::attributes = command_host::originalAttributes;
        return command_host::step(role + ".undo");
    }
};
struct MDagModifier : MDGModifier {
    explicit MDagModifier(const char* name = "dag") : MDGModifier(name) {}
    using MDGModifier::createNode;
    MObject createNode(const char*, MObject, MStatus* status) { return MDGModifier::createNode("node", status); }
};
struct MFnSet {
    MFnSet(MObject, MStatus* status) { *status = command_host::step("set.bind"); }
    MStatus addMember(MObject) { command_host::membership = true; return command_host::step("set.add"); }
    MStatus removeMember(MObject) { command_host::membership = false; return command_host::step("set.remove"); }
    MString name(MStatus* status) { *status=command_host::step("set.name");return "shadingGroup"; }
};
struct MFnParticleSystem {
    MFnParticleSystem() = default;
    MFnParticleSystem(MObject, MStatus* status) { *status = command_host::step("particle.bind"); }
    MObject create(MObject, MStatus* status) {
        const MObject object(++command_host::createdCount);
        command_host::particles[object.id] = true;
        *status = command_host::step("particle.create");
        return object;
    }
    MStatus emit(const MPointArray&) { return command_host::step("particle.emit"); }
    bool hasRgb() const { return false; }
    MStatus setObject(MObject) {return command_host::step("particle.setObject");}
    void position(MVectorArray& array) const {
        array.values.assign(command_host::particleCount,MVector());
    }
    unsigned int getPerParticleAttribute(const char*,MVectorArray& array,MStatus* status) const {
        *status=command_host::step("color.read");
        if (*status) array.values.assign(command_host::originalColorCount,MVector());
        return static_cast<unsigned int>(command_host::originalColorCount);
    }
    void setPerParticleAttribute(const char*, MVectorArray& array, MStatus* status) {
        if (!command_host::attributes) { *status = MS::kFailure; return; }
        command_host::colors = array.tag == "color.target" ? 1 : 0;
        *status = command_host::step(array.tag);
    }
    MStatus saveInitialState() { return command_host::step("particle.initial"); }
    MPlug findPlug(const char*, bool, MStatus* status) { *status = command_host::step("plug.find"); return MPlug(); }
    MString name(MStatus* status = nullptr) { const MStatus result = command_host::step("node.name"); if (status) *status = result; return MString("particle"); }
};
struct MItGeometry {
    int indexValue = 0;
    MItGeometry(const MDagPath&, MObject, MStatus* status) { *status = command_host::step("geometry.bind"); }
    int count(MStatus* status) { *status = command_host::step("geometry.count"); return command_host::geometryCount; }
    bool isDone() const {
        // Bound deliberately broken mutations without relying on test timeouts.
        if (command_host::calls["geometry.next"] > 32) throw std::runtime_error("fixture iteration watchdog");
        return indexValue >= command_host::geometryLength;
    }
    int index(MStatus* status) { *status = command_host::step("geometry.index"); return indexValue; }
    MPoint position(MSpace::Space, MStatus* status) { *status = command_host::step("geometry.position"); return MPoint(indexValue,0,0); }
    MStatus next() {
        const MStatus status = command_host::step("geometry.next");
        if (status && !command_host::stalledGeometry) ++indexValue;
        return status;
    }
};
struct MFnSingleIndexedComponent {
    MObject create(MFn::Type, MStatus* status) { *status = command_host::step("component.create"); return MObject(1); }
    MStatus addElements(const MIntArray&) { return command_host::step("component.elements"); }
};
struct MFnWeightGeometryFilter {
    MFnWeightGeometryFilter(MObject, MStatus* status) { *status = command_host::step("weight.bind"); }
    MStatus getWeights(const MDagPath&, MObject, MFloatArray& array) { array.values = command_host::weights; return command_host::step("weight.get"); }
    MStatus setWeight(const MDagPath&, MObject, MFloatArray& array) { command_host::weights = array.values; return command_host::step(array.tag); }
};

#include "common/utility/DCommandGuard.h"
#include "common/utility/DShapeFillPlan.h"
#include "math/DRandomGenerator.h"

namespace dommy {
inline MStatus ensureParticleRgbAttributes(const MFnParticleSystem&) { return command_host::step("particle.rgb"); }
// Attribute-queue API seam; real helper and native ownership are tested elsewhere.
inline MStatus queueMissingParticleRgbAttributes(const MFnParticleSystem&,MDGModifier&,bool& pending) {
    const MStatus status=command_host::step("attribute.queue");
    if (status) pending=!command_host::attributes;
    return status;
}
namespace legacy { const int kSpaceDuplicaterTypeId = 1, kSurfaceDuplicaterTypeId = 2; }
}
inline MStatus sampleShadingNetworkInBatches(
    const MString&, bool, bool, MFloatMatrix&, const MFloatPointArray& points,
    const MFloatArray*, const MFloatArray*, const MFloatVectorArray*,
    const MFloatPointArray*, const void*, const void*, const void*,
    MFloatVectorArray& colors, MFloatVectorArray&) {
    colors.values.assign(command_host::sampledColorCount<0 ? points.length() :
                         static_cast<unsigned int>(command_host::sampledColorCount), MVector(1,0,0));
    return command_host::step("shading");
}
struct CommandBase {
    unsigned int resultCount = 0;
    static void displayError(const MString& message) { MGlobal::displayError(message); }
    static void displayWarning(const MString& message) { MGlobal::displayWarning(message); }
    void setResult(const MString&) { command_host::step("result"); resultCount = 1; }
    void setResult(const MStringArray& values) { command_host::step("result"); resultCount = values.length(); }
};
struct shapeFillCmd : CommandBase {
    int density = 1;
    double thickness = 0, noise = 0;
    MString camName, sgName;
    bool onColor = false, onShadow = false, onReuseMap = false;
    MObjectArray createdParticleShapes;
    MDagModifier deleteModifier{"delete"};
    bool deleteOperationsQueued = false;
    unsigned int queuedDeleteCount = 0;
    bool particlesDeleted = false;
    MStatus parseArgs(const MArgList&) { return command_host::step("parse"); }
    MStatus doIt(const MArgList&);
    MStatus undoIt();
    MStatus redoIt();
};
#define NODE_COMMAND(name) struct name : CommandBase { \
    MDagModifier dagMod; MObject shadingGroup{1}, outputMeshNode{1}; \
    bool hasShadingMembership = false; \
    MStatus doIt(const MArgList&); MStatus undoIt(); MStatus redoIt(); };
NODE_COMMAND(spaceDuplicaterCmd)
NODE_COMMAND(surfaceDuplicaterCmd)
NODE_COMMAND(icosahedronCmd)
#undef NODE_COMMAND
struct icosahedronNode { static const int typeId = 3; };
struct woolCurveCmd : CommandBase {
    MDagModifier dagMod;
    MDGModifier dgMod{"dg"};
    MStatus doIt(const MArgList&); MStatus undoIt(); MStatus redoIt();
};
struct DClusterWeightCmd {
    MDagPath srcMeshPath;
    MObjectHandle clusterHandle;
    MObject componentObject{1};
    MPoint centerPt;
    double calcDistance = 2;
    MFloatArray originalWeights{"weight.original"}, targetWeights{"weight.target"};
    bool planReady = true, applied = false;
    MString sourceMeshName{"mesh"},targetClusterName{"cluster"};
    DClusterWeightCmd() { originalWeights.values.assign(2,0); targetWeights.values.assign(2,1); }
    MStatus buildWeightPlan(MObject);
    MStatus parseArgs(const MArgList&) {return command_host::step("parse");}
    MStatus doIt(const MArgList&);
    MStatus applyWeights(MFloatArray&);
    MStatus undoIt(); MStatus redoIt();
};
struct setParticleRGBCmd {
    MObjectHandle particleHandle;
    MDGModifier attributeModifier{"attribute"};
    MVectorArray originalColors{"color.original"}, targetColors{"color.target"};
    bool hadRgbAttribute = false, attributeEditsQueued = true, attributesApplied = false;
    bool planReady = true, applied = false;
    bool onShadow=false,onReuseMap=false;
    MString cameraName;
    MStatus parseArgs(const MArgList&) {return command_host::step("parse");}
    MStatus doIt(const MArgList&);
    MStatus applyColors(MVectorArray&);
    MStatus undoIt(); MStatus redoIt();
};
