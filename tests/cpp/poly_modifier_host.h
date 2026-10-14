#pragma once

#include <maya/MStatus.h>
#include <maya/MString.h>
#include <array>
#include <cassert>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace MFn { enum Type { kMesh, kTransform, kMeshPolygonComponent, kInvalid }; }
struct MFloatVector {
    float x = 0, y = 0, z = 0;
    float& operator[](unsigned int i) { return i == 0 ? x : i == 1 ? y : z; }
};
struct MObject {
    int id = 0;
    explicit MObject(int value = 0) : id(value) {}
    bool isNull() const { return id == 0; }
    bool hasFn(MFn::Type type) const { return id == 6000 && type == MFn::kMeshPolygonComponent; }
    static const MObject kNullObj;
};
const MObject MObject::kNullObj;
struct MTypeId { unsigned int id; explicit MTypeId(unsigned int value = 0) : id(value) {} };
struct MArgList {};
struct MIntArray {
    std::vector<int> values;
    MStatus clear();
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
};
struct MFloatVectorArray {};

namespace poly_host {
enum Failure { returnedFailure, standardException, unknownException, invalidParameter };
struct Fault { std::string name; unsigned int occurrence; Failure failure; };
struct Node {
    bool alive = true, shape = false, intermediate = false;
    int parent = 0, mesh = 100;
    std::map<unsigned int, MFloatVector> tweaks;
};
static std::map<int, Node> nodes;
static std::map<int, int> meshData;
static std::map<int, MFloatVector> numericData;
static std::map<std::string, unsigned int> calls;
static std::vector<Fault> faults;
static int nextNode, nextData, reports, duplicateCount;
static bool history, recordHistory, withTweaks, stalledSelection;
static unsigned int selectionCount, childCount, reportedTweakCount;
static int skippedSelection; // 1: non-mesh; 2: non-polygon component.

inline MStatus step(const std::string& name) {
    const unsigned int occurrence = ++calls[name];
    for (const Fault& fault : faults) {
        if (fault.name != name || fault.occurrence != occurrence) continue;
        if (fault.failure == standardException) throw std::bad_alloc();
        if (fault.failure == unknownException) throw 42;
        return fault.failure == invalidParameter ? MS::kInvalidParameter : MS::kFailure;
    }
    return MS::kSuccess;
}
inline void fail(const std::string& name, Failure failure, unsigned int occurrence = 1) {
    faults.push_back(Fault{name, occurrence, failure});
}
inline void reset(bool hasHistory = false, bool recording = false, bool tweaks = false) {
    nodes.clear(); meshData.clear(); numericData.clear(); calls.clear(); faults.clear();
    nextNode = 10; nextData = 10000; reports = duplicateCount = 0;
    history = hasHistory; recordHistory = recording; withTweaks = tweaks;
    stalledSelection = false; selectionCount = 1; skippedSelection = 0;
    childCount = 3; reportedTweakCount = 0;
    nodes[1] = Node(); nodes[2] = Node(); nodes[3] = Node();
    nodes[2].shape = true; nodes[2].parent = 1;
    MFloatVector tweak; tweak.x = tweaks ? 1.0F : 0.0F;
    nodes[2].tweaks[7] = tweak;
}
inline MStatus setStatus(MStatus* output, const std::string& name) {
    const MStatus status = step(name);
    if (output) *output = status;
    return status;
}
inline unsigned int aliveDuplicates() {
    unsigned int result = 0;
    for (const auto& node : nodes) if (node.first >= 10 && node.second.alive) ++result;
    return result;
}
}

inline MStatus MIntArray::clear() {
    const MStatus status = poly_host::step("faces.clear");
    if (status) values.clear();
    return status;
}

struct MObjectHandle {
    MObject value;
    MObjectHandle() = default;
    explicit MObjectHandle(MObject object) : value(object) {}
    bool isValid() const { return poly_host::nodes.count(value.id) != 0; }
    bool isAlive() const { return isValid() && poly_host::nodes.at(value.id).alive; }
    MObject object() const { return value; }
};
struct MDagPath {
    int id = 2;
    MFn::Type type = MFn::kMesh;
    MStatus extendToShape() {
        const MStatus status = poly_host::step("path.extend");
        if (type == MFn::kInvalid) return MS::kFailure;
        for (const auto& node : poly_host::nodes) {
            if (node.second.alive && node.second.shape && node.second.parent == id) { id = node.first; break; }
        }
        return status;
    }
    bool isValid() const { return poly_host::nodes.count(id) && poly_host::nodes.at(id).alive; }
    MFn::Type apiType() const { return type; }
    MObject node(MStatus* status = nullptr) const {
        if (status) poly_host::setStatus(status, "path.node");
        return MObject(id);
    }
    static MStatus getAPathTo(MObject object, MDagPath& path) {
        path.id = object.id; return poly_host::step("path.get");
    }
};

enum PlugAttribute { inMesh = 101, outMesh, pnts, tweak, output, inputPolymesh, inputComponents };
struct MPlugArray;
struct MPlug {
    int owner = 0, attr = 0, index = -1, childIndex = -1;
    MPlug() = default;
    MPlug(MObject node, MObject attribute) : owner(node.id), attr(attribute.id) {}
    bool isNull() const { return owner == 0; }
    bool isArray() const { return attr == pnts || attr == tweak; }
    bool isCompound() const { return isArray() && index >= 0; }
    bool isConnected(MStatus* status = nullptr) const {
        poly_host::setStatus(status, "plug.connected"); return attr == inMesh && poly_host::history;
    }
    unsigned int logicalIndex(MStatus* status = nullptr) const {
        poly_host::setStatus(status, "plug.index"); return static_cast<unsigned int>(index);
    }
    unsigned int numElements(MStatus* status = nullptr) const {
        poly_host::setStatus(status, "plug.count");
        return poly_host::reportedTweakCount ? poly_host::reportedTweakCount :
            static_cast<unsigned int>(poly_host::nodes.at(owner).tweaks.size());
    }
    unsigned int numChildren(MStatus* status = nullptr) const {
        poly_host::setStatus(status, "plug.children"); return poly_host::childCount;
    }
    MPlug elementByPhysicalIndex(unsigned int physical, MStatus* status = nullptr) const {
        poly_host::setStatus(status, "plug.physical");
        auto found = poly_host::nodes.at(owner).tweaks.begin();
        std::advance(found, physical);
        MPlug result = *this; result.index = static_cast<int>(found->first); return result;
    }
    MPlug elementByLogicalIndex(unsigned int logical, MStatus* status = nullptr) const {
        poly_host::setStatus(status, "plug.logical");
        MPlug result = *this; result.index = static_cast<int>(logical); return result;
    }
    MPlug child(unsigned int i, MStatus* status = nullptr) const {
        poly_host::setStatus(status, "plug.child"); MPlug result = *this;
        result.childIndex = static_cast<int>(i); return result;
    }
    MObject node(MStatus* status = nullptr) const {
        poly_host::setStatus(status, "plug.node"); return MObject(owner);
    }
    MObject attribute(MStatus* status = nullptr) const {
        poly_host::setStatus(status, "plug.attribute"); return MObject(attr);
    }
    bool connectedTo(MPlugArray&, bool, bool, MStatus* status = nullptr) const;
    MStatus getValue(MObject& value) const;
    MStatus setValue(const MObject& value);
};
struct MPlugArray {
    std::vector<MPlug> values;
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
    MPlug& operator[](unsigned int i) { return values.at(i); }
    const MPlug& operator[](unsigned int i) const { return values.at(i); }
    MStatus clear() { const MStatus status = poly_host::step("connections.clear"); if (status) values.clear(); return status; }
};
static std::map<int, int> temporaryInputs;
inline bool MPlug::connectedTo(MPlugArray& plugs, bool asDestination, bool asSource, MStatus* status) const {
    poly_host::setStatus(status, "connections.read");
    plugs.values.clear();
    if (attr == inMesh && poly_host::history) plugs.values.push_back(MPlug(MObject(3), MObject(outMesh)));
    if (attr == pnts && childIndex == 0 && poly_host::withTweaks) {
        if (asDestination) plugs.values.push_back(MPlug(MObject(3), MObject(outMesh)));
        if (asSource) plugs.values.push_back(MPlug(MObject(3), MObject(inMesh)));
    }
    return !plugs.values.empty();
}
inline MStatus MPlug::getValue(MObject& value) const {
    const MStatus status = poly_host::step(attr == pnts || attr == tweak ? "tweak.read" : "mesh.read");
    value = MObject(poly_host::nextData++);
    if (attr == pnts || attr == tweak) {
        poly_host::numericData[value.id] = poly_host::nodes.at(owner).tweaks[static_cast<unsigned int>(index)];
    } else {
        if (temporaryInputs.count(owner)) {
            poly_host::nodes.at(owner).mesh = poly_host::nodes.at(temporaryInputs.at(owner)).mesh;
        }
        poly_host::meshData[value.id] = poly_host::nodes.at(owner).mesh;
    }
    return status;
}
inline MStatus MPlug::setValue(const MObject& value) {
    if (attr == pnts || attr == tweak) {
        poly_host::nodes.at(owner).tweaks[static_cast<unsigned int>(index)] = poly_host::numericData.at(value.id);
        return poly_host::step("tweak.write");
    }
    if (attr == inputComponents) return poly_host::step("components.write");
    poly_host::nodes.at(owner).mesh = poly_host::meshData.at(value.id);
    return poly_host::step("mesh.write");
}

struct MFnDependencyNode {
    MObject object;
    MFnDependencyNode() = default;
    explicit MFnDependencyNode(MObject node, MStatus* status = nullptr) : object(node) {
        poly_host::setStatus(status, "node.bind");
    }
    MStatus setObject(MObject node) { object = node; return poly_host::step("node.set"); }
    MObject attribute(const char* name, MStatus* status = nullptr) const {
        poly_host::setStatus(status, "node.attribute");
        const std::map<std::string, int> attributes = {{"inMesh", inMesh}, {"outMesh", outMesh},
            {"pnts", pnts}, {"tweak", tweak}, {"output", output},
            {"inputPolymesh", inputPolymesh}, {"inputComponents", inputComponents}};
        return MObject(attributes.at(name));
    }
    MPlug findPlug(const char* name, bool, MStatus* status = nullptr) const {
        const std::map<std::string, int> attributes = {{"inMesh", inMesh}, {"outMesh", outMesh},
            {"pnts", pnts}, {"tweak", tweak}, {"output", output},
            {"inputPolymesh", inputPolymesh}, {"inputComponents", inputComponents}};
        const MObject attr(attributes.at(name));
        poly_host::setStatus(status, "plug.find"); return MPlug(object, attr);
    }
};
struct MFnDagNode : MFnDependencyNode {
    MFnDagNode() = default;
    explicit MFnDagNode(MObject node, MStatus* status = nullptr) : MFnDependencyNode(node, status) {}
    MObject duplicate(bool = false, bool = false, MStatus* status = nullptr) const {
        // Exceptions before the returned handle cannot be tracked by callers.
        const MStatus result = poly_host::step("duplicate");
        const int root = poly_host::nextNode++, shape = poly_host::nextNode++;
        poly_host::nodes[root] = poly_host::Node();
        poly_host::nodes[shape] = poly_host::nodes.at(object.id);
        poly_host::nodes[shape].parent = root;
        ++poly_host::duplicateCount;
        if (status) *status = result;
        return MObject(root);
    }
    unsigned int parentCount(MStatus* status = nullptr) const { poly_host::setStatus(status, "node.parents"); return 1; }
    MObject parent(unsigned int, MStatus* status = nullptr) const {
        poly_host::setStatus(status, "node.parent"); return MObject(poly_host::nodes.at(object.id).parent);
    }
    unsigned int childCount(MStatus* status = nullptr) const { poly_host::setStatus(status, "node.children"); return 1; }
    MObject child(unsigned int, MStatus* status = nullptr) const {
        poly_host::setStatus(status, "node.child");
        for (const auto& node : poly_host::nodes) if (node.second.parent == object.id) return MObject(node.first);
        return MObject();
    }
    MStatus setIntermediateObject(bool value) { poly_host::nodes.at(object.id).intermediate = value; return poly_host::step("node.intermediate"); }
    MStatus getPath(MDagPath& path) const { path.id = object.id; return poly_host::step("node.path"); }
};
struct MFnNumericData {
    enum Type { k3Float };
    MObject value;
    MFnNumericData() = default;
    MFnNumericData(MObject object, MStatus* status) : value(object) { poly_host::setStatus(status, "numeric.bind"); }
    MObject create(Type, MStatus* status) {
        poly_host::setStatus(status, "numeric.create"); value = MObject(poly_host::nextData++);
        poly_host::numericData[value.id] = MFloatVector(); return value;
    }
    MStatus getData(float& x, float& y, float& z) {
        const MFloatVector vector = poly_host::numericData.at(value.id);
        x = vector.x; y = vector.y; z = vector.z; return poly_host::step("numeric.read");
    }
    MStatus setData(float x, float y, float z) {
        MFloatVector& vector = poly_host::numericData.at(value.id);
        vector.x = x; vector.y = y; vector.z = z; return poly_host::step("numeric.write");
    }
};
struct MDGModifier {
    std::vector<int> created;
    std::vector<std::pair<MPlug, MPlug>> connections;
    bool temporary = false;
    std::vector<std::pair<MPlug, MObject>> values;
    std::vector<MFloatVector> oldValues;
    MStatus newPlugValue(const MPlug& plug, MObject& value) {
        const MStatus status = poly_host::step("value.queue");
        if (status) {
            values.push_back({plug, value});
            oldValues.push_back(poly_host::nodes.at(plug.owner).tweaks.at(plug.index));
        }
        return status;
    }
    MObject createNode(MTypeId, MStatus* status) { return createNode("polyBlast", status); }
    MObject createNode(const MString&, MStatus* status) { return createNode("polyBlast", status); }
    MObject createNode(const char*, MStatus* status = nullptr) {
        const int node = poly_host::nextNode++;
        poly_host::nodes[node] = poly_host::Node(); created.push_back(node);
        poly_host::setStatus(status, "dg.create"); return MObject(node);
    }
    MStatus connect(const MPlug& source, const MPlug& target) {
        const MStatus status = poly_host::step("connection.queue");
        if (status) connections.push_back({source, target});
        if (target.owner == 2 && source.owner >= 10 && poly_host::nodes.at(source.owner).shape) temporary = true;
        return status;
    }
    MStatus disconnect(const MPlug&, const MPlug&) { return poly_host::step("disconnect.queue"); }
    MStatus doIt() {
        for (const auto& item : values)
            poly_host::nodes.at(item.first.owner).tweaks[item.first.index] = poly_host::numericData.at(item.second.id);
        for (int node : created) poly_host::nodes[node].alive = true;
        if (temporary) {
            for (const auto& edge : connections) temporaryInputs[edge.second.owner] = edge.first.owner;
        } else if (!created.empty()) poly_host::nodes[2].mesh = 50;
        return poly_host::step(temporary ? "temp.do" : "dg.do");
    }
    MStatus undoIt() {
        for (std::size_t i = 0; i < values.size(); ++i)
            poly_host::nodes.at(values[i].first.owner).tweaks[values[i].first.index] = oldValues[i];
        for (int node : created) poly_host::nodes[node].alive = false;
        if (temporary) for (const auto& edge : connections) temporaryInputs.erase(edge.second.owner);
        else if (!created.empty()) poly_host::nodes[2].mesh = 100;
        return poly_host::step(temporary ? "temp.undo" : "dg.undo");
    }
};
struct MDagModifier : MDGModifier {
    int shape = 0, originalParent = 0, newParent = 0, deletedRoot = 0;
    MStatus reparentNode(MObject node, MObject parent) {
        shape = node.id; originalParent = poly_host::nodes.at(shape).parent; newParent = parent.id;
        return poly_host::step("reparent.queue");
    }
    MStatus deleteNode(MObject node) { deletedRoot = node.id; return poly_host::step("dag.delete.queue"); }
    MStatus doIt() {
        if (shape) poly_host::nodes.at(shape).parent = newParent;
        if (deletedRoot) poly_host::nodes.at(deletedRoot).alive = false;
        return poly_host::step("dag.do");
    }
    MStatus undoIt() {
        if (shape) poly_host::nodes.at(shape).parent = originalParent;
        if (deletedRoot) poly_host::nodes.at(deletedRoot).alive = true;
        return poly_host::step("dag.undo");
    }
};
struct MSelectionList {
    unsigned int length(MStatus* status = nullptr) const { poly_host::setStatus(status, "selection.count"); return poly_host::selectionCount; }
};
struct MItSelectionList {
    unsigned int position = 0;
    explicit MItSelectionList(const MSelectionList&, MFn::Type = MFn::kInvalid,
                             MStatus* status = nullptr) {
        poly_host::setStatus(status, "selection.construct");
    }
    MStatus setFilter(MFn::Type) { return poly_host::step("selection.filter"); }
    bool isDone() const { return position >= poly_host::selectionCount; }
    MStatus next() {
        // A fixture-only ceiling prevents a deliberately broken loop hanging
        // the mutation run. The production-bound assertion still requires one
        // advance, not this emergency exit.
        if (poly_host::stalledSelection && poly_host::calls["selection.next"] > 8) throw 42;
        if (!poly_host::stalledSelection) ++position;
        return poly_host::step("selection.next");
    }
    MStatus getDagPath(MDagPath& path, MObject& component) {
        path.id = 2; path.type = position == 0 && poly_host::skippedSelection == 1 ? MFn::kInvalid : MFn::kMesh;
        component = MObject(position == 0 && poly_host::skippedSelection == 2 ? 6001 :
                            position == 0 && poly_host::skippedSelection == 3 ? 6000 : 0);
        return poly_host::step("selection.path");
    }
};
struct MGlobal {
    static void displayError(const MString&) { ++poly_host::reports; }
    static void displayWarning(const MString&) { ++poly_host::reports; }
    static MStatus getActiveSelectionList(MSelectionList&) { return poly_host::step("selection.active"); }
    static MStatus executeCommand(const char*, int& result) { result = poly_host::recordHistory ? 1 : 0; return poly_host::step("history.query"); }
    static MStatus deleteNode(MObject& object) {
        poly_host::nodes.at(object.id).alive = false;
        for (auto& node : poly_host::nodes) if (node.second.parent == object.id) node.second.alive = false;
        return poly_host::step("node.delete");
    }
};
struct MPxCommand {
    virtual ~MPxCommand() = default;
    virtual bool isUndoable() const { return false; }
    virtual MStatus doIt(const MArgList&) { return MS::kFailure; }
    virtual MStatus undoIt() { return MS::kFailure; }
    virtual MStatus redoIt() { return MS::kFailure; }
    static void setResult(const char*) {
        const MStatus status = poly_host::step("result");
        if (!status) throw std::runtime_error("result publication failed");
    }
};
struct MFnComponentListData {
    MObject create(MStatus* status) { poly_host::setStatus(status, "components.create"); return MObject(5000); }
};
struct MFnMesh {
    MFnMesh() = default;
    MFnMesh(const MDagPath&, MStatus* status) { poly_host::setStatus(status, "mesh.bind"); }
    int numPolygons(MStatus* status) { poly_host::setStatus(status, "mesh.polygons"); return 4; }
    MObject copy(const MObject& source, const MObject& destination, MStatus* status) {
        poly_host::meshData[destination.id] = poly_host::meshData.at(source.id);
        poly_host::setStatus(status, "mesh.copy"); return destination;
    }
};
struct MFnMeshData {
    MObject create(MStatus* status) {
        MObject data(poly_host::nextData++);
        poly_host::meshData[data.id] = 0;
        poly_host::setStatus(status, "mesh.data.create"); return data;
    }
};

#include "basePolyModifierCmd.h"
#include "polyBlast/polyBlastCmd.h"
#include "common/utility/DCommandGuard.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DMeshSelection.h"

inline basePolyModifierFty::basePolyModifierFty() {}
inline basePolyModifierFty::~basePolyModifierFty() {}
inline void polyBlastFty::setMesh(const MObject& mesh) { fMesh = mesh; }
inline void polyBlastFty::setFaces(const MIntArray& ids) { pSelFaces = ids; }
inline MStatus polyBlastFty::doIt() { poly_host::nodes.at(fMesh.id).mesh /= 2; return poly_host::step("direct.edit"); }
struct polyBlastNode { static MTypeId id; };
MTypeId polyBlastNode::id(123);
namespace dommy { namespace poly_blast {
inline MStatus readPolygonFaceComponent(const MObject&, std::size_t, std::size_t, std::vector<int>& ids) { ids = {0, 1}; return MS::kSuccess; }
inline MStatus buildPolygonComponentList(const std::vector<int>&, MObject& value) { value = MObject(5000); return MS::kSuccess; }
inline MStatus copyFaceIdsToMaya(const std::vector<int>& ids, MIntArray& target) { target.values = ids; return MS::kSuccess; }
}}
