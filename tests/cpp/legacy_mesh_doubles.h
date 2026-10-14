#pragma once

// Only Maya boundaries are doubled. Production compute/helper bodies and the
// spatial search execute unchanged. This is not a native mesh/DG emulator.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DAttributeBlendMath.h"
#include "common/utility/DBoundedRadiusSearch.h"
#include <map>
#include <iostream>

#define CHECK_MSTATUS_AND_RETURN_IT(s) DOMMY_RETURN_IF_FAILURE(s)
inline void EchoWarning(const char*, const char*) {}
namespace MSpace { enum Space { kObject, kWorld }; }
namespace {
int throwAt = -1, shortAt = -1, published = 0, cleaned = 0;
bool badIndex = false, stuckIterator = false, earlyIterator = false, invalidRamp = false;
bool nullCreated = false, nullCopied = false, disconnected = false;
bool existingColorChannels = false;
bool constantRamp = false;
float rampValue = 0.0f;
double normalValue = 1.0, edgeLengthScale = 1.0;
float hitValue = 1.0f;
bool missRay = false, shortenAllPoints = false;
std::vector<std::string> outputEvents;
MStatus api(const std::string& name) {
    const int at = callCount;
    const MStatus status = operation(name);
    if (at == throwAt) throw std::runtime_error("injected native failure");
    return status;
}
}

struct MVector {
    double x, y, z;
    MVector(double x = 0, double y = 0, double z = 0):x(x), y(y), z(z) {}
    MVector operator*(double t) const { return {x*t, y*t, z*t}; }
    MVector operator-() const { return {-x, -y, -z}; }
    double length() const { return std::sqrt(x*x + y*y + z*z); }
    MStatus normalize() {
        const auto status = api("vector.normalize");
        if (status) { const double d = length(); x /= d; y /= d; z /= d; }
        return status;
    }
};
struct MPoint {
    double x, y, z, w;
    MPoint(double x = 0, double y = 0, double z = 0):x(x), y(y), z(z), w(1) {}
    double& operator[](unsigned i) { assert(i < 4); return i == 0 ? x : i == 1 ? y : i == 2 ? z : w; }
    double operator[](unsigned i) const { assert(i < 4); return i == 0 ? x : i == 1 ? y : i == 2 ? z : w; }
    MVector operator-(const MPoint& p) const { return {x-p.x, y-p.y, z-p.z}; }
    MPoint operator+(const MVector& v) const { return {x+v.x, y+v.y, z+v.z}; }
    double distanceTo(const MPoint& p) const {
        const auto v = *this-p; return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    }
    bool operator==(const MPoint& p) const { return x==p.x && y==p.y && z==p.z && w==p.w; }
};
struct MColor {
    float r, g, b, a;
    MColor(float r = 0, float g = 0, float b = 0, float a = 1):r(r), g(g), b(b), a(a) {}
    float& operator[](unsigned i) { assert(i < 4); return i == 0 ? r : i == 1 ? g : i == 2 ? b : a; }
};
struct MFloatPoint {
    float x = 0, y = 0, z = 0;
    MFloatPoint() = default;
    explicit MFloatPoint(const MPoint& p):x(float(p.x)),y(float(p.y)),z(float(p.z)) {
        assert(std::isfinite(x) && std::isfinite(y) && std::isfinite(z));
    }
};
struct MFloatVector {
    float x, y, z;
    explicit MFloatVector(const MVector& v):x(float(v.x)),y(float(v.y)),z(float(v.z)) {
        assert(std::isfinite(x) && std::isfinite(y) && std::isfinite(z));
    }
};
struct MMeshIsectAccelParams {};
template<class T> struct Array : std::vector<T> {
    using std::vector<T>::vector;
    unsigned int length() const { return static_cast<unsigned int>(this->size()); }
    T& operator[](std::size_t i) { assert(i < this->size()); return this->at(i); }
    const T& operator[](std::size_t i) const { assert(i < this->size()); return this->at(i); }
    MStatus copy(const Array& other) {
        const int at = callCount; const auto s = api("array.copy");
        if (s) { this->assign(other.begin(), other.end());
            if (at == shortAt && !this->empty()) this->pop_back(); }
        return s;
    }
    MStatus setLength(unsigned n) {
        const int at = callCount; const auto s = api("array.length");
        if (s) this->resize(at == shortAt && n ? n-1 : n);
        return s;
    }
    MStatus append(const T& value) { const auto s = api("array.append"); if (s) this->push_back(value); return s; }
};
using MPointArray = Array<MPoint>;
using MColorArray = Array<MColor>;
using MDoubleArray = Array<double>;
using MFloatArray = Array<float>;
using MIntArray = Array<int>;
using MStringArray = Array<MString>;

struct NativeMesh {
    MPointArray points;
    MColorArray colors;
    int reportedPoints = -1, reportedColors = -1;
    int reportedEdges = -1;
    std::vector<std::pair<int, int>> edges = {{0, 1}, {1, 2}, {2, 3}, {3, 4}};
};
namespace {
std::map<std::string, NativeMesh> meshes;
std::map<std::string, MObject> outputs;
std::vector<MSpace::Space> positionSpaces;
std::vector<float> rampPositions;
int meshSerial = 0;
}
struct MPlug {
    std::string name;
    MPlug(const MObject&, const MObject& attr):name(attr.name) {}
    bool operator==(const MObject& attr) const { return name == attr.name; }
    bool operator!=(const MObject& attr) const { return name != attr.name; }
    bool isConnected(MStatus* status) const { *status = api("plug.connected"); return !disconnected; }
};
struct MDataHandle {
    bool valid;
    std::string name;
    double value;
    MObject object;
    void check() const { assert(valid); }
    double asDouble() const { check(); return value; }
    short asShort() const { check(); return static_cast<short>(value); }
    int asInt() const { check(); return static_cast<int>(value); }
    bool asBool() const { check(); return value != 0; }
    MFnData::Type type() const { check(); return MFnData::kMesh; }
    MObject data() const { check(); return object; }
    MStatus set(const MObject& mesh) {
        check(); const auto s = api("output.set."+name);
        if (s) { outputs[name] = mesh; ++published; outputEvents.push_back("set."+name); }
        return s;
    }
    void setClean() { check(); ++cleaned; }
};
struct MDataBlock {
    std::map<std::string, double> controls;
    std::map<std::string, MObject> inputs;
    MDataHandle inputValue(const MObject& attr, MStatus* status) {
        *status = api("input."+attr.name);
        return {bool(*status), attr.name, controls[attr.name], inputs[attr.name]};
    }
    MDataHandle outputValue(const MObject& attr, MStatus* status) {
        *status = api("output.handle."+attr.name);
        outputEvents.push_back("handle."+attr.name);
        return {bool(*status), attr.name, 0.0, {""}};
    }
    MStatus setClean(const MPlug&) { const auto s = api("data.clean"); if (s) ++cleaned; return s; }
};
struct MFnMeshData {
    MObject create(MStatus* status) {
        *status = api("mesh.create");
        if (!*status || nullCreated) return MObject::kNullObj;
        MObject object{"created"+std::to_string(meshSerial++)}; meshes[object.name] = NativeMesh(); return object;
    }
};
struct MFnMesh {
    MObject object;
    MFnMesh() = default;
    MFnMesh(const MObject& o, MStatus* status):object(o) { *status = api("mesh.bind"); }
    int numVertices(MStatus* status) const {
        *status = api("mesh.count");
        const auto& m = meshes.at(object.name);
        return m.reportedPoints == -1 ? int(m.points.size()) : m.reportedPoints;
    }
    int numColors(MStatus* status) const {
        *status = api("mesh.colorCount");
        const auto& m = meshes.at(object.name);
        return m.reportedColors == -1 ? int(m.colors.size()) : m.reportedColors;
    }
    MStatus getPoints(MPointArray& out, MSpace::Space space) {
        const int at = callCount; const auto s = api("mesh.points");
        if (s) { out = meshes.at(object.name).points;
            if (space == MSpace::kWorld) for (auto& p : out) p.y += 10;
            if ((at == shortAt || shortenAllPoints) && !out.empty()) out.pop_back(); }
        return s;
    }
    MStatus getVertexColors(MColorArray& out) {
        const int at = callCount; const auto s = api("mesh.colors");
        if (s) { out = meshes.at(object.name).colors;
            if (at == shortAt && !out.empty()) out.pop_back(); }
        return s;
    }
    MObject copy(const MObject& source, const MObject& destination, MStatus* status) {
        *status = api("mesh.copy");
        if (!*status || nullCopied) return MObject::kNullObj;
        object = destination; meshes[object.name] = meshes.at(source.name); return destination;
    }
    MStatus setPoints(const MPointArray& points, MSpace::Space) {
        const auto s = api("mesh.setPoints"); if (s) meshes.at(object.name).points = points; return s;
    }
    MStatus setVertexColors(const MColorArray& colors, const MIntArray& ids) {
        const auto s = api("mesh.setColors");
        assert(colors.length() == ids.length()); if (s) meshes.at(object.name).colors = colors; return s;
    }
    MStatus updateSurface() { return api("mesh.update"); }
    int numEdges(MStatus* status) {
        *status = api("mesh.edges"); const auto& m = meshes.at(object.name);
        return m.reportedEdges == -1 ? int(m.edges.size()) : m.reportedEdges;
    }
    static MMeshIsectAccelParams autoUniformGridParams() { return {}; }
    void freeCachedIntersectionAccelerator() {}
    bool closestIntersection(const MFloatPoint&, const MFloatVector&, const MIntArray*, const MIntArray*,
        bool, MSpace::Space, float distance, bool, MMeshIsectAccelParams*, MFloatPoint&, float* parameter,
        int*, int*, float*, float*, float, MStatus* status) {
        *status = api("mesh.ray"); assert(std::isfinite(distance));
        if (*status) *parameter = hitValue;
        return !missRay;
    }
    MStatus setDisplayColors(bool) { return MS::kFailure; }
    bool hasColorChannels(const MString&, MStatus* status) { *status = api("mesh.hasColors"); return existingColorChannels; }
    int numPolygons(MStatus* status) { *status = api("mesh.faces"); return meshes.at(object.name).points.empty() ? 0 : 1; }
    int numFaceVertices(MStatus* status) { *status = api("mesh.corners"); return int(meshes.at(object.name).points.size()); }
    MStatus getVertices(MIntArray& counts, MIntArray& ids) {
        const int at = callCount; const auto s = api("mesh.topology");
        if (s) {
            const auto count = meshes.at(object.name).points.size();
            if (count) counts.push_back(int(count));
            for (std::size_t i = 0; i < count; ++i) ids.push_back(int(i));
            if (at == shortAt && !ids.empty()) ids.pop_back();
        } return s;
    }
    MStatus getColorSetNames(Array<MString>& names) {
        const auto s = api("mesh.colorSets");
        if (s && existingColorChannels) names.push_back("skinInfo");
        return s;
    }
    MStatus createColorSetDataMesh(MString&) { return api("mesh.createColors"); }
    MStatus clearColors(const MString* name) { assert(name); return api("mesh.clearColors"); }
    MStatus setColors(const MColorArray& colors, const MString* name) {
        assert(name); const auto s = api("mesh.setColors");
        if (s) meshes.at(object.name).colors = colors; return s;
    }
    MStatus assignColors(const MIntArray& ids, const MString* name) {
        assert(name); const auto s = api("mesh.assignColors");
        if (s) for (const int id : ids) assert(id >= 0 && unsigned(id) < meshes.at(object.name).colors.length());
        return s;
    }
    MStatus setCurrentColorSetName(const MString&) { return MS::kFailure; }
};
struct MItMeshVertex {
    MObject object;
    std::size_t ordinal = 0;
    MItMeshVertex(const MObject& o, MStatus* status):object(o) { *status = api("vertex.bind"); }
    bool isDone(MStatus* status) const {
        *status = api("vertex.done");
        return ordinal >= meshes.at(object.name).points.size() - (earlyIterator ? 1U : 0U);
    }
    MStatus next() { const auto s = api("vertex.next"); if (s && !stuckIterator) ++ordinal; return s; }
    int index(MStatus* status) const { *status = api("vertex.index"); return badIndex ? -1 : int(ordinal); }
    MStatus getNormal(MVector& normal, MSpace::Space) {
        const auto status = api("vertex.normal"); if (status) normal = MVector(0, normalValue, 0); return status;
    }
    MPoint position(MSpace::Space space, MStatus* status) const {
        *status = api("vertex.position"); positionSpaces.push_back(space);
        auto point = meshes.at(object.name).points.at(ordinal);
        if (space == MSpace::kWorld) point.y += 10;
        return point;
    }
    MStatus setPosition(const MPoint& point, MSpace::Space space = MSpace::kObject) {
        const auto s = api("vertex.set"); if (s) {
            auto local = point; if (space == MSpace::kWorld) local.y -= 10;
            meshes.at(object.name).points.at(ordinal) = local;
        } return s;
    }
};
struct MItMeshEdge {
    MObject object;
    std::size_t ordinal = 0;
    MItMeshEdge(const MObject& o, MStatus* status):object(o) { *status = api("edge.bind"); }
    bool isDone(MStatus* status) const {
        *status = api("edge.done");
        const auto count = meshes.at(object.name).edges.size();
        return ordinal >= (earlyIterator && count ? count - 1U : count);
    }
    MStatus next() { const auto s = api("edge.next"); if (s && !stuckIterator) ++ordinal; return s; }
    int index(MStatus* status) const { *status = api("edge.index"); return badIndex ? -1 : int(ordinal); }
    int index(int endpoint, MStatus* status) const {
        *status = api("edge.vertex"); if (badIndex) return -1;
        const auto& edge = meshes.at(object.name).edges.at(ordinal); return endpoint ? edge.second : edge.first;
    }
    MStatus getLength(double& length) const {
        const auto status = api("edge.length");
        if (status) {
            const auto& mesh = meshes.at(object.name); const auto& edge = mesh.edges.at(ordinal);
            length = mesh.points.at(edge.first).distanceTo(mesh.points.at(edge.second)) * edgeLengthScale;
        } return status;
    }
};
struct MeshRamp {
    MeshRamp(const MPlug&, MStatus* status) { *status = api("ramp.bind"); }
    void getValueAtPosition(float position, float& value, MStatus* status = nullptr) {
        const auto s = api("ramp.sample"); if (status) *status = s;
        if (s) { rampPositions.push_back(position);
            value = invalidRamp ? NAN : constantRamp ? rampValue : 0.5f + position*0.5f; }
    }
};
#define MRampAttribute MeshRamp

inline MDataBlock resetMeshRun(int failure = -1) {
    resetCalls(failure); throwAt = shortAt = -1; published = cleaned = 0;
    badIndex = stuckIterator = earlyIterator = invalidRamp = false;
    nullCreated = nullCopied = disconnected = false;
    existingColorChannels = false;
    constantRamp = false; rampValue = 0.0f;
    normalValue = edgeLengthScale = 1.0; hitValue = 1.0f; missRay = shortenAllPoints = false;
    meshes.clear(); outputs.clear(); outputEvents.clear(); positionSpaces.clear(); rampPositions.clear(); meshSerial = 0;
    meshes["base"].points = {{-2,0,0},{-0.5,0,0},{0,0,0},{0.5,0,0},{2,0,0}};
    meshes["target"].points = {{-3,2,0},{-1,2,0},{0,2,0},{1,2,0},{3,2,0}};
    meshes["target"].colors = {{0},{0.25f},{0.5f},{0.75f},{1}};
    meshes["mask"] = meshes["target"];
    MDataBlock block;
    block.inputs = {{"inBaseMesh", {"base"}}, {"inTargetMesh", {"target"}}, {"inMaskMesh", {"mask"}}};
    block.controls = {{"aDistance",1}, {"aTolerance",0.01}, {"aWeight",0.5},
        {"aUseMask",1}, {"aUseColor",1}, {"aType",0}, {"aSpace",0}, {"aInvert",0}};
    return block;
}
