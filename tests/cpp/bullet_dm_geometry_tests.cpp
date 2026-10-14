// Production preparation/cleanup methods with failed Maya reads and real
// Bullet shape ownership. Solver timeline/dispatch are tested separately.
#include <cassert>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "btBulletDynamicsCommon.h"
#include "BulletDynamics/ConstraintSolver/btFixedConstraint.h"
#include "db_constraint_base.h"
#include "db_collision_shape_base.h"
#include "Collision/DShapeFrameMath.h"
#include "Solver/DPassiveTransformMath.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "dm_body_mode.inc"

using DPIndexList=std::vector<std::size_t>;
static int reads=0,failedRead=0,shapes=0,failedShape=0;
static bool badInput=false,groupedInput=false;
static MStatus readStatus(){return ++reads==failedRead?MS::kFailure:MS::kSuccess;}
struct MMatrix {
    MVector position,scale{1,1,1};
    quatf rotation{1,0,0,0};
    bool finite=true;
};
namespace MSpace {enum {kTransform};}
struct MTransformationMatrix {
    MMatrix matrix;
    explicit MTransformationMatrix(const MMatrix& value):matrix(value){}
    MVector getTranslation(int,MStatus* status)const {*status=readStatus();return matrix.position;}
    MStatus getRotationQuaternion(double& x,double& y,double& z,double& w)const {
        x=matrix.rotation[1];y=matrix.rotation[2];z=matrix.rotation[3];w=matrix.rotation[0];return readStatus();
    }
    MStatus getScale(double* values,int)const {
        for(int axis=0;axis<3;++axis)values[axis]=matrix.scale[axis];return readStatus();
    }
};
struct MString {
    std::string value;
    MString(const char* text=""):value(text){}
    const char* asChar()const{return value.c_str();}
};
struct MFnDependencyNode {
    explicit MFnDependencyNode(int,MStatus* status){*status=readStatus();}
    MUuid uuid(MStatus* status)const {*status=readStatus();return {};}
    MString name(MStatus* status)const {*status=readStatus();return MString("geometry");}
};
struct MPlug {MPlug(int,int){}};
template<class T>MStatus readPlugValue(MPlug,T& value){value=T();return readStatus();}
static MStatus readPlugValue(MPlug,bool& value){value=groupedInput;return readStatus();}
static MStatus readPlugValue(MPlug,MString& value){value=MString("*");return readStatus();}
struct MDataHandle {};
namespace dommy {namespace maya {
inline bool isFiniteMatrix(const MMatrix& value){return value.finite;}
struct ScopedPlugDataHandle {
    MDataHandle value;
    MStatus read(MPlug){return readStatus();}
    MDataHandle& handle(){return value;}
};
}}
struct Primitive {MMatrix matrix;MMatrix getTransformMatrix()const{return matrix;}};
using ConstDMItem=std::shared_ptr<const Primitive>;
using ConstDCItem=ConstDMItem;
struct Group {
    DPIndexList ids;
    void getActiveIndexList(DPIndexList& output)const{output=ids;}
};
struct Geometry {
    std::vector<std::shared_ptr<Primitive>> items;
    std::map<std::string,std::shared_ptr<Group>> groups;
    bool hasValidMeshTopology()const{return true;}
    std::size_t numberOfMesh()const{return items.size();}
    std::size_t numberOfCurves()const{return items.size();}
    ConstDMItem getMeshPtrConst(std::size_t index)const{return items.at(index);}
    ConstDCItem getCurvePtr(std::size_t index)const{return items.at(index);}
    std::shared_ptr<const Group> getGrpPtr(const std::string& name)const{return groups.at(name);}
};
using DMeshData=Geometry;using DCurveData=Geometry;
static Geometry input;
static const Geometry* getInputDMfromHandle(MDataHandle&){return badInput?nullptr:&input;}
static const Geometry* getInputDCfromHandle(MDataHandle&){return badInput?nullptr:&input;}
static void getDMeshMatchedGroups(const Geometry& data,const std::string&,std::vector<std::string>& names){
    for(const auto& entry:data.groups)names.push_back(entry.first);
}
static void getDCurveMatchedGroups(const Geometry& data,const std::string& pattern,std::vector<std::string>& names){
    getDMeshMatchedGroups(data,pattern,names);
}
struct TestShape:db_collision_shape_base {
    static int live;
    TestShape(){++live;set_shape(new btBoxShape(btVector3(1,1,1)));}
    ~TestShape()override{--live;}
};
int TestShape::live=0;
static db_collision_shape_base* createCollisionShape(short,const ConstDMItem*){
    if(++shapes==failedShape)return nullptr;return new TestShape;
}
static db_collision_shape_base* create2DCollisionShape(const ConstDCItem* item){return createCollisionShape(0,item);}
static void EchoWarning(const char*,const char*){}
#include "dm_rigid_world_reference.inc"
struct DBulletRigidBodyDeleter {void operator()(btRigidBody*)const noexcept;};
using DBulletRigidBodyPtr=std::unique_ptr<btRigidBody,DBulletRigidBodyDeleter>;
#include "dm_rigid_ownership.inc"
#include "dm_geometry_release.inc"
#include "dm_geometry_helpers.inc"
struct GeometryNode {
    enum {inDM,inDC,aCollisionType,aIsCompound,aCompoundGroupName,aUseCluster,aClusterGroupName};
    DBulletWeakWorldReference m_worldReference;
    std::vector<db_collision_shape_base*> collisionShapes,compoundCollisionLists,clusteredCollisionLists;
    std::vector<btRigidBody*> rbList,compRbList,clsRbList;
    std::vector<db_constraint_base*> connectConstLists;
    std::vector<btTransform> startTransformList,compoundStartTransformList,clusteredStartTransformList;
    std::vector<MVector> startScaleList;
    std::vector<MMatrix> kinematicTransformList;
    std::vector<DPIndexList> compoundIdLists,clusteredIdLists;
    std::size_t preCollsionCount=0,m_geometryCount=0;
    bool m_geometryGrouped=false;
    std::string m_geometryGroupPattern;
    int thisMObject()const{return 1;}
    MString name()const{return MString("geometry");}
};
#define GEOMETRY_NODE(Name) struct Name:GeometryNode { \
    MStatus prepareCollisionGeometry(bool);void clearCollisionGeometry()noexcept; \
    void detachFromWorld()noexcept;~Name(){clearCollisionGeometry();} };
GEOMETRY_NODE(DMbtRigid)
struct DCbtRigid2D:GeometryNode {
    MStatus prepareCollisionGeometry(bool, DBulletBodyMode = DBulletBodyMode::Dynamic);
    void clearCollisionGeometry()noexcept;void detachFromWorld()noexcept;
    ~DCbtRigid2D(){clearCollisionGeometry();}
};
GEOMETRY_NODE(DMbtFracture)
#undef GEOMETRY_NODE
#include "dm_geometry_callbacks.inc"
static void resetInput(std::size_t count=2) {
    reads=failedRead=shapes=failedShape=0;badInput=groupedInput=false;
    input=Geometry();
    for(std::size_t i=0;i<count;++i) {
        input.items.push_back(std::make_shared<Primitive>());
        input.items.back()->matrix.position=MVector(i*4,0,0);
    }
}
template<class Node>static void checkPreparation(const char* label) {
    resetInput();Node node;
    assert(node.prepareCollisionGeometry(true));const int count=reads;
    auto* previous=node.collisionShapes[0];
    for(int failure=1;failure<=count;++failure) {
        reads=0;failedRead=failure;
        assert(!node.prepareCollisionGeometry(true));
        assert(node.collisionShapes.size()==2 && node.collisionShapes[0]==previous && TestShape::live==2);
    }
    failedRead=0;
    for(int failure:{1,2}) {
        shapes=0;failedShape=failure;assert(!node.prepareCollisionGeometry(true));
        assert(node.collisionShapes[0]==previous && TestShape::live==2);
    }
    failedShape=0;badInput=true;assert(!node.prepareCollisionGeometry(true));badInput=false;
    input.items[0]->matrix.scale=MVector(0,1,1);assert(!node.prepareCollisionGeometry(true));
    input.items[0]->matrix.scale=MVector(1,1,1);
    input.items[0]->matrix.finite=false;assert(!node.prepareCollisionGeometry(true));
    input.items[0]->matrix.finite=true;
    // Reinitialization is driven by the solver action, not frame 1. Changed
    // transforms must be captured even when the primitive count is unchanged.
    input.items[0]->matrix.position=MVector(1001,2,3);
    assert(node.prepareCollisionGeometry(true));
    assert(node.startTransformList[0].getOrigin().x()==1001);
    btDefaultCollisionConfiguration config;btCollisionDispatcher dispatcher(&config);
    btDbvtBroadphase broadphase;btSequentialImpulseConstraintSolver solver;
    btDiscreteDynamicsWorld world(&dispatcher,&broadphase,&solver,&config);
    DBulletWorldReferencePtr reference(new DBulletWorldReference(&world));
    node.m_worldReference=reference;
    std::unique_ptr<btMotionState> motion(new btDefaultMotionState(node.startTransformList[0]));
    btRigidBody::btRigidBodyConstructionInfo info(0,motion.get(),node.collisionShapes[0]->shape());
    AddBulletRigidBody(world,node.rbList,MakeBulletRigidBody(std::move(motion),info));
    input.items.clear();assert(node.prepareCollisionGeometry(false));
    assert(node.collisionShapes.empty() && node.startTransformList.empty() && node.startScaleList.empty());
    assert(node.kinematicTransformList.empty() && node.preCollsionCount==0 && node.m_geometryCount==0);
    assert(node.rbList.empty() && world.getNumCollisionObjects()==0 && TestShape::live==0);
    assert(ResolveBulletWorld(node.m_worldReference)==&world);
    resetInput();assert(node.prepareCollisionGeometry(true));
    input.items.pop_back();assert(!node.prepareCollisionGeometry(false));
    assert(node.collisionShapes.size()==2);
    node.clearCollisionGeometry();reference->invalidate();
    std::printf("%s geometry: %d failed host reads, late shape failures, reset/empty/repopulate and shrink rejection passed\n",label,count);
}
template<class Node>static void checkGroups(const char* label) {
    resetInput(3);groupedInput=true;Node node;
    input.groups["empty"]=std::make_shared<Group>();
    auto group=std::make_shared<Group>();group->ids={0,1};input.groups["pieces"]=group;
    assert(node.prepareCollisionGeometry(true));
    assert(node.collisionShapes.size()==1 && node.collisionShapes[0]->itemIndex()==2);
    assert(node.compoundCollisionLists.size()+node.clusteredCollisionLists.size()==1);
    auto* previous=node.collisionShapes[0];
    group->ids={0,5};assert(!node.prepareCollisionGeometry(true));assert(node.collisionShapes[0]==previous);
    group->ids={0,0};assert(!node.prepareCollisionGeometry(true));assert(node.collisionShapes[0]==previous);
    group->ids={0,1};input.items.clear();assert(node.prepareCollisionGeometry(false));
    assert(node.compoundCollisionLists.empty() && node.clusteredCollisionLists.empty());
    assert(node.compoundIdLists.empty() && node.clusteredIdLists.empty() && TestShape::live==0);
    std::printf("%s grouped geometry: empty group, membership, rejected invalid/duplicate IDs and empty clearing passed\n",label);
}
template<class Node>static void checkAppendAndAnimation() {
    resetInput();Node node;assert(node.prepareCollisionGeometry(true));
    auto* first=node.collisionShapes[0];
    input.items[0]->matrix.position=MVector(9,0,0);
    assert(node.prepareCollisionGeometry(false));
    assert(node.collisionShapes[0]==first && node.kinematicTransformList[0].position.x==9);
    assert(node.startTransformList[0].getOrigin().x()==0);
    input.items.push_back(std::make_shared<Primitive>());
    assert(node.prepareCollisionGeometry(false));
    assert(node.collisionShapes.size()==3 && node.collisionShapes[0]==first);
    assert(node.kinematicTransformList.size()==3);
    std::puts("Incremental geometry preserves existing bodies' shapes and refreshes unchanged-count animation");
}
int main() {
    checkPreparation<DMbtRigid>("3D");checkPreparation<DCbtRigid2D>("2D");
    checkPreparation<DMbtFracture>("Fracture");
    checkGroups<DCbtRigid2D>("2D");checkGroups<DMbtFracture>("Fracture");
    checkAppendAndAnimation<DMbtRigid>();checkAppendAndAnimation<DCbtRigid2D>();
    {
        resetInput();DCbtRigid2D node;assert(node.prepareCollisionGeometry(true));
        auto* previous=node.collisionShapes[0];
        groupedInput=true;
        for(bool rebuild:{false,true}) {
            assert(!node.prepareCollisionGeometry(rebuild,DBulletBodyMode::Kinematic));
            assert(node.collisionShapes[0]==previous && node.m_geometryCount==2);
        }
        input.items.clear();assert(node.prepareCollisionGeometry(false,DBulletBodyMode::Kinematic));
        assert(node.collisionShapes.empty());
        std::puts("2D compound kinematic preparation rejected before changing geometry; empty clearing remains valid");
    }
    assert(TestShape::live==0);
}
