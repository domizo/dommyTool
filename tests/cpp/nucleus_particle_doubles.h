#pragma once
// Run the complete production compute body. Native simulation, expression
// evaluation and DPData storage are boundaries, not substitute implementations.
#include "maya_attribute_doubles.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DPointArrayConversion.h"
#include "common/utility/DMayaAttributeArray.h"
#include "common/utility/DParticleFieldMath.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <memory>

#define CHECK_MSTATUS(s) {const MStatus checked=(s);if(!checked)checked.pAPIerror(__FILE__,__LINE__);}
#define CHECK_MSTATUS_AND_RETURN_IT(s) {if(!(s))return(s);}
#define DOMMY_MAYA_DEBUG(a,b) ((void)0)
static int shortArrayAt=-1;
static int requiredInputKind=0;
static bool requiredInputBoundary=false;
static double startTime=1.0;
static std::map<std::string,double> numericInputs;
static std::map<std::string,std::string> stringInputs;
static std::map<std::string,std::array<float,3>> vectorInputs;
using float3=float[3];

// This suite forwards scalar seconds, without changing the shared time double.
struct ParticleTime {
    enum Unit {kSeconds,kFrames};
    double time;
    Unit unit;
    explicit ParticleTime(double v=0,Unit u=kSeconds):time(v),unit(u){}
    static Unit uiUnit(){return kFrames;}
    double value()const{return time;}
    double as(Unit target)const{return time*(unit==kFrames?1.0/24:1.0)*(target==kFrames?24:1);}
    bool operator<=(const ParticleTime& other)const{return as(kSeconds)<=other.as(kSeconds);}
};
#define MTime ParticleTime
static MTime::Unit startTimeUnit=MTime::kSeconds,currentTimeUnit=MTime::kSeconds;
struct MFloatPoint {
    float x,y,z,w;
    MFloatPoint(float a=0,float b=0,float c=0,float d=1):x(a),y(b),z(c),w(d){}
    float operator[](unsigned i)const {assert(i<4);const float a[]={x,y,z,w};return a[i];}
    float& operator[](unsigned i){assert(i<4);float* a[]={&x,&y,&z,&w};return *a[i];}
    bool operator==(const MFloatPoint& p)const{return x==p.x&&y==p.y&&z==p.z&&w==p.w;}
};
struct MPoint {
    double x,y,z,w;
    MPoint(double a=0,double b=0,double c=0,double d=1):x(a),y(b),z(c),w(d){}
    MPoint(const MFloatPoint& p):x(p.x),y(p.y),z(p.z),w(p.w){}
    bool operator==(const MPoint& p)const{return x==p.x&&y==p.y&&z==p.z&&w==p.w;}
};
struct MFloatPointArray:std::vector<MFloatPoint> {
    using std::vector<MFloatPoint>::vector;
    unsigned int length()const{return size();}
    MFloatPoint& operator[](unsigned i){assert(i<size());return at(i);}
    const MFloatPoint& operator[](unsigned i)const{assert(i<size());return at(i);}
    MStatus setLength(unsigned n){const int i=callCount;const auto s=operation("array.resize");if(s){resize(n);if(i==shortArrayAt&&!empty())pop_back();}return s;}
    MStatus copy(const MFloatPointArray& other){const int i=callCount;const auto s=operation("array.copy");if(s){*this=other;if(i==shortArrayAt&&!empty())pop_back();}return s;}
};
struct MFloatArray:std::vector<float> {
    using std::vector<float>::vector;
    unsigned int length()const{return size();}
    float operator[](unsigned i)const{assert(i<size());return at(i);}
};
using DScalarDataList=std::vector<float>;
using DVec3Data=Imath::V3f;
using DVec3DataList=std::vector<DVec3Data>;

static int published=0,cleaned=0,positionWrites=0,velocityWrites=0;
static int invalidResult=0;
static unsigned invalidComponent=0;
static float invalidValue=0;
static double currentTime=2;
static bool emitEnabled=true,forceEnabled=false,realForce=false;
static std::vector<float> mergedSources;

struct PluginDataBoundary {virtual ~PluginDataBoundary()=default;};
using MTypeId=MFnData::Type;
struct DPData:PluginDataBoundary {
    static constexpr MFnData::Type id=MFnData::kPointArray;
    MFloatPointArray points{MFloatPoint(1,2,3),MFloatPoint(4,5,6)};
    MFloatPointArray velocities=MFloatPointArray(2,MFloatPoint(0,0,0,0));
    MFloatPointArray forces=MFloatPointArray(2,MFloatPoint(1,0,0,0));
    bool hasVelocity=true;
    bool validTopology=true;
    bool hasValidPointTopology()const{return validTopology;}
    std::size_t reportedCount=std::numeric_limits<std::size_t>::max();
    std::map<std::string,DScalarDataList> scalars;
    std::map<std::string,DPAttributeData::Ptr> attributes;
    const std::vector<MFloatPoint>* getPositionPtrConst()const{return &points;}
    std::size_t numberOfPoints()const{return reportedCount==std::numeric_limits<std::size_t>::max()?points.size():reportedCount;}
    void setDPData(const DPData& d){*this=d;}
    bool mergeDPData(const DPData& d){
        assert(!d.points.empty());mergedSources.push_back(d.points[0].x);
        points.insert(points.end(),d.points.begin(),d.points.end());
        velocities.insert(velocities.end(),d.velocities.begin(),d.velocities.end());
        forces.insert(forces.end(),d.forces.begin(),d.forces.end());return true;
    }
    bool isExistAttr(const std::string& key)const{return attributes.count(key)||key=="force"||(key=="vel"&&hasVelocity)||scalars.count(key);}
    // Storage is still a boundary, but typed attributes and their conversion
    // are real production classes/functions, never replacement converters.
    DPAttributeData::ConstPtr getAttr(const std::string& key)const{
        const auto supplied=attributes.find(key);if(supplied!=attributes.end())return supplied->second;
        const auto scalar=scalars.find(key);
        if(scalar!=scalars.end())return std::make_shared<DPFloatAttr>(scalar->second);
        if(key=="force"||(key=="vel"&&hasVelocity)){
            DVec3DataList values;
            for(const auto& p:key=="force"?forces:velocities)values.emplace_back(p.x,p.y,p.z);
            return std::make_shared<DPVec3fAttr>(values);
        }
        return nullptr;
    }
    void addAttrWithValues(const std::string& key,const DVec3DataList& values){assert(key=="vel");velocities.clear();for(const auto& p:values)velocities.emplace_back(p[0],p[1],p[2],0);hasVelocity=true;}
    void addAttr(const std::string& key,const DPFloatAttr& value){scalars[key]=value.value();}
    void setPosition(const MFloatPointArray& p){++positionWrites;points=p;}
    void setDPAttributeValues(const std::string& key,const MFloatPointArray& v){assert(key=="vel");++velocityWrites;velocities=v;}
    void updateBBox(){}
};
constexpr MFnData::Type DPData::id;
static DPData source,lastOutput;
static std::vector<DPData> emitters;
static std::vector<std::string> activations;
// Optional input variants: 0 valid, 1 empty, 2 wrong Maya type, 3 wrong C++ type,
// 4 correct type id with a null plugin pointer. Topology is a separate flag.
static int optionalInputKind=0;
static bool optionalInputConnected=false;
static PluginDataBoundary wrongPluginData;
static std::vector<int> emitterInputKinds;
static std::vector<unsigned> emitterLogicalIndices;
static std::vector<bool> emitterConnected;

struct MDataHandle {
    std::string attribute;
    bool valid;
    unsigned emitterIndex;
    MDataHandle(std::string a="",bool v=true,unsigned i=0):attribute(a),valid(v),emitterIndex(i){}
    MDataHandle child(const MObject& a)const{assert(valid&&emitterIndex<emitters.size());return {a.name,true,emitterIndex};}
    MTime asTime()const{assert(valid);return MTime(attribute=="aStartTime"?startTime:currentTime,attribute=="aStartTime"?startTimeUnit:currentTimeUnit);}
    int inputKind()const{
        if(requiredInputBoundary && (attribute=="inDP"||attribute=="nextDP"))return requiredInputKind;
        return attribute=="aEmitDP"&&!emitterInputKinds.empty()?emitterInputKinds.at(emitterIndex):attribute=="aEmitDP"?0:optionalInputKind;
    }
    MObject data(){assert(valid);return inputKind()==1?MObject::kNullObj:MObject{"inputData"};}
    MTypeId typeId()const{assert(valid);return inputKind()==2?MFnData::kString:DPData::id;}
    PluginDataBoundary* asPluginData()const{assert(valid&&inputKind()!=2);return inputKind()==3?&wrongPluginData:inputKind()==4?nullptr:attribute=="aEmitDP"?&emitters.at(emitterIndex):&source;}
    bool asBool()const{assert(valid);auto i=numericInputs.find(attribute);if(i!=numericInputs.end())return i->second!=0;return attribute=="aIsEmit"?emitEnabled:attribute=="aEnableForce"?forceEnabled:realForce;}
    float asFloat()const{assert(valid);auto i=numericInputs.find(attribute);return i==numericInputs.end()?1:float(i->second);}
    int asInt()const{assert(valid);auto i=numericInputs.find(attribute);return i==numericInputs.end()?1:int(i->second);}
    const float3& asFloat3()const{
        assert(valid);static float3 scratch;
        auto value=vectorInputs.find(attribute);
        for(unsigned i=0;i<3;++i)scratch[i]=value==vectorInputs.end()?float(i+1):value->second[i];
        return scratch;
    }
    MString asString()const{
        assert(valid);
        auto value=stringInputs.find(attribute);if(value!=stringInputs.end())return value->second.c_str();
        if(attribute=="aActiveEmit")return activations.at(emitterIndex).c_str();
        if(attribute=="aThickAttribute"||attribute=="aRadiusAttribute")return "pscale";
        if(attribute=="aMassAttribute")return "mass";
        return attribute=="aForceAttribute"?"force":"vel";
    }
};
struct MArrayDataHandle {
    bool valid;
    unsigned cursor=0;
    explicit MArrayDataHandle(bool v):valid(v){}
    unsigned elementCount(MStatus* s){assert(valid);*s=operation("emit.count");return emitters.size();}
    MDataHandle inputValue(MStatus* s){assert(valid&&cursor<emitters.size());*s=operation("emit.read "+text(cursor));return {"emitter",bool(*s),cursor};}
    MStatus next(){assert(valid&&cursor+1<emitters.size());const auto s=operation("emit.next");if(s)++cursor;return s;}
    unsigned elementIndex(MStatus* s)const{assert(valid);*s=operation("emit.index");return emitterLogicalIndices.empty()?cursor:emitterLogicalIndices.at(cursor);}
};
struct MPlug {
    MObject attribute;
    unsigned emitterIndex=0;
    bool valid=true;
    MPlug(MObject a):attribute(a){}
    MPlug(MObject,MObject a):attribute(a){}
    bool operator==(const MObject& a)const{return attribute.name==a.name;}
    bool operator!=(const MObject& a)const{return !(*this==a);}
    MPlug elementByLogicalIndex(unsigned index,MStatus* status)const{
        assert(valid&&attribute.name=="aEmitterList");*status=operation("emit.plugElement");MPlug result=*this;result.valid=bool(*status);
        if(*status){
            if(emitterLogicalIndices.empty())result.emitterIndex=index;
            else {const auto found=std::find(emitterLogicalIndices.begin(),emitterLogicalIndices.end(),index);assert(found!=emitterLogicalIndices.end());result.emitterIndex=found-emitterLogicalIndices.begin();}
        }
        return result;
    }
    MPlug child(MObject childAttribute,MStatus* status)const{
        assert(valid&&emitterIndex<emitters.size());*status=operation("emit.plugChild");MPlug result=*this;result.attribute=childAttribute;result.valid=bool(*status);return result;
    }
    bool isConnected(MStatus* status)const{
        assert(valid);*status=operation(attribute.name=="aEmitDP"?"emit.connected":"input.connected");
        return attribute.name=="aEmitDP"?(emitterConnected.empty()?false:emitterConnected.at(emitterIndex)):optionalInputConnected;
    }
};
struct MDataBlock {
    MDataHandle inputValue(const MObject& a,MStatus* s){*s=operation("input "+a.name);return {a.name,bool(*s)};}
    MArrayDataHandle inputArrayValue(const MObject&,MStatus* s){*s=operation("emit.array");return MArrayDataHandle(bool(*s));}
    MDataHandle outputValue(const MObject& a,MStatus* s){*s=operation("output "+a.name);return {a.name,bool(*s)};}
    MStatus setClean(const MPlug&){const auto s=operation("data.clean");if(s)++cleaned;return s;}
};
const DPData* getInputDP(const MObject&,MDataBlock&){return &source;}
void EchoWarning(const char*,const std::string&){}
void checkValidString(std::string& s,const char* fallback){if(s.empty())s=fallback;}
struct MFnPluginData {
    std::shared_ptr<DPData> candidate;
    MObject object()const{return {"pluginData"};}
    MObject create(MFnData::Type,MStatus* s){*s=operation("data.create");if(*s){candidate=std::make_shared<DPData>();candidate->points.clear();}return *s?object():MObject::kNullObj;}
};
struct DPointSeExpr {
    std::string value;
    explicit DPointSeExpr(std::string s):value(s){}
    bool usesVar(const char* s)const{return value==s;}
    bool usesPointDataFunctions()const{return value=="point()";}
    bool isValid()const{return value!="invalid";}
    std::string parseError()const{return "invalid";}
};
struct SeVec3d {double x=0;double operator[](int)const{return x;}};
namespace dommy {
namespace maya {
// The isFinitePoint implementation is extracted below from production.
MStatus createPluginData(MFnPluginData& p,MFnData::Type,DPData*& out){const auto s=operation("data.create");if(s){p.candidate=std::make_shared<DPData>();out=p.candidate.get();}return s;}
template<class T>T* pluginDataPointer(MFnPluginData& p,MStatus* s){*s=operation("data.pointer");return *s?p.candidate.get():nullptr;}
MStatus publishPluginDataObject(MDataHandle& h,const MObject& o,const DPData& d){
    assert(h.valid&&!o.isNull());const auto s=operation("publish "+h.attribute);
    if(s){lastOutput=d;++published;}return s;
}
MStatus setExpressionTime(DPointSeExpr&,const MTime&){return operation("expression.time");}
MStatus getEvaluationStepSeconds(MDataBlock&,double& dt){dt=1.0/24;return MS::kSuccess;}
}
namespace expression {
bool evaluateChecked(const DPointSeExpr& e,SeVec3d& v){v.x=e.value=="0"?0:1;return e.value!="evaluationFailure";}
}}
struct ParticleBoundary {
    unsigned count=2;
    MFloatPointArray points{MFloatPoint(10,20,30),MFloatPoint(40,50,60)};
    MFloatPointArray velocities=MFloatPointArray(2,MFloatPoint(2,3,4,0));
    bool solved=false;
    MStatus setTopology(int n){const auto s=operation("native.topology");if(s){count=n;points.resize(n);velocities.resize(n);}return s;}
    MStatus setPositions(const MFloatPointArray& p,bool initial){assert(initial);const auto s=operation("native.positions");if(s)points=p;return s;}
    std::vector<float> radii,masses;
    std::map<std::string,double> controls;
    MStatus setThickness(float v){const auto s=operation("native.radiusScalar");if(s)radii.assign(count,v);return s;}
    MStatus setThickness(float* v){assert(count>0&&v);const auto s=operation("native.radius");if(s)radii.assign(v,v+count);return s;}
    MStatus setInverseMass(float v){const auto s=operation("native.massScalar");if(s)masses.assign(count,v);return s;}
    MStatus setInverseMass(float* v){assert(count>0&&v);const auto s=operation("native.mass");if(s)masses.assign(v,v+count);return s;}
#define CONTROL_SETTER(name,type) MStatus name(type value){const auto s=operation("native." #name);if(s)controls[#name]=value;return s;}
    CONTROL_SETTER(setBounce,float) CONTROL_SETTER(setFriction,float) CONTROL_SETTER(setDamping,float)
    CONTROL_SETTER(setDisableGravity,bool) CONTROL_SETTER(setLiquidSimulation,bool)
    CONTROL_SETTER(setIncompressibility,float) CONTROL_SETTER(setRestDensity,float)
    CONTROL_SETTER(setLiquidRadiusScale,float) CONTROL_SETTER(setViscosity,float)
    CONTROL_SETTER(setSurfaceTension,float) CONTROL_SETTER(setMaxIterations,int)
    CONTROL_SETTER(setMaxSelfCollisionIterations,int) CONTROL_SETTER(setSelfCollisionSoftness,float)
    CONTROL_SETTER(setCollide,bool) CONTROL_SETTER(setSelfCollide,bool) CONTROL_SETTER(setSelfCollideWidth,float)
#undef CONTROL_SETTER
    MStatus getNumVertices(unsigned& n){const auto s=operation(solved?"result.count":"particle.count");if(s)n=count+(solved&&(invalidResult==5||invalidResult==8)?1:0);return s;}
    MStatus getPositions(MFloatPointArray& p){
        const auto s=operation("result.positions");if(s){p=points;if(invalidResult==1)p.pop_back();if(invalidResult==2||invalidResult==8)p.push_back({});
            if(invalidResult==6){float* components[]={&p[0].x,&p[0].y,&p[0].z,&p[0].w};*components[invalidComponent]=invalidValue;}}
        return s;
    }
    MStatus getVelocities(MFloatPointArray& v){
        const auto s=operation("result.velocities");if(s){v=velocities;if(invalidResult==3)v.pop_back();if(invalidResult==4||invalidResult==8)v.push_back({});
            if(invalidResult==7){float* components[]={&v[0].x,&v[0].y,&v[0].z,&v[0].w};*components[invalidComponent]=invalidValue;}}
        return s;
    }
    MStatus getInverseMass(MFloatArray& values){const auto s=operation("result.inverseMass");if(s)values.assign(masses.begin(),masses.end());return s;}
    MStatus setVelocities(const MFloatPointArray& v){const auto s=operation("native.velocities");if(s)velocities=v;return s;}
};
using MnParticle=ParticleBoundary;
struct MnSolver {
    MnSolver() = default;
    MnSolver(const MnSolver&) = default;
    float start=7.0f;
    std::map<std::string,std::vector<double>> settings;
    int solveCalls=0;
    MnSolver& operator=(const MnSolver& other){const auto s=operation("solver.replace");if(!s)throw std::runtime_error("replace failed");start=other.start;return *this;}
    MStatus createNSolver(){return operation("solver.create");}
    MStatus setStartTime(float v){assert(std::isfinite(v));const auto s=operation("solver.start");if(s)start=v;return s;}
    MStatus addNObject(ParticleBoundary*){return operation("solver.add");}
#define SOLVER_SETTER(name,type) MStatus name(type value){const auto s=operation("solver." #name);if(s)settings[#name]={double(value)};return s;}
    SOLVER_SETTER(setGravity,float) SOLVER_SETTER(setAirDensity,float)
    SOLVER_SETTER(setWindSpeed,float) SOLVER_SETTER(setWindNoiseIntensity,float)
    SOLVER_SETTER(setDisabled,bool) SOLVER_SETTER(setSubsteps,int) SOLVER_SETTER(setMaxIterations,int)
#undef SOLVER_SETTER
    MStatus setGravityDir(float x,float y,float z){const auto s=operation("solver.setGravityDir");if(s)settings["setGravityDir"]={x,y,z};return s;}
    MStatus setWindDir(float x,float y,float z){const auto s=operation("solver.setWindDir");if(s)settings["setWindDir"]={x,y,z};return s;}
    MStatus solve(float value){const auto s=operation("solver.solve");if(s){settings["solve"]={value};++solveCalls;}return s;}
};
struct ParticleNodeBoundary {
    ParticleBoundary fNObject;
    MnSolver solver;
    std::vector<int*> colObjList;
    MString name()const{return "DPnParticle";}
    MObject thisMObject()const{return {"node"};}
    MStatus ensureNParticle(){return operation("particle.ensure");}
    MStatus initializeDPDataParticle(DPData&,MDataBlock&){return operation("particle.init");}
    MStatus initializeDPSolver(MDataBlock&){return operation("solver.init");}
    MStatus updateDPDataParticle(DPData& d,MDataBlock&){
        const auto s=operation("particle.update");if(s){fNObject.count=d.numberOfPoints();fNObject.points.resize(fNObject.count,MFloatPoint(7,8,9));fNObject.velocities.resize(fNObject.count,MFloatPoint(4,5,6,0));}return s;
    }
    MStatus syncCollisions(MDataBlock&){return operation("collisions");}
    MStatus calculateDPSolver(MDataBlock&,float){const auto s=operation("solver.solve");if(s)fNObject.solved=true;return s;}
};
