#pragma once
// API-shaped failure injection; not a renderer or a Maya runtime substitute.
#include <maya/MStatus.h>
#include <maya/MMatrix.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <memory>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

std::vector<std::string> operations;
std::vector<std::string> debugMessages;
int diagnosticReports=0;
int apiErrorReports=0;
std::map<int,bool> connectionValues;
int failAt = -1, failureKind = 0, shortAt = -1;
void* activeRenderItem=nullptr;
void* failedShaderItem=nullptr;
bool api(const char* name) {
    operations.push_back(name);
    assert(operations.size() < 500U);
    if (static_cast<int>(operations.size()) - 1 != failAt) return true;
    if (std::string(name)=="render.parameter" || std::string(name)=="render.shader")
        failedShaderItem=activeRenderItem;
    if (failureKind == 2) throw std::bad_alloc();
    if (failureKind == 3) throw std::runtime_error("injected failure");
    return false;
}
void reset() { operations.clear(); debugMessages.clear(); connectionValues.clear(); diagnosticReports=apiErrorReports=0; failAt = shortAt = -1; failureKind = 0; activeRenderItem=failedShaderItem=nullptr; }
void recordDebugMessage(const std::string& message) {
    if (!api("debug.message")) throw std::runtime_error("diagnostic output");
    debugMessages.push_back(message);
}
void result(MStatus* status, bool success) { if (status) *status = success ? MS::kSuccess : MS::kFailure; }
class MString {
    std::string value;
public:
    MString(const char* text = "") : value(text) {}
    bool operator==(const MString& other) const { return value == other.value; }
    MString toLowerCase() const { return *this; }
    const char* asChar() const { return value.c_str(); }
    MString operator+(const MString& other) const { return MString((value+other.value).c_str()); }
};
struct MColor {
    float r, g, b, a;
    MColor(float red=0, float green=0, float blue=0, float alpha=1)
        : r(red), g(green), b(blue), a(alpha) {}
};
template <typename T> struct Coordinate {
    T x, y, z, w=1;
    Coordinate(T a=0, T b=0, T c=0) : x(a), y(b), z(c) {}
    explicit Coordinate(const double* values) : x(values[0]), y(values[1]), z(values[2]) {}
    T& operator[](std::size_t i) { assert(i<3); return i==0?x:(i==1?y:z); }
    T operator[](std::size_t i) const { assert(i<3); return i==0?x:(i==1?y:z); }
    double length() const { return std::sqrt(double(x)*x+double(y)*y+double(z)*z); }
    Coordinate operator/(double n) const { return Coordinate(x/n,y/n,z/n); }
};
using MPoint = Coordinate<double>;
using MVector = Coordinate<double>;
using MFloatVector = Coordinate<float>;
struct MBoundingBox {
    MPoint low, high;
    MBoundingBox() = default;
    MBoundingBox(const MPoint& minimum, const MPoint& maximum) : low(minimum), high(maximum) {}
    MPoint min() const { return low; }
    MPoint max() const { return high; }
};
template <typename T> std::ostream& operator<<(std::ostream& stream,const Coordinate<T>& value) {
    return stream<<value.x<<','<<value.y<<','<<value.z;
}
MPoint operator*(const MPoint& p, const MMatrix& matrix) {
    MPoint out;
    for (int j=0;j<3;++j) out[j]=p[0]*matrix[0][j]+p[1]*matrix[1][j]+p[2]*matrix[2][j]+matrix[3][j];
    return out;
}
struct MObject {
    int key=0;
    explicit MObject(int id=0):key(id){}
    bool isNull()const{return key==-1;}
    static const MObject kNullObj;
};
const MObject MObject::kNullObj(-1);
struct SettingValue { bool boolean=true; short mode=1; int integer=-7; double number=2.5; MString text{"attribute"}; };
std::map<int,SettingValue> settingValues;
bool reachedSettingsData=false, failDiagnosticOutput=false;
bool useConnectionAdmission=false;
volatile bool settingsObservation=false;
void observeSettings(bool value){settingsObservation=value;}
struct MDagPath {};
using float2=float[2];
using DPointList = std::vector<MVector>;
struct DPData {
    static const int id = 1;
    std::size_t count=0;
    DPointList points;
    bool nullPositions=false;
    std::size_t numberOfPoints() const { return count; }
    bool hasValidPointTopology() const { return points.size()==count; }
    const DPointList* getPositionPtrConst() const {
        if (!api("point.positions")) throw std::runtime_error("positions");
        return nullPositions ? nullptr : &points;
    }
};
const DPData* inputPointData=nullptr;
bool pointConnected=true;
double inputPointSize=3.0;
MColor inputPointColor(1,1,1,1);
struct DPointDisplay { static MObject inDP, aPointSize, aSolidColor; };
MObject DPointDisplay::inDP, DPointDisplay::aPointSize, DPointDisplay::aSolidColor;
struct MPlug {
    MObject attr;
    MPlug(const MObject&,const MObject& object):attr(object) {}
    bool isNull() const { return attr.isNull(); }
    MObject attribute(MStatus* status=nullptr)const {
        const bool ok=api("plug.attribute");result(status,ok);
        return ok?attr:MObject::kNullObj;
    }
    bool isConnected(MStatus* status=nullptr) const {
        result(status,api("plug.connected"));
        const auto value=connectionValues.find(attr.key);
        return value==connectionValues.end()?pointConnected:value->second;
    }
    MString name()const{return "plug";}
    MObject asMObject(MStatus* status=nullptr) const {
        const bool ok=api("vdb.color.object"); result(status,ok); return MObject();
    }
    bool asBool(MStatus* status=nullptr) const {
        const bool ok=api("vdb.drawHeads"); result(status,ok); return true;
    }
    float asFloat(MStatus* status=nullptr) const {
        const bool ok=api("vdb.scale"); result(status,ok); return 1.0f;
    }
};
struct MFnDependencyNode {
    MObject node;
    MFnDependencyNode(const MObject& value,MStatus* status=nullptr):node(value){result(status,api("instancer.bind"));}
    MPlug findPlug(const MObject& attr,bool,MStatus* status=nullptr)const{
        result(status,api("instancer.find"));return MPlug(node,attr);
    }
    MString name()const{return "instancer";}
};
void EchoWarning(const char*,const char*){++apiErrorReports;if(failDiagnosticOutput)throw std::bad_alloc();}
struct DOpenVDBDisplayVP2 { static MObject aVectorColor,aIsDrawHeads,aVectorScale,aPointSize; };
MObject DOpenVDBDisplayVP2::aVectorColor,DOpenVDBDisplayVP2::aIsDrawHeads,
        DOpenVDBDisplayVP2::aVectorScale,DOpenVDBDisplayVP2::aPointSize;
struct MFnNumericData {
    MFnNumericData(const MObject&,MStatus* status=nullptr) { result(status,api("vdb.color.data")); }
    MStatus getData(float& r,float& g,float& b) const {
        if (!api("vdb.color.read")) return MS::kFailure;
        r=g=b=1.0f; return MS::kSuccess;
    }
};
MStatus readPlugValue(const MObject&,const MObject&,MColor& output) {
    if (!api("point.color")) return MS::kFailure;
    output=inputPointColor; return MS::kSuccess;
}
MStatus readPlugValue(const MObject&,const MObject& attribute,double& output) {
    if(attribute.key) {
        if(!api(("setting."+std::to_string(attribute.key)).c_str())) return MS::kFailure;
        output=settingValues[attribute.key].number;return MS::kSuccess;
    }
    if (!api("point.size")) return MS::kFailure;
    output=inputPointSize; return MS::kSuccess;
}
template <typename T> MStatus readSetting(const MObject& attribute,T& output,const T& value) {
    if(!api(("setting."+std::to_string(attribute.key)).c_str()))return MS::kFailure;
    output=value;return MS::kSuccess;
}
MStatus readPlugValue(const MObject&,const MObject& a,bool& v){return readSetting(a,v,settingValues[a.key].boolean);}
MStatus readPlugValue(const MPlug& p,short& v){return readSetting(p.attr,v,settingValues[p.attr.key].mode);}
MStatus readPlugValue(const MObject&,const MObject& a,int& v){return readSetting(a,v,settingValues[a.key].integer);}
MStatus readPlugValue(const MObject&,const MObject& a,MString& v){return readSetting(a,v,settingValues[a.key].text);}
MStatus readPlugValue(const MPlug& p,int& v){return readPlugValue(MObject(),p.attr,v);}
MStatus readPlugValue(const MPlug& p,double& v){return readPlugValue(MObject(),p.attr,v);}
MStatus readPlugValue(const MPlug& p,MString& v){return readPlugValue(MObject(),p.attr,v);}
using double3=double[3];using double2=double[2];
struct MDataHandle {
    double3 triple={1,2,3};double2 pair={4,5};
    MVector asVector()const{return MVector(triple);}
    const double3& asDouble3()const{return triple;}
    const double2& asDouble2()const{return pair;}
};
namespace dommy { namespace maya {
MStatus productionReportEntryPointException(const char*,const char*,const std::exception_ptr&) noexcept;
struct ScopedPlugDataHandle {
    MDataHandle data;
    MDataHandle& handle(){return data;}
    MStatus read(const MPlug& plug){return api(("handle."+std::to_string(plug.attr.key)).c_str())?MS::kSuccess:MS::kFailure;}
};
template <typename T> MStatus getPluginDataFromPlug(
    const MPlug&,int,MObject&,const T*& output) {
    output=nullptr;
    if (!api("point.input") || !inputPointData) return MS::kFailure;
    output=inputPointData; return MS::kSuccess;
}
inline void reportViewportCallbackException(
    const char* owner,const char* callback,const std::exception_ptr& error) noexcept {
    ++diagnosticReports;productionReportEntryPointException(owner,callback,error);
}
}}
namespace tbb {
std::function<void()> pointStagingObserver;
template <typename T> struct blocked_range {
    T first,last;
    blocked_range(T begin,T end,std::size_t) : first(begin),last(end) {
        if (!api("point.range")) throw std::runtime_error("range");
    }
    T begin() const { return first; } T end() const { return last; }
};
template <typename R,typename F> void parallel_for(const R& range,F function) {
    if (pointStagingObserver) pointStagingObserver();
    if (!api("point.parallel")) throw std::runtime_error("parallel");
    function(range);
    if (pointStagingObserver) pointStagingObserver();
    if (!api("point.parallel.after")) throw std::runtime_error("parallel.after");
}
}
std::size_t DGetGlobalProcessNum(std::size_t) { return 1U; }
namespace MSpace { enum Space { kObject, kWorld }; }
struct MGlobal { static void displayError(const MString&) {++apiErrorReports;if(failDiagnosticOutput)throw std::bad_alloc();} };
template <typename T> struct Array {
    std::vector<T> values;
    unsigned int length() const { return static_cast<unsigned int>(values.size()); }
    T& operator[](unsigned int i) { return values.at(i); }
    const T& operator[](unsigned int i) const { return values.at(i); }
    MStatus setLength(unsigned int n) {
        if (!api("array.allocate")) return MS::kFailure;
        values.resize(shortAt == static_cast<int>(operations.size())-1 && n ? n-1 : n);
        return MS::kSuccess;
    }
    MStatus copy(const Array& other) {
        if (!api("array.copy")) return MS::kFailure;
        values = other.values;
        return MS::kSuccess;
    }
};
using MIntArray = Array<int>;
using MPointArray = Array<MPoint>;
using MFloatVectorArray = Array<MFloatVector>;
using MStringArray = Array<MString>;
int elementCount = 3;
bool stuck = false, early = false, invalidIndex = false, degenerate = false;
struct Iterator {
    int ordinal = 0;
    Iterator(const MObject&, MStatus* status) { result(status, api("iterator.create")); }
    bool isDone(MStatus* status) const {
        result(status, api("iterator.done"));
        return ordinal >= elementCount - (early ? 1 : 0);
    }
    MStatus next() {
        if (!api("iterator.next")) return MS::kFailure;
        if (!stuck) ++ordinal;
        return MS::kSuccess;
    }
    int index(MStatus* status = nullptr) const {
        result(status, api("iterator.index")); return invalidIndex ? -1 : ordinal;
    }
};
struct MItMeshEdge : Iterator {
    using Iterator::Iterator;
    using Iterator::index;
    int index(int endpoint, MStatus* status) const {
        result(status, api("edge.endpoint")); return invalidIndex ? -1 : endpoint;
    }
    MPoint point(int endpoint, MSpace::Space, MStatus* status) const {
        result(status, api("edge.point")); return MPoint(endpoint, 0, 0);
    }
};
struct MItMeshPolygon : Iterator {
    using Iterator::Iterator;
    MStatus numTriangles(int& count) const { count = degenerate ? 0 : 1; return api("face.count") ? MS::kSuccess : MS::kFailure; }
    MStatus getNormal(MVector& normal) const { normal=MVector(0,0,1); return api("face.normal") ? MS::kSuccess : MS::kFailure; }
    MStatus getTriangle(int, MPointArray& points, MIntArray& ids) const {
        if (!api("face.triangle")) return MS::kFailure;
        points.values={MPoint(0,0,0),MPoint(1,0,0),MPoint(0,1,0)}; ids.values={0,1,2}; return MS::kSuccess;
    }
    MStatus getTriangles(MPointArray& points, MIntArray& ids) const { return getTriangle(0,points,ids); }
};
struct MFnMesh {
    int numPolygons(MStatus* status) const { result(status, api("mesh.faces")); return elementCount; }
    MStatus getFaceVertexNormal(int, int, MVector& normal) const {
        normal=MVector(0,0,1); return api("mesh.normal") ? MS::kSuccess : MS::kFailure;
    }
};
namespace MHWRender {
struct MGeometry;
struct MGeometryTypes {
    enum { kPosition, kTexture, kColor, kNormal, kUnsignedInt32, kFloat,
           kDouble, kUnsignedChar, kPoints, kLines, kTriangles, kAll };
};
struct MVertexBufferDescriptor {
    MString item, semanticText;
    int kind=0, components=3;
    int type=MGeometryTypes::kFloat;
    unsigned int elementBytes=sizeof(float);
    MVertexBufferDescriptor() = default;
    MVertexBufferDescriptor(const char* name, int semantic, int dimension, const char* text="")
        : item(name), semanticText(text), kind(semantic), components(dimension) {}
    MString name() const { return item; }
    MString semanticName() const { return semanticText; }
    int semantic() const { return kind; }
    int dimension() const { return components; }
    int dataType() const { return type; }
    unsigned int dataTypeSize() const { return elementBytes; }
};
struct MVertexBufferDescriptorList {
    std::vector<MVertexBufferDescriptor> values;
    bool overrideCount=false;
    int reportedCount=0;
    int length() const { return overrideCount ? reportedCount : static_cast<int>(values.size()); }
    bool getDescriptor(int i, MVertexBufferDescriptor& out) const {
        if (!api("gpu.descriptor")) return false;
        out=values.at(i); return true;
    }
};
struct MGeometryRequirements {
    MVertexBufferDescriptorList list;
    const MVertexBufferDescriptorList& vertexRequirements() const { return list; }
};
struct MVertexBuffer {
    int dimension;
    std::vector<float> values;
    explicit MVertexBuffer(int dim) : dimension(dim) {}
    void* acquire(unsigned int count, bool) {
        if (!api("gpu.vertex.acquire")) return nullptr;
        values.resize(static_cast<std::size_t>(count)*dimension); return values.data();
    }
    void commit(void*) { if (!api("gpu.vertex.commit")) throw std::runtime_error("commit"); }
};
struct MIndexBuffer {
    std::vector<unsigned int> values;
    void* acquire(unsigned int count, bool) {
        if (!api("gpu.index.acquire")) return nullptr;
        values.resize(count); return values.data();
    }
    void commit(void*) { if (!api("gpu.index.commit")) throw std::runtime_error("commit"); }
};
struct MGeometry : MGeometryTypes {
    std::vector<std::unique_ptr<MVertexBuffer>> vertices;
    std::vector<std::unique_ptr<MIndexBuffer>> indices;
    MVertexBuffer* createVertexBuffer(const MVertexBufferDescriptor& desc) {
        if (!api("gpu.vertex.create")) return nullptr;
        vertices.emplace_back(new MVertexBuffer(desc.dimension())); return vertices.back().get();
    }
    MIndexBuffer* createIndexBuffer(int) {
        if (!api("gpu.index.create")) return nullptr;
        indices.emplace_back(new MIndexBuffer); return indices.back().get();
    }
};
int pendingRenderItems=0;
struct MShaderInstance {
    int stockType=-1;
    bool released=false;
    std::vector<int> parameterTypes{0,1,2,3,4,5,6,7,8,9,10,11,12,13};
    mutable std::size_t parameterIndex=0;
    void parameterList(MStringArray& output) const {
        if (!api("debug.parameters")) throw std::runtime_error("parameters");
        parameterIndex=0;
        output.values.assign(parameterTypes.size(),MString("parameter"));
    }
    int parameterType(const MString&) const {
        if (!api("debug.type")) throw std::runtime_error("type");
        return parameterTypes.at(parameterIndex++);
    }
    bool isArrayParameter(const MString&) const {
        if (!api("debug.array")) throw std::runtime_error("array");
        return false;
    }
    MString semantic(const MString&) const {
        if (!api("debug.semantic")) throw std::runtime_error("semantic");
        return MString("semantic");
    }
    template <typename Name,typename Value> bool setParameter(const Name&,const Value&) {
        return api("render.parameter");
    }
};
bool rendererAvailable=true, shaderManagerAvailable=true, stockShaderAvailable=true;
int failedStockType=-1;
std::vector<int> stockShaderRequests;
std::vector<std::unique_ptr<MShaderInstance>> stockShaderInstances;
std::vector<MShaderInstance*> shaderReleaseRequests;
bool releaseThrowsAfterCompletion=false;
struct MShaderManager {
    enum MStockShader {
        k3dDefaultMaterialShader, k3dFatPointShader, k3dSolidShader,
        k3dThickLineShader, k3dCPVSolidShader, k3dIntegerNumericShader,
        k3dPointVectorShader, k3dFloatNumericShader, k3dFloat2NumericShader,
        k3dFloat3NumericShader, k3dCPVFatPointShader, k3dCPVThickLineShader
    };
    MShaderInstance* getStockShader(MStockShader type) const {
        stockShaderRequests.push_back(int(type));
        if (!api("shader.acquire") || !stockShaderAvailable || int(type)==failedStockType)
            return nullptr;
        std::unique_ptr<MShaderInstance> instance(new MShaderInstance());
        instance->stockType=int(type);
        stockShaderInstances.push_back(std::move(instance));
        return stockShaderInstances.back().get();
    }
    void releaseShader(MShaderInstance* shader) const {
        shaderReleaseRequests.push_back(shader);
        const bool completes=api("shader.release");
        assert(shader && !shader->released);
        shader->released=true;
        if (!completes || releaseThrowsAfterCompletion)
            throw std::runtime_error("release after completion");
    }
};
struct MPxGeometryOverride {
    explicit MPxGeometryOverride(const MObject&) {}
    virtual ~MPxGeometryOverride() = default;
};
struct MRenderer {
    static MRenderer* theRenderer() {
        static MRenderer renderer;
        return api("shader.renderer") && rendererAvailable ? &renderer : nullptr;
    }
    const MShaderManager* getShaderManager() const {
        static MShaderManager manager;
        return api("shader.manager") && shaderManagerAvailable ? &manager : nullptr;
    }
};
struct MRenderItem {
    enum { DecorationItem, MaterialSceneItem, sActiveLineDepthPriority,
           sDormantWireDepthPriority,sSelectionDepthPriority,sDormantPointDepthPriority };
    MString item;
    bool enabled=false;
    const MShaderInstance* shader=nullptr;
    explicit MRenderItem(const char* name) : item(name) {}
    explicit MRenderItem(const MString& name) : item(name) {}
    static MRenderItem* Create(const MString& name,int,int) {
        if (!api("render.create")) return nullptr;
        MRenderItem* result=new MRenderItem(name); ++pendingRenderItems; return result;
    }
    static void Destroy(MRenderItem*& item) {
        if (item) {delete item; item=nullptr; --pendingRenderItems;}
    }
    void setDrawMode(int) { if (!api("render.mode")) throw std::runtime_error("mode"); }
    void depthPriority(int) { if (!api("render.depth")) throw std::runtime_error("depth"); }
    bool setShader(const MShaderInstance* value,const MString*) {
        if (!api("render.shader")) return false;
        shader=value; return true;
    }
    void enable(bool visible) {
        activeRenderItem=this;
        if (!api("render.enable")) throw std::runtime_error("enable");
        enabled=visible;
        assert(!visible || shader);
    }
    MString name() const { return item; }
    bool associateWithIndexBuffer(const MIndexBuffer*) const { return api("gpu.associate"); }
};
struct MGeometryUtilities {
    static MColor wireframeColor(const MDagPath&) {
        if (!api("render.wireframeColor")) throw std::runtime_error("wireframeColor");
        return MColor(1,1,1,1);
    }
};
struct MRenderItemList {
    struct DestroyItem {
        void operator()(MRenderItem* item) const noexcept { MRenderItem::Destroy(item); }
    };
    std::vector<MRenderItem> items;
    std::vector<std::unique_ptr<MRenderItem,DestroyItem>> owned;
    int length() const { return static_cast<int>(items.size()+owned.size()); }
    int indexOf(const MString& name) const {
        if (!api("render.find")) throw std::runtime_error("find");
        for (std::size_t i=0;i<items.size();++i) if (items[i].item==name) return static_cast<int>(i);
        for (std::size_t i=0;i<owned.size();++i) if (owned[i]->item==name) return static_cast<int>(items.size()+i);
        return -1;
    }
    bool append(MRenderItem* item) {
        if (!api("render.append")) return false;
        owned.reserve(owned.size()+1U);
        owned.emplace_back(item);
        return true;
    }
    const MRenderItem* itemAt(int i) const {
        return std::size_t(i)<items.size() ? &items.at(i) : owned.at(i-items.size()).get();
    }
    MRenderItem* itemAt(int i) {
        return std::size_t(i)<items.size() ? &items.at(i) : owned.at(i-items.size()).get();
    }
};
} // namespace MHWRender
