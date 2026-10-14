#pragma once

#include "maya_attribute_doubles.h"
#include "point_input_fixture.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaAttributeArray.h"
#include "common/utility/DNumericValidation.h"
#include "math/DBounds.h"
#include "math/DRandomGenerator.h"
#include "modules/particles/particle/field/DKillFieldMath.h"

#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <set>

// Host boundaries only: array sharing, failures and publication are modeled,
// not Maya DG ownership, particle simulation or arbitrary mesh geometry.
struct ParticleString : MString {
    ParticleString(const char* value = "") : MString(value) {}
    ParticleString(const MString& value) : MString(value) {}
    MStatus set(const char* value) {
        const MStatus status = operation("string.set");
        if (status) MString::operator=(MString(value));
        return status;
    }
    ParticleString operator+(const ParticleString& value) const {
        return ParticleString((std::string(asChar()) + value.asChar()).c_str());
    }
};
#define MString ParticleString

struct MVector {
    double x, y, z;
    MVector(double a = 0, double b = 0, double c = 0) : x(a), y(b), z(c) {}
    MVector operator+(const MVector& v) const { return MVector(x+v.x, y+v.y, z+v.z); }
    MVector operator/(double v) const { return MVector(x/v, y/v, z/v); }
    MVector operator*(double v) const { return MVector(x*v, y*v, z*v); }
    MVector operator-(const MVector& v) const { return MVector(x-v.x, y-v.y, z-v.z); }
    MVector& operator+=(const MVector& v) { x+=v.x; y+=v.y; z+=v.z; return *this; }
    double operator[](unsigned i) const { assert(i<3); return i==0 ? x : (i==1 ? y : z); }
    double operator*(const MVector& v) const { return x*v.x + y*v.y + z*v.z; }
    MVector& operator/=(double v) { x/=v; y/=v; z/=v; return *this; }
    double length() const { return std::sqrt(x*x + y*y + z*z); }
    bool operator==(const MVector& v) const { return x==v.x && y==v.y && z==v.z; }
    static const MVector zero;
};
const MVector MVector::zero;
struct ParticleMatrix : MMatrix {
    // Goldens use only diagonal scale and translation; native inversion is not tested.
    bool isSingular() const { return (*this)[0][0] == 0 || (*this)[1][1] == 0 || (*this)[2][2] == 0; }
    ParticleMatrix inverse() const {
        ParticleMatrix result;
        for (unsigned i=0; i<3; ++i) {
            result[i][i] = 1 / (*this)[i][i];
            result[3][i] = -(*this)[3][i] / (*this)[i][i];
        }
        return result;
    }
};
#define MMatrix ParticleMatrix
struct MPoint {
    double x, y, z, w;
    MPoint(double a=0, double b=0, double c=0) : x(a), y(b), z(c), w(1) {}
    MPoint(const MVector& v) : MPoint(v.x,v.y,v.z) {}
    MPoint operator+(const MVector& v) const { return MPoint(x+v.x,y+v.y,z+v.z); }
    MVector operator-(const MPoint& p) const { return MVector(x-p.x,y-p.y,z-p.z); }
    double distanceTo(const MPoint& p) const { return (*this-p).length(); }
    void cartesianize() { x/=w; y/=w; z/=w; w=1; }
    MPoint operator*(const MMatrix& m) const {
        const double p[4] = {x,y,z,w};
        double q[4] = {};
        for (unsigned i=0; i<4; ++i) for (unsigned j=0; j<4; ++j) q[j]+=p[i]*m[i][j];
        MPoint result(q[0],q[1],q[2]); result.w=q[3]; return result;
    }
    MPoint& operator*=(const MMatrix& m) { *this=*this*m; return *this; }
};
struct MBoundingBox {
    MPoint lower{-1,-1,-1}, upper{1,1,1};
    bool empty = false;
    MPoint min() const { return lower; }
    MPoint max() const { return upper; }
    void clear() { empty = true; }
    void expand(const MPoint& p) {
        if (empty) { lower=upper=p; empty=false; return; }
        lower.x=std::min(lower.x,p.x); lower.y=std::min(lower.y,p.y); lower.z=std::min(lower.z,p.z);
        upper.x=std::max(upper.x,p.x); upper.y=std::max(upper.y,p.y); upper.z=std::max(upper.z,p.z);
    }
    bool contains(const MPoint& p) const {
        return p.x>=lower.x && p.y>=lower.y && p.z>=lower.z && p.x<=upper.x && p.y<=upper.y && p.z<=upper.z;
    }
};
static int shortAt=-1, shortApplied=0, badAccesses=0, oversizedAllocations=0;
template<class T> struct ParticleArray {
    std::shared_ptr<std::vector<T>> values = std::make_shared<std::vector<T>>();
    unsigned int reportedLength=0;
    ParticleArray() = default;
    ParticleArray(std::initializer_list<T> v) : values(std::make_shared<std::vector<T>>(v)) {}
    ParticleArray(unsigned count, const T& initial) {
        const MStatus status=operation("array.constructor");
        checkCount(count); if (status) values->assign(count,initial);
    }
    static void checkCount(unsigned count) {
        if (count>128) { ++oversizedAllocations; throw std::length_error("test allocation limit"); }
    }
    unsigned length() const { return reportedLength ? reportedLength : values->size(); }
    MStatus clear() {
        const MStatus status=operation("array.clear");
        if (status) { values->clear(); reportedLength=0; } return status;
    }
    T& operator[](unsigned i) { if (i>=values->size()) ++badAccesses; return values->at(i); }
    const T& operator[](unsigned i) const { if (i>=values->size()) ++badAccesses; return values->at(i); }
    MStatus setLength(unsigned n) {
        const int ordinal=callCount;
        const MStatus status=operation("array.resize"); checkCount(n);
        if (status) { values->resize(n); shorten(ordinal); }
        return status;
    }
    MStatus copy(const ParticleArray& source) {
        const int ordinal=callCount;
        const MStatus status=operation("array.copy");
        *values=*source.values;
        if (!status && !values->empty()) values->pop_back();
        shorten(ordinal); return status;
    }
    void shorten(int ordinal) {
        if (ordinal==shortAt && !values->empty()) { values->pop_back(); ++shortApplied; }
    }
};
using MVectorArray=ParticleArray<MVector>;
using MDoubleArray=ParticleArray<double>;
using MIntArray=ParticleArray<int>;
using MPointArray=ParticleArray<MPoint>;
struct MStringArray : ParticleArray<MString> {
    using ParticleArray<MString>::ParticleArray;
    // Match Maya's lack of a status-returning copy() overload.
    MStatus copy(const MStringArray&) = delete;
    MStatus set(const MString& value, unsigned i) {
        const MStatus status=operation("stringArray.set");
        if (status) (*this)[i]=value;
        return status;
    }
};
using double2=double[2];
using double3=double[3];
using int3=int[3];
using DPoint=Imath::V3f;
struct DPointList : std::vector<DPoint> {
    using std::vector<DPoint>::vector;
    std::size_t reportedSize=0;
    std::size_t size() const { return reportedSize ? reportedSize : std::vector<DPoint>::size(); }
};
struct DPData {
    static const int id=22;
    bool topology=true;
    bool hasValidPointTopology()const{return topology;}
    DPointList points={DPoint(0,0,0),DPoint(2,0,0)};
    std::map<std::string,DPAttributeData::Ptr> attributes;
    const DPointList* getPositionPtrConst() const { return &points; }
    DPAttributeData::ConstPtr getAttr(const std::string& name) const {
        const auto it=attributes.find(name); return it==attributes.end() ? nullptr : it->second;
    }
    bool isExistAttr(const std::string& name) const { return bool(getAttr(name)); }
};
struct ParticleTime {
    enum Unit { kSeconds };
    double t=0;
    ParticleTime() = default;
    explicit ParticleTime(double v) : t(v) {}
    double as(Unit) const { return t; }
    double value() const { return t; }
    bool operator<=(const ParticleTime& b) const { return t<=b.t; }
};
#define MTime ParticleTime
bool operator==(const MObject& a,const MObject& b) { return a.name==b.name; }
bool operator!=(const MObject& a,const MObject& b) { return !(a==b); }
struct OutputData {
    std::map<std::string,MVectorArray> vectors;
    std::map<std::string,MDoubleArray> doubles;
    std::map<std::string,MIntArray> ints;
    std::map<std::string,MStringArray> strings;
};
struct ParticleFixture {
    DPData input;
    bool hasInput=true, meshValid=true, foundParticle=true, multipleParticles=false;
    bool foundBounds=true, rejectSurface=false, badSurface=false;
    bool sweptConnected=true, sweptValid=true;
    bool overrideMeshVertexCount=false;
    int meshVertexCount=0, meshReadDelta=0;
    MFnData::Type geometryType=MFnData::kMesh;
    std::vector<std::array<int,2>> meshEdges={{{0,1}}};
    std::vector<int> polygonVertices={0,1,2,3};
    int particleCount=2, polygons=1, publications=0, cleans=0, lifespanWrites=0;
    int meshReads=0, outputIndex=-1, inputIndex=-1;
    std::map<std::string,double> numbers;
    std::map<std::string,std::string> strings;
    std::map<std::string,std::array<double,3>> triples;
    std::array<double,2> range={{1,5}};
    int3 density={1,1,1};
    MMatrix matrix;
    MBoundingBox bounds;
    MPointArray meshPoints={MPoint(-1,-1,-1),MPoint(1,1,1)};
    MVectorArray positions={MVector(0,0,0),MVector(2,0,0)}, stagedForces, publishedForces;
    MVectorArray vectorAttribute={MVector(0,0,0),MVector(2,0,0)};
    MDoubleArray scalarAttribute={0,2}, lifespans={100,100};
    MIntArray ids={0,2};
    std::vector<std::pair<std::string,std::string>> transfers;
    OutputData staged, published;
};
static ParticleFixture fixture;
const MObject mOutput{"mOutput"}, mIsFull{"mIsFull"}, mCurrentTime{"mCurrentTime"},
    mStartTime{"mStartTime"}, mDeltaTime{"mDeltaTime"}, mRate{"mRate"}, mDirection{"mDirection"},
    mOutputForce{"mOutputForce"}, mInputData{"mInputData"}, mInputPositions{"mInputPositions"};
struct MPlug;
using MPlugArray=ParticleArray<MPlug>;
struct MPlug {
    MObject attribute;
    int index=42;
    explicit MPlug(const MObject& a,int i=42) : attribute(a),index(i) {}
    MPlug(const MObject&,const MObject& a) : attribute(a) {}
    bool operator!=(const MObject& a) const { return attribute!=a; }
    int logicalIndex(MStatus* s) const { *s=operation("plug.index"); return index; }
    bool isConnected(MStatus* s=nullptr) const {
        if(attribute.name=="inDP")return pointInputConnection(s);
        const MStatus status=operation("plug.isConnected");
        if (s) *s=status;
        return status && fixture.sweptConnected;
    }
    MObject node() const { return attribute; }
    void connectedTo(MPlugArray& out,bool source,bool,MStatus* s) const {
        *s=operation("plug.connections"); if (!*s) return;
        if (source && fixture.foundBounds) out.values->push_back(MPlug(MObject{"dag"}));
        if (!source && fixture.foundParticle) out.values->push_back(MPlug(MObject{"particle"}));
        if (!source && fixture.multipleParticles) out.values->push_back(MPlug(MObject{"particle2"}));
    }
};
struct MDataHandle {
    MObject attribute;
    int physical=-1;
    bool valid=true;
    MDataHandle(const MObject& a=MObject(),int p=-1,bool v=true) : attribute(a),physical(p),valid(v) {}
    MDataHandle child(const MObject& a) const { assert(valid); return MDataHandle(a,physical); }
    double asDouble() const { assert(valid); return fixture.numbers.at(attribute.name); }
    float asFloat() const { return static_cast<float>(asDouble()); }
    short asShort() const { return static_cast<short>(asDouble()); }
    int asInt() const { return static_cast<int>(asDouble()); }
    bool asBool() const { return asDouble()!=0; }
    MTime asTime() const { return MTime(asDouble()); }
    const double2& asDouble2() const { return *reinterpret_cast<const double2*>(fixture.range.data()); }
    const double3& asDouble3() const {
        return *reinterpret_cast<const double3*>(fixture.triples.at(attribute.name).data());
    }
    const int3& asInt3() const { return fixture.density; }
    MMatrix asMatrix() const { assert(valid); return fixture.matrix; }
    MString asString() const {
        assert(valid);
        if (physical>=0) return (attribute.name=="aSourceAttribute" ?
            fixture.transfers.at(physical).first : fixture.transfers.at(physical).second).c_str();
        return fixture.strings.at(attribute.name).c_str();
    }
    MObject data() const {
        assert(valid);
        if(attribute.name=="inDP"){
            ++pointInputFixture.dataReads;
            return fixture.hasInput?attribute:MObject::kNullObj;
        }
        const bool absent=((attribute.name=="inMeshObject" || attribute.name=="inputGeometry") && !fixture.meshValid) ||
            (attribute.name=="mSweptGeometry" && !fixture.sweptValid);
        return absent ? MObject::kNullObj : attribute;
    }
    MFnData::Type type() const { return fixture.meshValid ? fixture.geometryType : MFnData::kInvalid; }
    MStatus set(const MObject& object) {
        assert(valid); const MStatus status=operation("output.set");
        if (status) {
            ++fixture.publications;
            if (object.name=="forces") fixture.publishedForces=fixture.stagedForces;
            else { assert(object.name=="emitter"); fixture.published=fixture.staged; }
        }
        return status;
    }
};
struct MArrayDataBuilder {
    MDataHandle addElement(unsigned index,MStatus* s) {
        *s=operation("builder.element"); fixture.outputIndex=index;
        return MDataHandle(MObject{"output"},-1,bool(*s));
    }
};
struct MArrayDataHandle {
    MObject attribute;
    int physical=-1;
    bool valid;
    MArrayDataHandle(const MObject& a,bool v) : attribute(a),valid(v) {}
    MStatus jumpToElement(unsigned i) { assert(valid); fixture.inputIndex=i; return operation("array.logical"); }
    MStatus jumpToArrayElement(unsigned i) { assert(valid); physical=i; return operation("array.physical"); }
    unsigned elementCount(MStatus* s) { assert(valid); *s=operation("array.count"); return fixture.transfers.size(); }
    MDataHandle inputValue(MStatus* s) {
        assert(valid); *s=operation("array.input"); return MDataHandle(attribute,physical,bool(*s));
    }
    MArrayDataBuilder builder(MStatus* s) { assert(valid); *s=operation("array.builder"); return {}; }
};
struct MDataBlock {
    MDataHandle inputValue(const MObject& a,MStatus* s) {
        *s=operation("input "+a.name); return MDataHandle(a,-1,a.name=="inDP"||bool(*s));
    }
    MArrayDataHandle outputArrayValue(const MObject& a,MStatus* s) {
        *s=operation("outputArray "+a.name); return MArrayDataHandle(a,bool(*s));
    }
    MArrayDataHandle inputArrayValue(const MObject& a,MStatus* s) {
        *s=operation("inputArray "+a.name); return MArrayDataHandle(a,bool(*s));
    }
    MStatus setClean(const MPlug&) {
        const MStatus status=operation("output.clean"); if (status) ++fixture.cleans; return status;
    }
};
#define DOMMY_POINT_INPUT_DATA fixture.input
#define DOMMY_POINT_INPUT_PRESENT fixture.hasInput
#include "point_input_binding_fixture.h"
struct MFnArrayAttrsData {
    MObject create(MStatus* s) { *s=operation("emitter.create"); fixture.staged=OutputData(); return {"emitter"}; }
    MVectorArray vectorArray(const MString& n,MStatus* s) { *s=operation("attr.vector"); return fixture.staged.vectors[n.asChar()]; }
    MDoubleArray doubleArray(const MString& n,MStatus* s) { *s=operation("attr.double"); return fixture.staged.doubles[n.asChar()]; }
    MIntArray intArray(const MString& n,MStatus* s) { *s=operation("attr.int"); return fixture.staged.ints[n.asChar()]; }
    MStringArray stringArray(const MString& n,MStatus* s) { *s=operation("attr.string"); return fixture.staged.strings[n.asChar()]; }
};
struct MFnVectorArrayData {
    bool valid=true;
    MFnVectorArrayData() = default;
    MFnVectorArrayData(const MObject&,MStatus* s) { *s=operation("positions.bind"); valid=bool(*s); }
    MVectorArray array(MStatus* s) { assert(valid); *s=operation("positions.read"); return fixture.positions; }
    MObject create(const MVectorArray& a,MStatus* s) { *s=operation("forces.create"); if (*s) fixture.stagedForces=a; return {"forces"}; }
};
namespace MSpace { enum Space { kObject }; }
struct MFnMesh {
    bool valid;
    MFnMesh(const MObject&,MStatus* s) { *s=operation("mesh.bind"); valid=bool(*s); }
    int numVertices(MStatus* s) {
        assert(valid); *s=operation("mesh.vertices");
        return fixture.overrideMeshVertexCount ? fixture.meshVertexCount : fixture.meshPoints.length();
    }
    int numPolygons(MStatus* s) { assert(valid); *s=operation("mesh.polygons"); return fixture.polygons; }
    int numEdges(MStatus* s) { assert(valid); *s=operation("mesh.edges"); return fixture.meshEdges.size(); }
    MStatus getEdgeVertices(int i,int (&v)[2]) {
        assert(valid); const MStatus s=operation("mesh.edgeVertices");
        if (s) { v[0]=fixture.meshEdges.at(i)[0]; v[1]=fixture.meshEdges.at(i)[1]; } return s;
    }
    MStatus getPolygonVertices(int,MIntArray& v) {
        assert(valid); const MStatus s=operation("mesh.faceVertices");
        if (s) *v.values=fixture.polygonVertices; return s;
    }
    MStatus getPoints(MPointArray& p,MSpace::Space) {
        assert(valid); ++fixture.meshReads; const MStatus s=operation("mesh.points");
        if (s) {
            p=fixture.meshPoints;
            if (fixture.meshReadDelta) {
                p.values=std::make_shared<std::vector<MPoint>>(*p.values);
                p.values->resize(p.values->size()+fixture.meshReadDelta);
            }
        }
        return s;
    }
    MStatus getClosestPointAndNormal(const MPoint& p,MPoint& closest,MVector& normal,MSpace::Space) {
        assert(valid); const MStatus s=operation("mesh.closest");
        closest=MPoint(1,p.y,p.z); normal=MVector(fixture.rejectSurface ? -1 : 1,0,0);
        if (fixture.badSurface) normal.x=std::numeric_limits<double>::quiet_NaN();
        return s;
    }
};
struct MFnDagNode {
    MFnDagNode(const MObject&,MStatus* s) { *s=operation("dag.bind"); }
    MBoundingBox boundingBox(MStatus* s) { *s=operation("dag.bounds"); return fixture.bounds; }
};
struct MFnParticleSystem {
    MObject object_;
    MFnParticleSystem() = default;
    MFnParticleSystem(const MObject& o,MStatus* s) : object_(o) { *s=operation("particle.bind"); }
    MObject object() const { return object_; }
    MStatus setObject(const MObject& o) { object_=o; return operation("particle.object"); }
    unsigned count() const { return fixture.particleCount; }
    unsigned getPerParticleAttribute(const MString&,MDoubleArray& a,MStatus* s) const {
        *s=operation("particle.scalar"); if (*s) a=fixture.scalarAttribute; return a.length();
    }
    unsigned getPerParticleAttribute(const MString&,MVectorArray& a,MStatus* s) const {
        *s=operation("particle.vector"); if (*s) a=fixture.vectorAttribute; return a.length();
    }
    void particleIds(MIntArray& a) const { a=fixture.ids; }
    void setPerParticleAttribute(const MString&,MDoubleArray& a,MStatus* s) const {
        *s=operation("lifespan.set");
        if (*s) { fixture.lifespans=a; ++fixture.lifespanWrites; }
    }
};
