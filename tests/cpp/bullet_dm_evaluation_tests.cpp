// Runs both complete production compute bodies with failure-injectable host
// and world doubles. Native field math is tested separately with Bullet 2.88.
#include "Solver/DBulletEvaluation.h"
#include "Solver/DBulletStep.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DGeometryBatchBudget.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

// Logging is a host double here; complete initialize boundaries are exercised
// separately by plugin_initialization_fixture.cpp with the real entry guard.
namespace dommy { namespace maya {
inline MStatus reportEntryPointException(const char*, const char*, const std::exception_ptr&) {
    return MS::kFailure;
}
} }

static int calls, failAt, cleanCalls, steps, fieldCalls, initializations;
static bool failFields, throwStep, playback;
static int contactCount=1, manifoldReads=0;
static float contactPosition=3;
static unsigned inputArrayCount=0;
static std::map<int,std::vector<int>> connectedIndices;
static unsigned disconnectedSlots=0;
static std::vector<unsigned> publishedIndices;
static std::map<int,std::vector<unsigned>> publishedArrays;
static std::map<int,unsigned> physicalRequests;
static int excessiveAttribute=-1;
static bool emptyPayload=false;
static std::map<int,std::size_t> inputPointCosts,inputWorkCosts;
static int copiedPayloads=0;
static int failedDataAttribute=-1,nullDataAttribute=-1,invalidTopologyAttribute=-1,invalidUuidAttribute=-1;
static bool validReadTopology=true;
static const std::vector<int> defaultConnectedIndices{3};
static const std::vector<int> noConnectedIndices;
static MStatus operation() { return ++calls == failAt ? MS::kFailure : MS::kSuccess; }
static void resetFailures() {
    calls = failAt = cleanCalls = 0; failFields = throwStep = false;
    failedDataAttribute=nullDataAttribute=invalidTopologyAttribute=invalidUuidAttribute=-1;
    validReadTopology=true;
    excessiveAttribute=-1;physicalRequests.clear();emptyPayload=false;
    inputPointCosts.clear();inputWorkCosts.clear();copiedPayloads=0;
}
struct MObject {
    int value; std::shared_ptr<std::vector<double>> payload;
    MObject(int v=0):value(v) {} bool isNull() const { return value==0; }
};
static MObject publishedContacts;
struct MString { const char* asChar() const { return "test"; } };
struct MTime {
    enum {kSeconds}; double t;
    MTime(double v=0):t(v) {} double value() const {return t;}
    double as(int) const {return t/60;}
    MTime operator-(MTime rhs) const {return MTime(t-rhs.t);}
    bool operator==(MTime rhs) const {return t==rhs.t;}
};
struct MVector { double v[3]={0,0,0}; double operator[](int i) const {return v[i];} };
enum { inDC=1,inDM,inSoftDM,inRopeDC,inKinematicDCs,inKinematicDMs,inConst,
       outDC,outDM,outSoftDM,outRopeDC,outContactDP,aEnabled,aCurrentTime,aStartTime,
       aSubsteps,aFixedPhysicsRate,aGravity,aSplitImpulse,aAffectMayaFiels,
       aIsCheckAndDeletePanicSoftBody,aContactData,aIsPlane,aPlanePosition,aPlaneAxis,
       aPlaneRestitution,aPlaneFriction,aAirDensity,aWaterDensity,aWaterOffset,
       aWaterNormal,aMaxDisplacement };
struct MPlug;
struct MPlugArray : std::vector<MPlug> { unsigned length() const {return size();} };
static const std::vector<int>& inputConnections(int attribute) {
    const auto found=connectedIndices.find(attribute);
    if(found!=connectedIndices.end())return found->second;
    return attribute<=inRopeDC||attribute==inKinematicDCs||attribute==inKinematicDMs?
        defaultConnectedIndices:noConnectedIndices;
}
struct MPlug {
    int value,index; MPlug(int v=0,int element=-1):value(v),index(element) {}
    bool operator==(int v) const {return value==v;}
    unsigned numElements(MStatus* s=nullptr) const {if(s)*s=operation();return inputConnections(value).size()+disconnectedSlots;}
    unsigned numConnectedElements(MStatus* s=nullptr) const {
        if(s)*s=operation();
        return value==excessiveAttribute?unsigned(dommy::generation::kMaxConnectedGeometryCount+1):inputConnections(value).size();
    }
    MPlug connectionByPhysicalIndex(std::size_t physical,MStatus* s=nullptr) const {
        ++physicalRequests[value];
        if(s)*s=operation();
        const auto& indices=inputConnections(value);
        if(physical>=indices.size()){if(s)*s=MS::kInvalidParameter;return MPlug();}
        return MPlug(value,indices[physical]);
    }
    bool isConnected(MStatus* s=nullptr) const {if(s)*s=operation();return true;}
    bool isArray() const {return true;}
    MString name() const {return {};}
    MObject node(MStatus* s=nullptr) const {if(s)*s=operation();return MObject(value);}
    bool connectedTo(MPlugArray& a,bool,bool,MStatus* s) const {
        *s=operation(); a.push_back(*this); return true;
    }
};
struct MUuid {
    int id;
    MUuid(int value=0):id(value){}
    bool valid()const{return id!=0;}
    bool operator==(const MUuid& rhs)const{return id==rhs.id;}
};
using MTypeId=int;
using UuidList=std::vector<MUuid>;
struct MFnDependencyNode {
    MObject object; explicit MFnDependencyNode(MObject o,MStatus* s=nullptr):object(o) {if(s)*s=operation();}
    MPlug findPlug(int a,bool,MStatus* s=nullptr) const {if(s)*s=operation();return MPlug(a);}
    MUuid uuid(MStatus* s=nullptr) const {if(s)*s=operation();return MUuid(object.value==invalidUuidAttribute?0:object.value);}
    int typeId(MStatus* s=nullptr) const {if(s)*s=operation();return object.value;}
};
struct DMbtSoft {enum {typeId=inSoftDM};};
struct DCbtRope {enum {typeId=inRopeDC};};
struct GeometryData {
    static const int id=1;
    std::size_t points=1U,metadataWork=0U;
    MStatus copyFrom(const GeometryData&) {++copiedPayloads;return operation();}
    void updateBBox() {}
    unsigned numberOfCurves() const {return emptyPayload?0:1;}
    unsigned numberOfMesh() const {return emptyPayload?0:1;}
    bool deleteFromIds(const std::vector<int>&) {return bool(operation());}
};
using DCurveData=GeometryData; using DMeshData=GeometryData;
using DCurveDataPtrList=std::vector<GeometryData*>; using DMeshDataPtrList=DCurveDataPtrList;
namespace dommy {namespace bullet {
// Real typed cost inspection is tested separately. The complete evaluation
// executes the shared batch policy using these synthetic logical counts.
bool admitCurveInput(const GeometryData& source,geometry::GeometryBatchBudget& batch,bool retain=true){
    mesh::MeshMergePlan plan;
    if(!generation::checkedSum(0U,source.points,generation::kMaxGeneratedPointCount,plan.points))return false;
    plan.meshes=emptyPayload?0U:1U;plan.workItems=source.points;
    return batch.append(plan,source.metadataWork,0U,retain);
}
bool admitMeshInput(const GeometryData& source,geometry::GeometryBatchBudget& batch,bool retain=true){
    return admitCurveInput(source,batch,retain);
}
}}
using DPIdList=std::vector<int>;
struct MFnPluginData {
    MObject create(int,MStatus* s) { *s=operation(); return MObject(1); }
};
static GeometryData geometry;
struct MDataHandle {
    int attribute=0; double time=0,value=0; bool enabled=true,contacts=false;
    bool valid=true;
    void check() const {assert(valid && "failed Maya handle consumed");}
    bool asBool() const {check();return attribute==aEnabled?enabled:attribute==aContactData?contacts:attribute==aAffectMayaFiels || value!=0;}
    int asInt() const {check();return static_cast<int>(value);}
    double asDouble() const {check();return value;}
    MTime asTime() const {check();return MTime(attribute==aStartTime?value:time);}
    MVector asVector() const {check();MVector result;result.v[0]=value;return result;}
    MStatus set(const MObject& object) {
        assert(cleanCalls==0);MStatus status=operation();
        if(status && attribute==outContactDP)publishedContacts=object;
        return status;
    }
    void setClean() {++cleanCalls;}
};
struct MDataBlock;
struct MArrayDataBuilder {
    std::vector<unsigned> indices;
    int attribute;
    MArrayDataBuilder(MDataBlock*,MObject output,unsigned,MStatus* s):attribute(output.value) {*s=operation();}
    MDataHandle addElement(unsigned index,MStatus* s) {*s=operation();if(*s)indices.push_back(index);return {};}
};
struct MArrayDataHandle {
    unsigned elementCount(MStatus* status=nullptr) const {if(status)*status=operation();return inputArrayCount;}
    MStatus set(const MArrayDataBuilder& builder) {
        const MStatus status=operation();
        if(status){publishedIndices=builder.indices;publishedArrays[builder.attribute]=builder.indices;}return status;
    }
    MStatus setAllClean() {MStatus s=operation(); if(s)++cleanCalls; return s;}
};
struct MDataBlock {
    double time=0; bool enabled=true,contacts=false;
    std::map<int,double> values{{aSubsteps,1},{aFixedPhysicsRate,60}};
    MDataHandle inputValue(MObject a,MStatus* s) {
        if(a.value==aEnabled)cleanCalls=0;*s=operation();MDataHandle h;
        h.attribute=a.value;h.time=time;h.enabled=enabled;h.contacts=contacts;
        h.value=values[a.value];h.valid=bool(*s);return h;
    }
    MArrayDataHandle inputArrayValue(int,MStatus* s) {*s=operation();return {};}
    MArrayDataHandle outputArrayValue(MObject,MStatus* s) {*s=operation();return {};}
    MDataHandle outputValue(int a,MStatus* s) {*s=operation();MDataHandle h;h.attribute=a;return h;}
    MStatus setClean(const MPlug&) {MStatus s=operation(); if(s)++cleanCalls; return s;}
};
#include "dm_state.inc"
static std::set<std::pair<int,int>> dependencies;
static MStatus attributeAffects(MObject input,MObject output) {
    MStatus status=operation();if(status)dependencies.insert({input.value,output.value});return status;
}
#include "dm_dependencies.inc"
struct MConditionMessage { static bool getConditionState(const char*,MStatus* s) {*s=operation();return playback;} };
struct DPoint {float x,y,z;DPoint(float a,float b,float c):x(a),y(b),z(c){} }; using DVec3Data=DPoint;
using DPointList=std::vector<DPoint>; using DFloatArray=std::vector<float>; using DVec3DataList=DPointList;
using DPFloatAttr=DFloatArray; using DPVec3fAttr=DVec3DataList;
struct DPData {
    static const int id=2;std::shared_ptr<std::vector<double>> payload{new std::vector<double>};
    void setPosition(const DPointList& points){for(const auto& p:points){payload->push_back(p.x);payload->push_back(p.y);payload->push_back(p.z);}}
    void updateBBox(){}
    void addAttr(const char*,const DFloatArray& values){payload->insert(payload->end(),values.begin(),values.end());}
    void addAttr(const char*,const DVec3DataList& values){setPosition(values);}
};
namespace dommy { namespace maya {
inline bool isFloatRepresentableVector(const MVector&) {return true;}
inline MStatus getPluginData(MFnPluginData&,GeometryData*& output) {output=&::geometry;return operation();}
template<class T>MStatus getPluginDataFromPlug(const MPlug& plug,int,MObject& owner,const T*& output) {
    owner=MObject();output=nullptr;
    const MStatus status=operation();if(!status)return status;
    if(plug.value==failedDataAttribute)return MS::kInvalidParameter;
    if(plug.value==nullDataAttribute)return MS::kFailure;
    owner=MObject(1);output=&::geometry;validReadTopology=plug.value!=invalidTopologyAttribute;
    ::geometry.points=inputPointCosts.count(plug.value)?inputPointCosts.at(plug.value):1U;
    ::geometry.metadataWork=inputWorkCosts.count(plug.value)?inputWorkCosts.at(plug.value):0U;
    return MS::kSuccess;
}
template<class T>bool validatePluginData(const T&){return validReadTopology;}
template<class T,class F>MStatus createPluginDataObject(int,F populate,MObject& object) {
    MStatus s=operation();if(!s)return s; T data;populate(data);object=MObject(1);object.payload=data.payload;return MS::kSuccess;
}
} }
#include "dm_output.inc"
struct btVector3 {float v[3];float operator[](unsigned i)const{return v[i];}float x()const{return v[0];}float y()const{return v[1];}float z()const{return v[2];}btVector3(double a=0,double b=0,double c=0):v{float(a),float(b),float(c)}{} };
struct btManifoldPoint {btVector3 m_normalWorldOnB{0,1,0};btVector3 getPositionWorldOnA()const{return {contactPosition,2,3};}float getDistance()const{return -1;}float getAppliedImpulse()const{return 4;}int getLifeTime()const{return 5;} };
struct btPersistentManifold {int getNumContacts()const{return contactCount;}const btManifoldPoint& getContactPoint(int)const{static btManifoldPoint p;return p;} };
struct Dispatcher {int getNumManifolds(){++manifoldReads;return 1;}btPersistentManifold* getManifoldByIndexInternal(int){static btPersistentManifold m;return &m;} };
using btDispatcher=Dispatcher;
struct btCollisionWorld {Dispatcher* getDispatcher(){static Dispatcher d;return &d;} };
namespace dommy { namespace bullet {using SoftBodyForces=std::vector<int>;} }
struct World : btCollisionWorld {
    struct Info {bool m_splitImpulse=false;} info;
    btVector3 gravity;
    bool canAdvance(float,float,int){return true;}
    void setGravity(btVector3 value) {gravity=value;}
    const btVector3& getGravity() const {return gravity;}
    Info& getSolverInfo(){return info;}
    bool stepChecked(float,float,const dommy::bullet::SoftBodyForces& = {}) {++steps;if(throwStep)throw std::runtime_error("step");return true;}
    btCollisionWorld* getCollisionWorld(){return this;}
    int getNumCollisionObjects(){return 0;}
};
struct btSoftBodyWorldInfo {btVector3 m_gravity;};
enum { DB_SLV_2D_TRANS_KINEMA,DB_SLV_2D_UPDATE_CONST,DB_SLV_TRANS_KINEMA,DB_SLV_TRANS_ROPE,DB_SLV_UPDATE_CONST,DB_SLV_UPDATE_ROPE };
struct DBtSolverOptionParams {int m_solver_proc_type;float m_timestep;};
using DBt2DSolverOptionParams=DBtSolverOptionParams;
struct NodeBase {
    bool m_requiresStartFrame=true,m_reInitialize=false; MTime m_prevTime;
    std::vector<double> m_worldSettings;
    std::unique_ptr<World> m_dynamicsWorld;
    std::unique_ptr<btSoftBodyWorldInfo> m_worldInfo;
    MObject thisMObject(){return MObject(1000);} MString name(){return {};}
    const GeometryData* getInputDCfromPlug(MPlug plug,MObject& owner){
        const GeometryData* input=nullptr;
        const MStatus status=dommy::maya::getPluginDataFromPlug(plug,GeometryData::id,owner,input);
        return status&&input&&dommy::maya::validatePluginData(*input)?input:nullptr;
    }
    const GeometryData* getInputDMfromPlug(MPlug plug,MObject& owner){return getInputDCfromPlug(plug,owner);}
    void clearBullet(){m_dynamicsWorld.reset();m_worldInfo.reset();} void deleteRigidBodies(){}
    void initBullet(MDataBlock& data){
        ++initializations;m_dynamicsWorld.reset(new World);m_worldInfo.reset(new btSoftBodyWorldInfo);
        m_dynamicsWorld->setGravity(btVector3(data.values[aGravity],0,0));
        m_worldInfo->m_gravity=m_dynamicsWorld->getGravity();
    }
    bool failInit=false,failUpdate=false;
    int failProcess=-1;
    MStatus initRigidBodies(){return failInit?MS::kFailure:MS::kSuccess;}
    MStatus checkRigidBodies(){return failUpdate?MS::kFailure:MS::kSuccess;}
    MStatus solverProcessFromPlug(int,const DBtSolverOptionParams& option){return option.m_solver_proc_type==failProcess?MS::kFailure:MS::kSuccess;}
    bool failOutput=false;
    MStatus updateRigidBodies(DCurveDataPtrList*,UuidList*){return failOutput?MS::kFailure:MS::kSuccess;}
    void updateSoftBodies(DMeshDataPtrList*,UuidList*,std::vector<DPIdList>*){}
    bool updateRopes(DCurveDataPtrList*,UuidList*){return true;}
    MStatus applyFields(MDataBlock&,float){++fieldCalls;return failFields?MS::kFailure:MS::kSuccess;}
    MStatus applyFields(MDataBlock& d,float t,dommy::bullet::SoftBodyForces&){return applyFields(d,t);}
};
struct DCBullet2DSolver : NodeBase {MStatus compute(const MPlug&,MDataBlock&);};
struct DMBulletTest : NodeBase {
    #include "dm_contact_members.inc"
    MStatus compute(const MPlug&,MDataBlock&);
};
#define DM_BULLET_2D_DEBUG 0
#define DOMMY_MAYA_DEBUG(...) ((void)0)
static void EchoWarning(const char*,const char*) {}
#include "dm_compute.inc"

static void checkSparseInputs() {
    connectedIndices[inDC]={3,101};connectedIndices[inKinematicDCs]={17};
    disconnectedSlots=4;
    resetFailures();DCBullet2DSolver node;MDataBlock data;
    assert(node.compute(MPlug(outDC),data));
    assert(publishedIndices==std::vector<unsigned>({0,1}));
    data.time=1;assert(node.compute(MPlug(outDC),data));
    assert(publishedIndices==std::vector<unsigned>({0,1}));
    connectedIndices[inDC].clear();connectedIndices[inKinematicDCs].clear();
    resetFailures();assert(node.compute(MPlug(outDC),data));
    assert(publishedIndices.empty());
    connectedIndices.clear();disconnectedSlots=0;
    connectedIndices[inDM]={3,101};connectedIndices[inSoftDM]={7,88};
    connectedIndices[inRopeDC]={27,99};connectedIndices[inKinematicDMs]={17};
    disconnectedSlots=4;resetFailures();DMBulletTest meshNode;MDataBlock meshBlock;
    assert(meshNode.compute(MPlug(outDM),meshBlock));
    for(int output:{outDM,outSoftDM,outRopeDC})assert(publishedArrays[output]==std::vector<unsigned>({0,1}));
    for(auto& input:connectedIndices)input.second.clear();
    meshBlock.time=1;assert(meshNode.compute(MPlug(outDM),meshBlock));
    for(int output:{outDM,outSoftDM,outRopeDC})assert(publishedArrays[output].empty());
    connectedIndices.clear();disconnectedSlots=0;
    std::puts("2D inputs: sparse connected indices, retained unconnected slots and compact/empty outputs passed");
}

static void checkDataAdmission() {
    for(int attribute:{inDC,inKinematicDCs})for(int failure=0;failure<4;++failure){
        resetFailures();DCBullet2DSolver node;MDataBlock data;
        assert(node.compute(MPlug(outDC),data));
        const auto oldOutput=publishedIndices;const int oldSteps=steps,oldInit=initializations;
        if(failure==0)failedDataAttribute=attribute;
        if(failure==1)nullDataAttribute=attribute;
        if(failure==2)invalidTopologyAttribute=attribute;
        if(failure==3)invalidUuidAttribute=attribute;
        data.time=1;const MStatus status=node.compute(MPlug(outDC),data);
        // Kinematic UUIDs are not used; only geometry owner identity is required.
        if(failure==3&&attribute==inKinematicDCs){assert(status);continue;}
        assert(!status&&node.m_prevTime.t==0&&steps==oldSteps&&initializations==oldInit);
        assert(status==(failure==1?MS::kFailure:MS::kInvalidParameter));
        assert(publishedIndices==oldOutput);
        resetFailures();assert(node.compute(MPlug(outDC),data));
    }
    connectedIndices[inKinematicDMs]={17};
    for(int attribute:{inDM,inSoftDM,inRopeDC,inKinematicDMs})for(int failure=0;failure<4;++failure){
        resetFailures();DMBulletTest node;MDataBlock data;assert(node.compute(MPlug(outDM),data));
        const auto oldOutput=publishedArrays;const int oldSteps=steps,oldInit=initializations;
        if(failure==0)failedDataAttribute=attribute;
        if(failure==1)nullDataAttribute=attribute;
        if(failure==2)invalidTopologyAttribute=attribute;
        if(failure==3)invalidUuidAttribute=attribute;
        data.time=1;const MStatus status=node.compute(MPlug(outDM),data);
        if(failure==3&&attribute==inKinematicDMs){assert(status);continue;}
        assert(!status&&node.m_prevTime.t==0&&steps==oldSteps&&initializations==oldInit);
        assert(status==(failure==1?MS::kFailure:MS::kInvalidParameter));
        assert(publishedArrays==oldOutput);
        resetFailures();assert(node.compute(MPlug(outDM),data));
    }
    connectedIndices.clear();
    for(int attribute:{inDC,inKinematicDCs,inDM,inSoftDM,inRopeDC,inKinematicDMs}){
        resetFailures();excessiveAttribute=attribute;const int oldSteps=steps;
        MDataBlock data;DCBullet2DSolver curve;DMBulletTest mesh;
        const MStatus status=attribute==inDC||attribute==inKinematicDCs?
            curve.compute(MPlug(outDC),data):mesh.compute(MPlug(outDM),data);
        assert(status==MS::kInvalidParameter&&physicalRequests[attribute]==0&&steps==oldSteps);
    }
    resetFailures();emptyPayload=true;DCBullet2DSolver empty;MDataBlock data;
    assert(empty.compute(MPlug(outDC),data)&&publishedArrays[outDC].size()==1);
    for(int attribute:{inDC,inKinematicDCs,inDM,inSoftDM,inRopeDC,inKinematicDMs}){
        resetFailures();connectedIndices[attribute]={3,9};
        const bool retained=attribute!=inKinematicDCs&&attribute!=inKinematicDMs;
        if(retained)inputPointCosts[attribute]=1200000U;
        else inputWorkCosts[attribute]=70000000U;
        const int oldSteps=steps,oldInit=initializations;
        DCBullet2DSolver curve;DMBulletTest mesh;MDataBlock block;
        const MStatus status=attribute==inDC||attribute==inKinematicDCs?
            curve.compute(MPlug(outDC),block):mesh.compute(MPlug(outDM),block);
        assert(status==MS::kInvalidParameter&&steps==oldSteps&&initializations==oldInit&&cleanCalls==0);
        assert(copiedPayloads<=3);
        connectedIndices.clear();resetFailures();
        assert(attribute==inDC||attribute==inKinematicDCs?
            curve.compute(MPlug(outDC),block):mesh.compute(MPlug(outDM),block));
    }
    resetFailures();inputPointCosts[inDM]=1200000U;inputPointCosts[inSoftDM]=1200000U;
    DMBulletTest combined;MDataBlock combinedBlock;
    assert(combined.compute(MPlug(outDM),combinedBlock)==MS::kInvalidParameter&&copiedPayloads==1);
    resetFailures();
    std::puts("2D connected payload: read/type/topology/UUID failure preserves outputs, world and time; retry passed");
}

template<class Node>void checkNode(int output) {
    resetFailures(); Node node; MDataBlock data;
    assert(node.compute(MPlug(output),data));
    data.time=1;assert(node.compute(MPlug(output),data));
    data.time=2;assert(node.compute(MPlug(output),data));
    const int before=steps;
    assert(node.compute(MPlug(output),data));assert(steps==before);
    data.time=1;assert(!node.compute(MPlug(output),data));
    assert(node.m_requiresStartFrame && node.m_prevTime.t==2 && steps==before);
    data.time=2;assert(!node.compute(MPlug(output),data));assert(steps==before);
    data.time=0;assert(node.compute(MPlug(output),data));
    data.time=3;assert(!node.compute(MPlug(output),data));
    data.time=0;assert(node.compute(MPlug(output),data));
    data.time=1;failFields=true;assert(!node.compute(MPlug(output),data));
    assert(node.m_requiresStartFrame && node.m_prevTime.t==0);failFields=false;
    assert(!node.compute(MPlug(output),data));
    data.time=0;assert(node.compute(MPlug(output),data));data.time=1;throwStep=true;
    assert(!node.compute(MPlug(output),data));assert(node.m_requiresStartFrame && node.m_prevTime.t==0);
    throwStep=false;data.time=0;assert(node.compute(MPlug(output),data));data.time=1;
    calls=0;assert(node.compute(MPlug(output),data));const int total=calls;
    int failures=0;
    for(int phase=0;phase<2;++phase) for(int failure=1;failure<=total;++failure) {
        resetFailures(); Node candidate;data.time=0;assert(candidate.compute(MPlug(output),data));
        if(phase==1){data.time=1;assert(candidate.compute(MPlug(output),data));}
        calls=0;failAt=failure;data.time=phase==0?1:0;const int oldSteps=steps,oldInit=initializations;
        const MStatus status=candidate.compute(MPlug(output),data);
        assert(!status);assert(candidate.m_prevTime.t==(phase==0?0:1));++failures;
        if(steps>oldSteps || initializations>oldInit) {
            assert(candidate.m_requiresStartFrame);resetFailures();
            const int afterSteps=steps;data.time=1;
            assert(!candidate.compute(MPlug(output),data));assert(steps==afterSteps);
        }
        resetFailures();data.time=0;assert(candidate.compute(MPlug(output),data));
        assert(!candidate.m_requiresStartFrame);
    }
    resetFailures();Node disabled;data.enabled=false;data.time=50;
    const int oldSteps=steps;assert(disabled.compute(MPlug(output),data));
    assert(disabled.m_requiresStartFrame && disabled.m_prevTime.t==0 && steps==oldSteps);
    std::printf("DM compute output %d: %d host failures, fields, exceptions, rewind/retry/reset passed\n",output,failures);
}
static void checkDependencies() {
    for(bool mesh : {false,true}) {
        resetFailures();dependencies.clear();
        auto initialize=mesh?DMBulletTestDependencies:DCBullet2DSolverDependencies;
        assert(initialize());const int count=calls;
        std::vector<int> inputs={inConst,aCurrentTime,aEnabled,aStartTime,aGravity,
            aSplitImpulse,aSubsteps,aFixedPhysicsRate,aAffectMayaFiels,aIsPlane,
            aPlanePosition,aPlaneAxis,aPlaneRestitution,aPlaneFriction};
        const std::vector<int> outputs=mesh?std::vector<int>{outDM,outSoftDM,outRopeDC,outContactDP}:std::vector<int>{outDC};
        if(mesh)inputs.insert(inputs.end(),{inDM,inKinematicDMs,inSoftDM,inRopeDC,
            aAirDensity,aWaterDensity,aWaterOffset,aWaterNormal,aMaxDisplacement,
            aContactData,aIsCheckAndDeletePanicSoftBody});
        else inputs.insert(inputs.end(),{inDC,inKinematicDCs});
        assert(dependencies.size()==inputs.size()*outputs.size());
        for(int input:inputs)for(int output:outputs)assert(dependencies.count({input,output})==1);
        for(int i=1;i<=count;++i){resetFailures();failAt=i;assert(!initialize());assert(calls==i);}
        std::printf("DM dependency pairs and failure propagation: %d passed\n",count);
    }
}

template<class Node>static void checkSettings(int output,bool mesh) {
    std::vector<int> settings={aStartTime,aIsPlane,aPlanePosition,aPlaneAxis,aPlaneRestitution,aPlaneFriction};
    if(mesh)settings.insert(settings.end(),{aAirDensity,aWaterDensity,aWaterOffset,aWaterNormal,aMaxDisplacement});
    for(int attribute:settings){
        resetFailures();Node node;MDataBlock data;data.values[aIsPlane]=1;data.values[aPlaneAxis]=1;
        assert(node.compute(MPlug(output),data));data.time=1;assert(node.compute(MPlug(output),data));
        const int previous=steps;const auto oldSettings=node.m_worldSettings;
        if(attribute==aIsPlane)data.values[attribute]=0;
        else data.values[attribute]+=2;
        assert(!node.compute(MPlug(output),data));assert(node.m_requiresStartFrame);
        assert(steps==previous && node.m_worldSettings==oldSettings);
        data.time=data.values[aStartTime];assert(node.compute(MPlug(output),data));
        assert(!node.m_requiresStartFrame);
    }
    resetFailures();Node node;MDataBlock data;assert(node.compute(MPlug(output),data));
    data.time=1;assert(node.compute(MPlug(output),data));const int previous=steps;
    data.values[aPlanePosition]=100; // Inactive plane settings do not require reset.
    data.values[aGravity]=2;data.values[aFixedPhysicsRate]=120;data.values[aSubsteps]=2;
    assert(node.compute(MPlug(output),data));assert(!node.m_requiresStartFrame && steps==previous);
    data.time=2;assert(node.compute(MPlug(output),data));assert(steps==previous+2);
}

static void checkContacts() {
    resetFailures();DMBulletTest node;MDataBlock data;data.contacts=true;data.values[aSubsteps]=3;
    assert(node.compute(MPlug(outContactDP),data));assert(publishedContacts.payload->empty());
    data.time=1;assert(node.compute(MPlug(outRopeDC),data));
    const auto expected=*publishedContacts.payload;const auto firstObject=publishedContacts.payload;
    assert(expected==std::vector<double>({3,2,3,3,2,3,3,2,3,-1,-1,-1,4,4,4,5,5,5,0,1,0,0,1,0,0,1,0}));
    const int previous=steps,reads=manifoldReads;contactPosition=99;
    for(int output:{outDM,outSoftDM,outRopeDC,outContactDP})for(int element:{-1,0,37}) {
        if(output==outContactDP && element!=-1)continue;
        assert(node.compute(MPlug(output,element),data));assert(*publishedContacts.payload==expected);
        assert(publishedContacts.payload!=firstObject && steps==previous && manifoldReads==reads);
    }
    assert(node.compute(MPlug(inDM),data)==MS::kUnknownParameter);
    data.contacts=false;assert(node.compute(MPlug(outContactDP),data));assert(publishedContacts.payload->empty());
    data.contacts=true;assert(node.compute(MPlug(outContactDP),data));assert(*publishedContacts.payload==expected);
    data.enabled=false;assert(node.compute(MPlug(outContactDP),data));assert(publishedContacts.payload->empty());
    data.enabled=true;assert(node.compute(MPlug(outContactDP),data));assert(*publishedContacts.payload==expected);
    // Cache refresh never happens before publication completes.
    calls=0;data.time=2;assert(node.compute(MPlug(outDM),data));const int count=calls;
    for(int failure=1;failure<=count;++failure){
        resetFailures();DMBulletTest candidate;MDataBlock block;block.contacts=true;
        contactPosition=3;
        assert(candidate.compute(MPlug(outDM),block));block.time=1;assert(candidate.compute(MPlug(outDM),block));
        const auto cached=candidate.m_contactSamples.positions;
        calls=0;failAt=failure;block.time=2;contactPosition=99;
        // The three-substep reference and one-substep failures make the same
        // host API calls; native contacts are deliberately not host calls.
        assert(!candidate.compute(MPlug(outDM),block));
        assert(candidate.m_contactSamples.positions.size()==cached.size());
        assert(candidate.m_contactSamples.positions[0].x==cached[0].x);
    }
    resetFailures();data.time=0;assert(node.compute(MPlug(outContactDP),data));assert(publishedContacts.payload->empty());
    data.contacts=false;data.time=1;assert(node.compute(MPlug(outDM),data));
    data.contacts=true;contactCount=2;
    assert(node.compute(MPlug(outContactDP),data));assert(publishedContacts.payload->size()==18);
    const int collected=manifoldReads;assert(node.compute(MPlug(outContactDP),data));assert(manifoldReads==collected);
    // Invalid/oversized native samples fail before cache publication.
    data.contacts=false;data.time=2;assert(node.compute(MPlug(outDM),data));data.contacts=true;
    contactCount=int(dommy::generation::kMaxGeneratedPointCount)+1;
    assert(node.compute(MPlug(outContactDP),data)==MS::kInvalidParameter);
    assert(!node.m_contactSamplesValid);
    contactCount=1;contactPosition=std::numeric_limits<float>::infinity();
    assert(node.compute(MPlug(outContactDP),data)==MS::kInvalidParameter);
    contactPosition=3;assert(node.compute(MPlug(outContactDP),data));
    std::printf("DM all output requests, repeated-frame payloads, toggles, bounds and %d cache failures passed\n",count);
}

static void checkSettingsReader() {
    resetFailures();MDataBlock data;data.values[aIsPlane]=1;data.values[aPlaneAxis]=1;
    const dommy::bullet::PlaneAttributes attributes{
        aIsPlane,aPlanePosition,aPlaneAxis,aPlaneRestitution,aPlaneFriction};
    std::vector<double> settings{99};
    assert(dommy::bullet::readPlaneRestartSettings(data,0,attributes,settings));
    const int count=calls;
    for(int failure=1;failure<=count;++failure){
        resetFailures();failAt=failure;settings={99};
        assert(!dommy::bullet::readPlaneRestartSettings(data,0,attributes,settings));
        assert(settings==std::vector<double>{99});
    }
    for(int attribute:{aPlanePosition,aPlaneAxis,aPlaneRestitution,aPlaneFriction}) {
        resetFailures();data.values[attribute]=std::numeric_limits<double>::quiet_NaN();settings={99};
        assert(dommy::bullet::readPlaneRestartSettings(data,0,attributes,settings)==MS::kInvalidParameter);
        assert(settings==std::vector<double>{99});data.values[attribute]=0;
    }
    resetFailures();data.values[aIsPlane]=0;data.values[aPlanePosition]=std::numeric_limits<double>::infinity();
    assert(dommy::bullet::readPlaneRestartSettings(data,0,attributes,settings));
    assert(settings==std::vector<double>({0,0}));
    assert(dommy::bullet::readPlaneRestartSettings(data,std::numeric_limits<double>::infinity(),attributes,settings)==MS::kInvalidParameter);
}

static void checkSoftGravity() {
    resetFailures();DMBulletTest node;MDataBlock data;
    data.values[aGravity]=2;
    assert(node.compute(MPlug(outSoftDM),data));
    assert(node.m_worldInfo->m_gravity.x()==2);
    data.values[aGravity]=-9.8;data.time=1;
    assert(node.compute(MPlug(outSoftDM),data));
    assert(node.m_dynamicsWorld->getGravity().x()==float(-9.8));
    assert(node.m_worldInfo->m_gravity.x()==float(-9.8));
    const int previous=steps;
    data.values[aGravity]=4;
    assert(node.compute(MPlug(outSoftDM),data));
    assert(steps==previous && node.m_worldInfo->m_gravity.x()==float(-9.8));
    data.enabled=false;data.time=2;
    assert(node.compute(MPlug(outSoftDM),data));
    assert(steps==previous && node.m_worldInfo->m_gravity.x()==float(-9.8));
    data.enabled=true;
    assert(node.compute(MPlug(outSoftDM),data));
    assert(node.m_worldInfo->m_gravity.x()==4);
    for(double value:{0.0,1.0}) {
        data.time+=1;data.values[aGravity]=value;
        assert(node.compute(MPlug(outSoftDM),data));
        assert(node.m_worldInfo->m_gravity.x()==value);
        assert(node.m_dynamicsWorld->getGravity().x()==value);
    }
    // Reject an incomplete native state before dereferencing its world info,
    // including same-frame reads; StartTime recreates both descriptors.
    for(bool advance:{false,true}) {
        resetFailures();DMBulletTest candidate;MDataBlock block;
        assert(candidate.compute(MPlug(outSoftDM),block));
        block.time=1;assert(candidate.compute(MPlug(outSoftDM),block));
        candidate.m_worldInfo.reset();const int oldSteps=steps;
        if(advance)block.time=2;
        assert(!candidate.compute(MPlug(outSoftDM),block));
        assert(candidate.m_requiresStartFrame && steps==oldSteps && candidate.m_prevTime.t==1);
        block.time=0;assert(candidate.compute(MPlug(outSoftDM),block));
        assert(candidate.m_worldInfo && !candidate.m_requiresStartFrame);
    }
    std::puts("DM soft gravity changes, reuse, disabled state and missing-world-info recovery passed");
}

template<class Node>static void checkCompanionFailures(int output, std::initializer_list<int> failures) {
    inputArrayCount=1;
    for(int failure:failures) {
        resetFailures();Node node;MDataBlock data;
        if(failure==-3)node.failInit=true;
        else {
            assert(node.compute(MPlug(output),data));
            data.time=1;
            if(failure==-2)node.failUpdate=true;
            else node.failProcess=failure;
        }
        const int before=steps;
        assert(!node.compute(MPlug(output),data));
        assert(node.m_requiresStartFrame && node.m_prevTime.t==0);
        assert(steps==before);
        node.failInit=node.failUpdate=false;node.failProcess=-1;
        data.time=1;assert(!node.compute(MPlug(output),data));
        data.time=0;assert(node.compute(MPlug(output),data));
        data.time=1;assert(node.compute(MPlug(output),data));
    }
    std::puts("DM companion failures preserve time, require StartTime and recover passed");
    inputArrayCount=0;
}

template<class Node>static void checkOutputFailure(int output) {
    inputArrayCount=1;resetFailures();Node node;MDataBlock data;
    assert(node.compute(MPlug(output),data));
    node.failOutput=true;data.time=1;
    assert(!node.compute(MPlug(output),data));
    assert(node.m_requiresStartFrame && node.m_prevTime.t==0 && cleanCalls==0);
    node.failOutput=false;assert(!node.compute(MPlug(output),data));
    data.time=0;assert(node.compute(MPlug(output),data));
    data.time=1;assert(node.compute(MPlug(output),data));
    inputArrayCount=0;
    std::puts("Rigid output failures leave outputs dirty, preserve committed time and require StartTime");
}

template<class Node>static void checkNonDefaultStart(int output) {
    for(double start:{1.0,1001.0}) {
        resetFailures();Node node;MDataBlock data;
        data.values[aStartTime]=start;data.time=start;
        const int initialSteps=steps;
        assert(node.compute(MPlug(output),data));
        assert(node.m_prevTime.t==start && !node.m_requiresStartFrame && steps==initialSteps);
        data.time=start+1;
        assert(node.compute(MPlug(output),data));assert(steps==initialSteps+1);
        data.time=start;
        assert(node.compute(MPlug(output),data));assert(!node.m_requiresStartFrame && node.m_prevTime.t==start);
    }
    std::puts("Solver StartTime 1/1001 initialization, advance and restart passed");
}

int main() {
    using dommy::bullet::timelineAction;using dommy::bullet::TimelineAction;
    assert(timelineAction(true,false,99,true)==TimelineAction::Advance);
    assert(timelineAction(true,false,100,true)==TimelineAction::RestartRequired);
    assert(timelineAction(true,false,-1,true)==TimelineAction::RestartRequired);
    assert(timelineAction(false,false,0,false)==TimelineAction::RestartRequired);
    assert(timelineAction(false,true,0,false)==TimelineAction::Initialize);
    bool requiresStart=false,reported=true;
    {dommy::bullet::BulletWorldMutationGuard guard(requiresStart,reported);guard.arm();}
    assert(requiresStart && !reported);
    requiresStart=false;reported=true;
    {dommy::bullet::BulletWorldMutationGuard guard(requiresStart,reported);guard.arm();guard.commit();}
    assert(!requiresStart && reported);
    checkDependencies();
    checkSparseInputs();
    checkDataAdmission();
    checkNode<DCBullet2DSolver>(outDC);
    for(int output:{outDM,outSoftDM,outRopeDC,outContactDP})checkNode<DMBulletTest>(output);
    checkSettings<DCBullet2DSolver>(outDC,false);checkSettings<DMBulletTest>(outDM,true);
    checkNonDefaultStart<DCBullet2DSolver>(outDC);checkNonDefaultStart<DMBulletTest>(outDM);
    checkContacts();checkSettingsReader();checkSoftGravity();
    checkOutputFailure<DCBullet2DSolver>(outDC);checkOutputFailure<DMBulletTest>(outDM);
    checkCompanionFailures<DMBulletTest>(outDM, {-3,-2,DB_SLV_UPDATE_ROPE,DB_SLV_TRANS_ROPE,DB_SLV_TRANS_KINEMA,DB_SLV_UPDATE_CONST});
    checkCompanionFailures<DCBullet2DSolver>(outDC, {-3,-2,DB_SLV_2D_TRANS_KINEMA});
}
