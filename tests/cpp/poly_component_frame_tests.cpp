// Production node/frame/publication bodies with failure-injectable Maya seams.
// Rotation doubles transport finite values; they do not emulate Maya rotations.
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMeshInputValidation.h"
#include "common/utility/DPointMean.h"
#include "common/utility/DAttributeBlendMath.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int callCount = 0, failAt = -1, throwAt = -1, writes = 0, cleans = 0;
int componentId = 0, vertices = 3, faces = 1, edges = 3;
int polygonSize = 3, connectedSize = 1, returnedFaces = 1;
int endpoint0 = 0, endpoint1 = 1;
bool shortPoints = false, meshNull = false, wrongType = false;
std::vector<std::string> calls;
MStatus api(const std::string& name) {
    calls.push_back(name);
    const int index = callCount++;
    if (index == throwAt) throw std::runtime_error("injected API exception");
    return index == failAt ? MS::kFailure : MS::kSuccess;
}
void resetCalls() {
    callCount = writes = cleans = 0; failAt = throwAt = -1; calls.clear();
}
}
namespace MSpace { enum Space { kObject, kWorld }; }
struct MQuaternion { double x = 0, y = 0, z = 0, w = 1; };
struct MVector {
    double x, y, z;
    MVector(double x = 0, double y = 0, double z = 0):x(x),y(y),z(z) {}
    MVector operator-(const MVector& p) const { return {x-p.x,y-p.y,z-p.z}; }
    MVector operator*(double s) const { return {x*s,y*s,z*s}; }
    MVector operator/(double s) const { return {x/s,y/s,z/s}; }
    double operator*(const MVector& p) const { return x*p.x+y*p.y+z*p.z; }
    MVector& operator+=(const MVector& p) { x+=p.x;y+=p.y;z+=p.z;return *this; }
    double length() const { return std::sqrt(x*x+y*y+z*z); }
    MQuaternion rotateTo(const MVector&) const { return {}; }
    MVector rotateBy(const MQuaternion&) const { return *this; }
};
struct MPoint {
    double x, y, z, w;
    MPoint(double x = 0,double y = 0,double z = 0,double w = 1):x(x),y(y),z(z),w(w) {}
    MVector operator-(const MPoint& p) const {
        assert(w == 1 && p.w == 1); return {x-p.x,y-p.y,z-p.z};
    }
    MPoint operator+(const MVector& p) const { assert(w == 1);return {x+p.x,y+p.y,z+p.z}; }
    MPoint& operator+=(const MVector& p) { assert(w == 1);x+=p.x;y+=p.y;z+=p.z;return *this; }
};
struct MMatrix {
    double values[4][4] = {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    double* operator[](unsigned i) { return values[i]; }
    const double* operator[](unsigned i) const { return values[i]; }
};
struct MTransformationMatrix {
    enum RotationOrder { kXYZ };
    MMatrix matrix;
    void rotateBy(const MQuaternion&,MSpace::Space,MStatus* status) { *status=api("rotate"); }
    MStatus addTranslation(const MVector& v,MSpace::Space) {
        const auto status=api("translation");
        if(status) { matrix[3][0]=v.x;matrix[3][1]=v.y;matrix[3][2]=v.z; } return status;
    }
    MStatus getRotation(double* v,RotationOrder&) const {
        v[0]=v[1]=v[2]=0;return api("rotation-query");
    }
    MMatrix asMatrix() const { return matrix; }
};
template<class T> struct Array : std::vector<T> {
    using std::vector<T>::vector;
    unsigned length() const { return static_cast<unsigned>(this->size()); }
    MStatus setLength(unsigned count) {
        const auto status=api("point-storage");
        if(status) this->resize(shortPoints && count ? count-1 : count);return status;
    }
};
using MPointArray = Array<MPoint>;
using MIntArray = Array<int>;
std::map<int,MPoint> fixturePoints;
MIntArray fixtureIds, fixtureFaces;
std::map<int,MVector> fixtureNormals;
struct Payload { MMatrix matrix; MVector vector; };
struct MObject {
    std::string name;
    std::shared_ptr<Payload> payload;
    MObject(std::string name = ""):name(std::move(name)) {}
    bool isNull() const { return name.empty(); }
};
struct MPlug {
    std::string name;
    MPlug(const MObject& object):name(object.name) {}
    bool operator!=(const MObject& object) const { return name != object.name; }
};
namespace MFnData { enum Type { kMesh, kNumeric }; }
struct MDataBlock;
struct MDataHandle {
    MDataBlock* block; std::string attribute;
    int asInt() const { return componentId; }
    MFnData::Type type() const { return wrongType ? MFnData::kNumeric : MFnData::kMesh; }
    MObject asMesh() const { return MObject(meshNull ? "" : "mesh"); }
    MStatus set(const MObject&);
    void setClean() { ++cleans; }
};
struct MDataBlock {
    std::map<std::string,MObject> outputs;
    MDataHandle inputValue(const MObject& object,MStatus* status) {
        *status=api("input "+object.name);return {this,object.name};
    }
    MDataHandle outputValue(const MObject& object,MStatus* status) {
        *status=api("output "+object.name);return {this,object.name};
    }
    MStatus setClean(const MPlug&) { return api("data-clean"); }
};
MStatus MDataHandle::set(const MObject& object) {
    const auto status=api("set "+attribute);
    if(status) { block->outputs[attribute]=object; ++writes; } return status;
}
struct MFnMatrixData {
    MObject create(const MMatrix& matrix,MStatus* status) {
        *status=api("matrix-data");MObject obj("matrix");
        obj.payload=std::make_shared<Payload>();obj.payload->matrix=matrix;return obj;
    }
};
struct MFnNumericData {
    enum Type { k3Double };
    MObject object;
    MObject create(Type,MStatus* status) {
        *status=api("numeric-data");object=MObject("numeric");
        object.payload=std::make_shared<Payload>();return object;
    }
    MStatus setData(double x,double y,double z) {
        const auto status=api("numeric-value");if(status) object.payload->vector={x,y,z};return status;
    }
};
struct MFnMesh {
    MFnMesh(const MObject&,MStatus* status) { *status=api("mesh-bind"); }
    int numVertices(MStatus* status) const { *status=api("vertices");return vertices; }
    int numPolygons(MStatus* status) const { *status=api("faces");return faces; }
    int numEdges(MStatus* status) const { *status=api("edges");return edges; }
    int polygonVertexCount(int,MStatus* status) const { *status=api("polygon-size");return polygonSize; }
    MStatus getPolygonVertices(int,MIntArray& ids) const {
        const auto status=api("polygon-ids");if(status) ids=fixtureIds;return status;
    }
    MStatus getPoint(int id,MPoint& point,MSpace::Space space) const {
        assert(space==MSpace::kObject);const auto status=api("point");
        if(status) point=fixturePoints.at(id);return status;
    }
    MStatus getPoints(MPointArray& points,MSpace::Space) const {
        const auto status=api("all-points");
        if(status) for(const auto& p:fixturePoints) points.push_back(p.second);return status;
    }
    MStatus getPolygonNormal(int id,MVector& normal,MSpace::Space space) const {
        assert(space==MSpace::kObject);const auto status=api("normal");
        if(status) normal=fixtureNormals.at(id);return status;
    }
};
struct MItMeshEdge {
    MItMeshEdge(MObject,MStatus* status) { *status=api("edge-bind"); }
    MStatus setIndex(int,int&) { return api("edge-index"); }
    int index(int endpoint,MStatus* status) { *status=api("endpoint");return endpoint ? endpoint1 : endpoint0; }
    MStatus numConnectedFaces(int& count) const { count=connectedSize;return api("connected-size"); }
    int getConnectedFaces(MIntArray& ids,MStatus* status) const {
        *status=api("connected-ids");if(*status) ids=fixtureFaces;return returnedFaces;
    }
};
#include "poly_component_frame.inc"

void setup() {
    resetCalls();componentId=0;vertices=3;faces=1;edges=3;
    polygonSize=3;connectedSize=returnedFaces=1;endpoint0=0;endpoint1=1;
    shortPoints=meshNull=wrongType=false;
    fixturePoints={{0,{0,0,0}},{1,{2,0,0}},{2,{0,2,0}}};
    fixtureIds={0,1,2};fixtureFaces={0};fixtureNormals={{0,{0,0,1}}};
}
MStatus evaluate(bool face,MDataBlock& data,const char* output="outMatrix") {
    const MPlug plug{MObject(output)};
    return face ? DPolyFaceToMat().compute(plug,data) : DPolyEdgeToMat().compute(plug,data);
}
MDataBlock seeded() {
    MDataBlock data;
    for(const char* name: {"outMatrix","outTranslate","outRotate"}) data.outputs[name]=MObject("previous");
    return data;
}
void rejected(bool face) {
    auto data=seeded();assert(!evaluate(face,data));assert(writes==0 && cleans==0);
    for(const auto& p:data.outputs) assert(p.second.name=="previous");
}
int main() {
    int injectedCases=0;
    const std::size_t storageCount=(256U*1024U*1024U)/(sizeof(MPoint)+sizeof(int));
    assert(dommy::mesh::validComponentFrameStorage(storageCount,sizeof(MPoint)+sizeof(int),4));
    assert(!dommy::mesh::validComponentFrameStorage(storageCount+1,sizeof(MPoint)+sizeof(int),4));
    assert(!dommy::mesh::validComponentFrameStorage(1,1,100000001U));
    assert(!dommy::mesh::validComponentFrameStorage(0,1,1));
    assert(!dommy::mesh::validComponentFrameStorage(2,std::numeric_limits<std::size_t>::max(),1));
    for(bool face: {false,true}) {
        setup();auto data=seeded();assert(evaluate(face,data)==MS::kSuccess);
        assert(writes==3 && cleans==3);
        const auto v=data.outputs.at("outTranslate").payload->vector;
        assert(v.x==(face ? 2.0/3.0 : 1.0) && v.y==(face ? 2.0/3.0 : 0.0));
        const auto m=data.outputs.at("outMatrix").payload->matrix;
        assert(m[3][0]==v.x && m[3][1]==v.y);
        assert(std::count(calls.begin(),calls.end(),"point")== (face ? 3 : 2));
        assert(std::count(calls.begin(),calls.end(),"all-points")==0);
        assert(std::count(calls.begin(),calls.end(),"faces")==1);
        const int total=callCount;
        const auto firstSet=std::find(calls.begin(),calls.end(),"set outMatrix");
        assert(firstSet!=calls.end());
        for(const char* name: {"output outMatrix","output outTranslate","output outRotate"})
            assert(std::find(calls.begin(),calls.end(),name)<firstSet);
        const int firstSetIndex=static_cast<int>(firstSet-calls.begin());
        injectedCases+=total*2;
        for(int at=0;at<total;++at) for(bool exception: {false,true}) {
            setup();data=seeded();if(exception) throwAt=at;else failAt=at;
            const auto source=fixturePoints;
            assert(!evaluate(face,data));assert(callCount==at+1);
            if(at<firstSetIndex) assert(writes==0 && cleans==0);
            if(writes==0) for(const auto& p:data.outputs) assert(p.second.name=="previous");
            for(const auto& p:source) {
                const auto& actual=fixturePoints.at(p.first);
                assert(actual.x==p.second.x && actual.y==p.second.y &&
                       actual.z==p.second.z && actual.w==p.second.w);
            }
            resetCalls();assert(evaluate(face,data)==MS::kSuccess && writes==3 && cleans==3);
        }
        for(const char* output: {"outTranslate","outRotate"}) {
            setup();data=seeded();assert(evaluate(face,data,output)==MS::kSuccess);
        }
        setup();data=seeded();assert(evaluate(face,data,"unrelated")==MS::kUnknownParameter && callCount==0);
        setup();vertices=std::numeric_limits<int>::max();data=seeded();
        assert(evaluate(face,data)==MS::kSuccess && std::count(calls.begin(),calls.end(),"all-points")==0);
        for(int invalid: {-1,0}) { setup();vertices=invalid;rejected(face); }
        for(int invalid: {-1,3}) { setup();componentId=invalid;if(face) faces=3;rejected(face); }
        setup();meshNull=true;rejected(face);setup();wrongType=true;rejected(face);
        for(int id: {-1,3}) {
            setup();if(face) fixtureIds[2]=id;else endpoint1=id;rejected(face);
            assert(std::count(calls.begin(),calls.end(),"point")==0);
        }
        for(double invalid: {0.0,std::numeric_limits<double>::infinity()}) {
            setup();fixturePoints[1].w=invalid;rejected(face);
        }
        for(int id: {0,1,2}) {
            if(!face && id==2) continue;
            setup();fixturePoints[id].x=std::numeric_limits<double>::quiet_NaN();rejected(face);
        }
        setup();fixturePoints[1]={4,0,0,2};data=seeded();assert(evaluate(face,data)==MS::kSuccess);
        assert(data.outputs.at("outTranslate").payload->vector.x==(face ? 2.0/3.0 : 1.0));
        setup();fixtureNormals[0]={0,0,0};rejected(face);
        setup();fixtureNormals[0].x=std::numeric_limits<double>::infinity();rejected(face);
    }
    for(int invalid: {-1,0,2,10000001,static_cast<int>(storageCount+1)}) {
        setup();polygonSize=invalid;rejected(true);
        assert(std::count(calls.begin(),calls.end(),"polygon-ids")==0);
    }
    for(bool longer: {false,true}) {
        setup();if(longer) fixtureIds.push_back(0);else fixtureIds.pop_back();rejected(true);
        assert(std::count(calls.begin(),calls.end(),"point-storage")==0);
    }
    setup();shortPoints=true;rejected(true);assert(std::count(calls.begin(),calls.end(),"point")==0);
    for(int invalid: {-1,0,2000001}) {
        setup();connectedSize=invalid;rejected(false);
        assert(std::count(calls.begin(),calls.end(),"connected-ids")==0);
    }
    setup();faces=-1;rejected(false);setup();connectedSize=2;rejected(false);
    for(int count: {0,2}) { setup();returnedFaces=count;rejected(false); }
    setup();fixtureFaces={};rejected(false);setup();fixtureFaces={0,0};rejected(false);
    setup();fixturePoints[1]=fixturePoints[0];rejected(false);
    for(int id: {-1,1}) { setup();fixtureFaces={id};rejected(false); }
    // Non-contiguous global IDs must be mapped to face-local point ordinals.
    setup();vertices=1000;fixtureIds={7,42,999};
    fixturePoints={{7,{0,0,0}},{42,{2,0,0}},{999,{0,2,0}}};
    auto data=seeded();assert(evaluate(true,data)==MS::kSuccess);
    // Accurate arithmetic mean, independent of polygon vertex order.
    setup();const double large=std::ldexp(1.0,70);
    fixturePoints={{0,{large,0,0}},{1,{1,1,0}},{2,{-large,0,0}}};
    unsigned permutations=0;
    do {
        resetCalls();data=seeded();assert(evaluate(true,data)==MS::kSuccess);
        const auto v=data.outputs.at("outTranslate").payload->vector;
        assert(v.x==1.0/3.0 && v.y==1.0/3.0);++permutations;
    } while(std::next_permutation(fixtureIds.begin(),fixtureIds.end()));
    assert(permutations==6);
    for(int size: {4,5}) {
        setup();vertices=polygonSize=size;fixtureIds={0,1,2,3};fixturePoints[3]={2,2,0};
        if(size==5) { fixtureIds.push_back(4);fixturePoints[4]={1,1,0}; }
        data=seeded();assert(evaluate(true,data)==MS::kSuccess);
        const auto v=data.outputs.at("outTranslate").payload->vector;assert(v.x==1 && v.y==1);
    }
    std::cout << "Poly frames: selected reads, budgets/cardinality, exact centers, production publication, "
              << injectedCases << " API failures/exceptions and retries passed; no Maya runtime\n";
}
