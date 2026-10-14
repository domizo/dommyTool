// Production soft callbacks and solver dispatch with failure-injectable host
// doubles. Native construction is covered by the separate Bullet fixtures.
#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "SoftBody/DSoftBodyPreparation.h"
#include "dm_body_mode.inc"
#include "dm_soft_topology.inc"

#define DM_BULLET_SOFT_DEBUG 0
#define DM_BULLET_2D_DEBUG 0
#define DOMMY_MAYA_DEBUG(...) ((void)0)
static int hostCalls=0,hostFail=0,parameterCalls=0,parameterFail=0;
static bool failRead=false,invalidInput=false;
static MStatus hostStatus() {return ++hostCalls==hostFail?MS::kFailure:MS::kSuccess;}
using MObject=int;using MTypeId=int;
struct Node {virtual ~Node()=default;};
static std::map<int,Node*> nodes;
struct MString {const char* asChar() const {return "test";}};
static void EchoWarning(const char*,const char*) {}
struct MPlug;
struct MPlugArray : std::vector<MPlug> {unsigned length() const {return size();}};
struct MPlug {
    int attribute;
    MPlug(int,int value):attribute(value) {}
    unsigned numConnectedElements(MStatus* status) const {*status=hostStatus();return nodes.count(attribute)?1:0;}
    MPlug connectionByPhysicalIndex(unsigned,MStatus* status) const {*status=hostStatus();return *this;}
    bool connectedTo(MPlugArray& output,bool asDestination,bool asSource,MStatus* status) const {
        assert(asDestination && !asSource);*status=hostStatus();output.push_back(*this);return true;
    }
    int node(MStatus* status) const {*status=hostStatus();return attribute;}
};
struct MFnDependencyNode {
    int object;
    explicit MFnDependencyNode(int value,MStatus* status=nullptr):object(value) {if(status)*status=hostStatus();}
    int typeId(MStatus* status) const {*status=hostStatus();return object;}
    int uuid() const {return object;}
    MString name() const {return {};}
};
struct MDataHandle {};
namespace dommy {namespace maya {
struct ScopedPlugDataHandle {
    MDataHandle value;
    MDataHandle& handle() {return value;}
    MStatus read(MPlug) {return failRead?MS::kFailure:MS::kSuccess;}
};
template<class T>T* checkedUserNode(const MFnDependencyNode& fn,int) {return dynamic_cast<T*>(nodes[fn.object]);}
}}
template<class T>MStatus readPlugValue(MPlug,T&) {
    return ++parameterCalls==parameterFail?MS::kFailure:MS::kSuccess;
}
struct btSoftRigidDynamicsWorld {int removed=0;void removeSoftBody(int*) {++removed;}};
struct btSoftBodyWorldInfo {};
struct DBulletWorldReference {
    btSoftRigidDynamicsWorld* pointer;
    explicit DBulletWorldReference(btSoftRigidDynamicsWorld* value):pointer(value) {}
    btSoftRigidDynamicsWorld* get() const {return pointer;}
};
using DBulletWorldReferencePtr=std::shared_ptr<DBulletWorldReference>;
static btSoftRigidDynamicsWorld* ResolveBulletWorld(const DBulletWorldReferencePtr& value) {return value?value->get():nullptr;}
struct customDBData {int nodeUuid;std::size_t index;std::string prefixName;};
struct db_softbody_base {
    static int live;
    int native=0;
    std::unique_ptr<customDBData> user;
    db_softbody_base() {++live;}
    ~db_softbody_base() {--live;}
    int* body() {return &native;}
    void set_user_data(customDBData* value) {user.reset(value);}
};
int db_softbody_base::live=0;
#include "dm_soft_params.inc"
struct MIntArray : std::vector<int> {using std::vector<int>::vector;unsigned length()const{return size();}};
static bool topologyFailure=false;
struct Primitive {
    std::size_t points=3;
    MIntArray faces{3},connections{0,1,2};
    std::size_t numberOfPoints()const{return points;}
    int numFace()const{return faces.size();}
    std::size_t numberOfConnections()const{return connections.size();}
    bool hasValidPolygonTopology()const{return points>=3;}
    MStatus copyTopology(MIntArray& f,MIntArray& c)const {
        if(topologyFailure)return MS::kFailure;f=faces;c=connections;return MS::kSuccess;
    }
};
using ConstDMItem=const Primitive*;
struct DMeshData {
    Primitive primitive;std::vector<const Primitive*> meshes{&primitive};
    std::size_t numberOfMesh() const {return meshes.size();}
    std::vector<const Primitive*>::const_iterator beginPrims() const {return meshes.begin();}
    std::vector<const Primitive*>::const_iterator endPrims() const {return meshes.end();}
    const Primitive* getMeshPtrConst(std::size_t i) const {return meshes[i];}
};
static DMeshData input;
static const DMeshData* getInputDMfromHandle(MDataHandle&) {return invalidInput?nullptr:&input;}
static std::unique_ptr<db_softbody_base> createSoftBodyShape(btSoftBodyWorldInfo*,const btSoftParams*,const Primitive&) {
    return std::unique_ptr<db_softbody_base>(new db_softbody_base);
}
static void addSoftBody(btSoftRigidDynamicsWorld&,std::vector<db_softbody_base*>& output,std::unique_ptr<db_softbody_base> body) {
    output.push_back(body.release());
}
struct DMbtSoft : Node {
    enum {typeId=3,inDM=20};
#include "dm_soft_attributes.inc"
    DBulletWorldReferencePtr m_worldReference;
    std::vector<db_softbody_base*> softBodyShapes;
    std::size_t preSoftBodyCount=0;
    std::size_t m_nativeWork=0;
    std::vector<DBulletSoftTopology> m_inputTopology;
    int thisMObject() {return typeId;}
    MString name()const{return {};}
    MStatus evalParms(btSoftParams&);
    MStatus resetSoftBodies(btSoftRigidDynamicsWorld*,btSoftBodyWorldInfo*,const DBulletWorldReferencePtr&);
    MStatus updateSoftBodies(btSoftRigidDynamicsWorld*,btSoftBodyWorldInfo*);
    void releaseSoftBodies() noexcept;
    ~DMbtSoft() override {releaseSoftBodies();}
};
#include "dm_soft_callbacks.inc"

struct DMbtRigid : Node {
    enum {typeId=1};
    int failure=-1;
    bool rebuilt=false;
    DBulletBodyMode preparedMode=DBulletBodyMode::Dynamic,updatedMode=DBulletBodyMode::Dynamic;
    MStatus prepareCollisionGeometry(bool rebuild,DBulletBodyMode mode=DBulletBodyMode::Dynamic) {rebuilt=rebuild;preparedMode=mode;return failure==5?MS::kFailure:MS::kSuccess;}
    MStatus resetRigidBodies(btSoftRigidDynamicsWorld*,DBulletWorldReferencePtr) {return failure==0?MS::kFailure:MS::kSuccess;}
    MStatus resetRigidBodiesKinematic(btSoftRigidDynamicsWorld*,DBulletWorldReferencePtr) {return failure==1?MS::kFailure:MS::kSuccess;}
    MStatus updateRigidBodies(btSoftRigidDynamicsWorld*,DBulletBodyMode mode=DBulletBodyMode::Dynamic) {updatedMode=mode;return failure==2?MS::kFailure:MS::kSuccess;}
    MStatus updateCollisionShape(btSoftRigidDynamicsWorld*) {return failure==3?MS::kFailure:MS::kSuccess;}
    MStatus updateRigidBodiesKinematic(btSoftRigidDynamicsWorld*,float) {return failure==4?MS::kFailure:MS::kSuccess;}
    void detachFromWorld() {}
};
struct DMbtFracture : DMbtRigid {enum {typeId=2};};
struct DCbtRigid2D : DMbtRigid {enum {typeId=7};};
struct DCbtRope : Node {
    enum {typeId=4};int failure=-1,detached=0;
    MStatus resetRopes(btSoftRigidDynamicsWorld*,btSoftBodyWorldInfo*,DBulletWorldReferencePtr) {return failure==0?MS::kFailure:MS::kSuccess;}
    MStatus updateRopes(btSoftRigidDynamicsWorld*,btSoftBodyWorldInfo*) {return failure==1?MS::kFailure:MS::kSuccess;}
    MStatus updateLockPosition(btSoftRigidDynamicsWorld*,btSoftBodyWorldInfo*,float) {return failure==2?MS::kFailure:MS::kSuccess;}
    void detachFromWorld() {++detached;}
};
struct DBPointConstraint : Node {
    enum {typeId=5};
    int failure=-1;
    MStatus createConstraint(btSoftRigidDynamicsWorld*,DBulletWorldReferencePtr) {return failure==0?MS::kFailure:MS::kSuccess;}
    MStatus updateConstraint(btSoftRigidDynamicsWorld*,float) {return failure==1?MS::kFailure:MS::kSuccess;}
    void detachFromWorld() {}
};
struct DBLinkConstraint : DBPointConstraint {enum {typeId=6};};
enum {DB_SLV_INIT_RB,DB_SLV_INIT_KINEMA,DB_SLV_INIT_SB,DB_SLV_INIT_ROPE,DB_SLV_INIT_CONST,
      DB_SLV_UPDATE_RB,DB_SLV_UPDATE_KINEMA,DB_SLV_UPDATE_SHAPE_RB,DB_SLV_UPDATE_SB,
      DB_SLV_UPDATE_ROPE,DB_SLV_TRANS_KINEMA,DB_SLV_TRANS_ROPE,DB_SLV_UPDATE_CONST,
      DB_SLV_DETACH_WORLD_RESOURCES};
struct DBtSolverOptionParams {int m_solver_proc_type=0;float m_timestep=1;};
struct DMBulletTest {
    enum {inDM=1,inKinematicDMs=2,inSoftDM=3,inRopeDC=4,inConst=5};
    std::unique_ptr<btSoftRigidDynamicsWorld> m_dynamicsWorld{new btSoftRigidDynamicsWorld};
    std::unique_ptr<btSoftBodyWorldInfo> m_worldInfo{new btSoftBodyWorldInfo};
    DBulletWorldReferencePtr m_worldReference{new DBulletWorldReference(m_dynamicsWorld.get())};
    int thisMObject() {return 0;}
    MStatus solverProcessFromPlug(MObject,DBtSolverOptionParams);
    MStatus initRigidBodies();MStatus checkRigidBodies();
};
enum {DB_SLV_2D_INIT_RB,DB_SLV_2D_INIT_KINEMA,DB_SLV_2D_INIT_CONST,
      DB_SLV_2D_UPDATE_RB,DB_SLV_2D_UPDATE_SHAPE_RB,DB_SLV_2D_UPDATE_KINEMA,
      DB_SLV_2D_UPDATE_CONST,DB_SLV_2D_TRANS_KINEMA,DB_SLV_2D_DETACH_WORLD_RESOURCES};
using DBt2DSolverOptionParams=DBtSolverOptionParams;
struct DCBullet2DSolver {
    enum {inDC=7,inKinematicDCs=8,inConst=5};
    std::unique_ptr<btSoftRigidDynamicsWorld> m_dynamicsWorld{new btSoftRigidDynamicsWorld};
    DBulletWorldReferencePtr m_worldReference{new DBulletWorldReference(m_dynamicsWorld.get())};
    int thisMObject() {return 0;}
    MStatus solverProcessFromPlug(MObject,DBt2DSolverOptionParams);
    MStatus initRigidBodies();MStatus checkRigidBodies();
};
#include "dm_dispatch.inc"

template<class Solver,class Rigid>static void checkRigidDispatch(int attribute,
    std::initializer_list<int> operations) {
    Solver solver;Rigid rigid;nodes={{Rigid::typeId,&rigid}};
    int failure=0;DBtSolverOptionParams option;
    for(int operation:operations) {
        rigid.failure=failure++;option.m_solver_proc_type=operation;
        assert(!solver.solverProcessFromPlug(attribute,option));
        rigid.failure=-1;assert(solver.solverProcessFromPlug(attribute,option));
    }
    rigid.failure=0;assert(!solver.initRigidBodies());
    rigid.failure=2;assert(!solver.checkRigidBodies());
    rigid.failure=3;assert(!solver.checkRigidBodies());
    rigid.failure=5;assert(!solver.initRigidBodies());assert(!solver.checkRigidBodies());
    rigid.failure=-1;assert(solver.initRigidBodies());assert(rigid.rebuilt);
    assert(solver.checkRigidBodies());assert(!rigid.rebuilt);
    rigid.failure=-1;hostCalls=0;assert(solver.solverProcessFromPlug(attribute,option));
    const int count=hostCalls;
    for(int failed=1;failed<=count;++failed) {
        hostCalls=0;hostFail=failed;assert(!solver.solverProcessFromPlug(attribute,option));
    }
    hostFail=0;nodes[Rigid::typeId]=nullptr;
    assert(!solver.solverProcessFromPlug(attribute,option));
    nodes.clear();
    std::puts("Rigid dispatch: all five child failures and graph failures propagated");
}

static void checkSoftCallbacks() {
    btSoftRigidDynamicsWorld world;btSoftBodyWorldInfo info;
    DBulletWorldReferencePtr reference(new DBulletWorldReference(&world));
    btSoftParams params;DMbtSoft node;
    parameterCalls=0;assert(node.evalParms(params));const int parameterCount=parameterCalls;
    for(bool update:{false,true})for(int failure=0;failure<parameterCount+3;++failure) {
        node.releaseSoftBodies();input.meshes={&input.primitive};
        failRead=invalidInput=false;parameterFail=0;
        assert(node.resetSoftBodies(&world,&info,reference));
        auto* previous=node.softBodyShapes[0];const int removed=world.removed;
        if(update)input.meshes.push_back(&input.primitive);
        failRead=failure==0;invalidInput=failure==1;
        parameterCalls=0;parameterFail=failure>=3?failure-2:0;
        // Invalid native-world descriptors also fail without destroying old data.
        auto* descriptor=failure==2?nullptr:&info;
        const MStatus status=update?node.updateSoftBodies(&world,descriptor):node.resetSoftBodies(&world,descriptor,reference);
        assert(!status && node.softBodyShapes.size()==1 && node.softBodyShapes[0]==previous);
        assert(node.preSoftBodyCount==1 && world.removed==removed && db_softbody_base::live==1);
    }
    failRead=invalidInput=false;parameterFail=0;
    assert(node.updateSoftBodies(&world,&info));assert(node.softBodyShapes.size()==2);
    input.meshes.clear();assert(node.resetSoftBodies(&world,&info,reference));
    assert(node.softBodyShapes.empty() && node.preSoftBodyCount==0 && db_softbody_base::live==0);
    input.meshes={&input.primitive};assert(node.resetSoftBodies(&world,&info,reference));
    std::printf("DM soft reset/update: %d scalar read failures, input/type/world failures and empty/reset recovery passed\n",parameterCount);
}

static void checkSoftAdmission() {
    btSoftParams params;
    assert(areValidSoftBodyParameters(params));
    for (double value : {1e100, -1e100, 1e-100,
            std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        params.m_Pressure=value;assert(!areValidSoftBodyParameters(params));
    }
    params=btSoftParams();params.m_Pressure=-2;assert(areValidSoftBodyParameters(params));
    for (int btSoftParams::* field : {&btSoftParams::m_positionIterations,
            &btSoftParams::m_velocityIterations, &btSoftParams::m_driftIterations, &btSoftParams::m_clusterIterations}) {
        params=btSoftParams();params.*field=1024;assert(areValidSoftBodyParameters(params));
        for (int invalid : {-1,1025,std::numeric_limits<int>::max()}) {
            params.*field=invalid;assert(!areValidSoftBodyParameters(params));
        }
    }
    for (double btSoftParams::* field : {&btSoftParams::m_stiffness, &btSoftParams::m_Damping,
            &btSoftParams::m_rigid_impulse_split, &btSoftParams::m_RigidContactHardness}) {
        params=btSoftParams();params.*field=1.01;assert(!areValidSoftBodyParameters(params));
        params.*field=-0.01;assert(!areValidSoftBodyParameters(params));
    }
    params=btSoftParams();params.m_rest_length_scale=1e-100;assert(!areValidSoftBodyParameters(params));
    for (float margin:{-1.f,1e30f,std::numeric_limits<float>::max()}) {
        params=btSoftParams();params.m_collisionMargin=margin;assert(!areValidSoftBodyParameters(params));
    }
    params=btSoftParams();
    DBulletSoftTopology topology;topology.points=3;topology.faces={3};topology.connections={0,1,2};
    std::size_t work=0;
    assert(softBodyWorkBudget({topology},0,params,work) && work>0);
    const std::size_t one=work;
    assert(softBodyWorkBudget({topology,topology},1,params,work) && work==2*one);
    work=dommy::bullet::kMaxSoftBodyWork;
    assert(!softBodyWorkBudget({topology},0,params,work) && work==dommy::bullet::kMaxSoftBodyWork);
    topology.points=65536;work=7;
    assert(!softBodyWorkBudget({topology},0,params,work) && work==7);
    topology.points=500;params.m_is_bend_constraint=true;params.m_bendConstraintDistance=3;
    assert(!softBodyWorkBudget({topology},0,params,work)); // Cubic native search.
    topology.points=4;params.m_is_cluster=true;params.m_numClusters=8193;
    assert(!softBodyWorkBudget({topology},0,params,work));
    params.m_numClusters=2;params.m_bendConstraintDistance=2;work=0;
    assert(softBodyWorkBudget({topology},0,params,work));

    btSoftRigidDynamicsWorld world;btSoftBodyWorldInfo info;
    DBulletWorldReferencePtr reference(new DBulletWorldReference(&world));
    DMbtSoft node;input.primitive=Primitive();input.meshes={&input.primitive};
    assert(node.resetSoftBodies(&world,&info,reference));auto* previous=node.softBodyShapes[0];
    input.primitive.points=65536;
    assert(!node.resetSoftBodies(&world,&info,reference) && node.softBodyShapes[0]==previous);
    input.primitive=Primitive();input.meshes.push_back(&input.primitive);
    node.m_nativeWork=dommy::bullet::kMaxSoftBodyWork;
    assert(!node.updateSoftBodies(&world,&info) && node.softBodyShapes.size()==1);
    assert(node.softBodyShapes[0]==previous && node.preSoftBodyCount==1);
    input.meshes.clear();assert(node.updateSoftBodies(&world,&info) && node.m_nativeWork==0);
    input.meshes={&input.primitive};
    std::puts("DM soft float conversion, coefficient/iteration bounds and native work admission passed");
}

static void checkSoftTopology() {
    btSoftRigidDynamicsWorld world;btSoftBodyWorldInfo info;
    DBulletWorldReferencePtr reference(new DBulletWorldReference(&world));
    DMbtSoft node;
    input.primitive=Primitive();input.meshes={&input.primitive};
    assert(node.resetSoftBodies(&world,&info,reference));
    auto* previous=node.softBodyShapes[0];
    for(int change=0;change<5;++change) {
        if(change==0)topologyFailure=true;
        if(change==1)input.primitive.points=4;
        if(change==2)input.primitive.connections={0,2,1};
        if(change==3)input.meshes[0]=nullptr;
        if(change==4)input.primitive.points=dommy::generation::kMaxGeneratedPointCount+1;
        assert(!node.updateSoftBodies(&world,&info));
        assert(node.softBodyShapes[0]==previous && world.removed==0);
        assert(!node.resetSoftBodies(&world,&info,reference) || (change==1 || change==2));
        // Valid changed topology is accepted only by an explicit reset.
        topologyFailure=false;input.primitive=Primitive();input.meshes={&input.primitive};
        assert(node.resetSoftBodies(&world,&info,reference));
        previous=node.softBodyShapes[0];world.removed=0;
    }
    input.meshes.push_back(&input.primitive);
    assert(node.updateSoftBodies(&world,&info));assert(node.softBodyShapes.size()==2);
    input.meshes.pop_back();assert(!node.updateSoftBodies(&world,&info));
    assert(node.softBodyShapes.size()==2);
    input.meshes.clear();assert(node.updateSoftBodies(&world,&info));
    assert(node.softBodyShapes.empty() && node.m_inputTopology.empty() && node.preSoftBodyCount==0);
    assert(world.removed==2 && node.m_worldReference==reference);
    assert(node.updateSoftBodies(&world,&info));assert(world.removed==2);
    input.meshes={&input.primitive};assert(node.updateSoftBodies(&world,&info));
    assert(node.softBodyShapes.size()==1 && node.preSoftBodyCount==1);
    std::puts("Soft topology: failed reads, null/bounded inputs, topology/shrink rejection, empty/repeated-empty/repopulate passed");
}

static void checkMotionDispatch() {
    DMBulletTest solver;DMbtRigid rigid;nodes={{DMbtRigid::typeId,&rigid}};
    DBtSolverOptionParams option;option.m_solver_proc_type=DB_SLV_UPDATE_KINEMA;
    assert(solver.solverProcessFromPlug(DMbtRigid::typeId,option));
    assert(rigid.updatedMode==DBulletBodyMode::Kinematic);
    DCBullet2DSolver solver2D;DCbtRigid2D rigid2D;nodes={{DCbtRigid2D::typeId,&rigid2D}};
    option.m_solver_proc_type=DB_SLV_2D_UPDATE_KINEMA;
    assert(solver2D.solverProcessFromPlug(DCbtRigid2D::typeId,option));
    assert(rigid2D.preparedMode==DBulletBodyMode::Kinematic && rigid2D.updatedMode==DBulletBodyMode::Kinematic);
    option.m_solver_proc_type=DB_SLV_2D_INIT_KINEMA;
    assert(solver2D.solverProcessFromPlug(DCbtRigid2D::typeId,option));
    assert(rigid2D.preparedMode==DBulletBodyMode::Kinematic);
    nodes.clear();
    std::puts("3D/2D kinematic motion mode reaches preparation and addition callbacks");
}

static void checkDispatch() {
    DMBulletTest solver;DMbtSoft soft;DCbtRope rope;
    nodes={{DMbtSoft::typeId,&soft},{DCbtRope::typeId,&rope}};
    for(int failure=0;failure<2;++failure) {
        failRead=failure==0;parameterFail=failure==1?1:0;parameterCalls=0;
        assert(!solver.initRigidBodies());
    }
    failRead=false;parameterFail=0;assert(solver.initRigidBodies());
    failRead=true;assert(!solver.checkRigidBodies());failRead=false;
    for(int failure=0;failure<3;++failure) {
        rope.failure=failure;
        DBtSolverOptionParams option;
        option.m_solver_proc_type=failure==0?DB_SLV_INIT_ROPE:failure==1?DB_SLV_UPDATE_ROPE:DB_SLV_TRANS_ROPE;
        assert(!solver.solverProcessFromPlug(DMBulletTest::inRopeDC,option));
    }
    rope.failure=-1;DBtSolverOptionParams option;option.m_solver_proc_type=DB_SLV_UPDATE_ROPE;
    hostCalls=0;assert(solver.solverProcessFromPlug(DMBulletTest::inRopeDC,option));const int count=hostCalls;
    for(int failure=1;failure<=count;++failure) {
        hostCalls=0;hostFail=failure;assert(!solver.solverProcessFromPlug(DMBulletTest::inRopeDC,option));
    }
    hostFail=0;nodes[DCbtRope::typeId]=nullptr;
    assert(!solver.solverProcessFromPlug(DMBulletTest::inRopeDC,option));
    option.m_solver_proc_type=DB_SLV_DETACH_WORLD_RESOURCES;
    assert(solver.solverProcessFromPlug(DMBulletTest::inRopeDC,option));
    nodes[DCbtRope::typeId]=&rope;assert(solver.solverProcessFromPlug(DMBulletTest::inRopeDC,option));assert(rope.detached==1);
    nodes.clear();
    std::puts("DM actual dispatch/init/update propagate child and graph failures; teardown remains best effort");
}

static void checkLegacyDispatch() {
    DMBulletTest solver;DMbtFracture fracture;DBPointConstraint point;DBLinkConstraint link;
    DBtSolverOptionParams option;
    nodes={{DMbtFracture::typeId,&fracture}};
    for(int failure:{0,2,5}) {
        fracture.failure=failure;
        option.m_solver_proc_type=failure==0?DB_SLV_INIT_RB:DB_SLV_UPDATE_RB;
        assert(!solver.solverProcessFromPlug(DMbtFracture::typeId,option));
        fracture.failure=-1;
        assert(solver.solverProcessFromPlug(DMbtFracture::typeId,option));
    }
    for(int type:{int(DBPointConstraint::typeId),int(DBLinkConstraint::typeId)}) {
        DBPointConstraint* node=type==DBPointConstraint::typeId?&point:&link;
        nodes={{type,node}};
        for(int failure:{0,1}) {
            node->failure=failure;
            option.m_solver_proc_type=failure==0?DB_SLV_INIT_CONST:DB_SLV_UPDATE_CONST;
            assert(!solver.solverProcessFromPlug(type,option));
            node->failure=-1;assert(solver.solverProcessFromPlug(type,option));
        }
    }
    nodes.clear();
    std::puts("Fracture and legacy constraint dispatch: all six child failures propagated");
}

int main() {
    checkSoftCallbacks();checkSoftTopology();checkSoftAdmission();checkDispatch();checkMotionDispatch();
    checkLegacyDispatch();
    checkRigidDispatch<DMBulletTest,DMbtRigid>(DMBulletTest::inDM,
        {DB_SLV_INIT_RB,DB_SLV_INIT_KINEMA,DB_SLV_UPDATE_RB,DB_SLV_UPDATE_SHAPE_RB,DB_SLV_TRANS_KINEMA});
    checkRigidDispatch<DCBullet2DSolver,DCbtRigid2D>(DCBullet2DSolver::inDC,
        {DB_SLV_2D_INIT_RB,DB_SLV_2D_INIT_KINEMA,DB_SLV_2D_UPDATE_RB,DB_SLV_2D_UPDATE_SHAPE_RB,DB_SLV_2D_TRANS_KINEMA});
    assert(db_softbody_base::live==0);
}
