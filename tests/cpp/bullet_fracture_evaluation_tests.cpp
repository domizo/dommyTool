// Complete production compute and shared adapters with failure-injectable Maya
// doubles. This checks control flow/ownership, not Maya DG or native physics.
#include "common/utility/DStatusOperation.h"
#include "common/utility/DGenerationLimits.h"
#include "modules/dynamics/DBullet/Solver/DBulletEvaluation.h"
#include "modules/dynamics/DBullet/RigidBody/DBRigidBodyParameters.h"
#include <cassert>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// Logging is a host double here; complete initialize boundaries are exercised
// separately by plugin_initialization_fixture.cpp with the real entry guard.
namespace dommy { namespace maya {
inline MStatus reportEntryPointException(const char*, const char*, const std::exception_ptr&) {
    return MS::kFailure;
}
} }

static int calls=0, failAt=0, liveData=0, nativeMutations=0;
static bool playback=false, throwBounds=false, invalidCopy=false, nullCreate=false;
static bool wrongInputType=false, nullInput=false, wrongCppType=false;
static double secondsPerFrame=1.0/24;
static unsigned connectedCount=2;
static std::vector<std::string> trace;
static MStatus operation(const std::string& name) {
    trace.push_back(name); return ++calls==failAt ? MS::kFailure : MS::kSuccess;
}
static void resetFailures() {calls=failAt=0;trace.clear();}
struct MPxData {virtual ~MPxData() {} };
struct DMeshData : MPxData {
    static const int id=101; int value=0; bool valid=true;
    DMeshData() {++liveData;} ~DMeshData() override {--liveData;}
    MStatus copyFrom(const DMeshData& source) {
        MStatus s=operation("copy");if(!s)return s;
        value=source.value;valid=source.valid && !invalidCopy;return MS::kSuccess;
    }
    void updateBBox() {if(throwBounds)throw std::runtime_error("bounds");}
    bool hasValidMeshTopology() const {return valid;}
};
const int DMeshData::id;
using MTypeId=int;
struct MObject {
    int value;std::shared_ptr<MPxData> payload;
    MObject(int v=0):value(v) {}
    bool isNull() const {return value==0;}
    static const MObject kNullObj;
};
const MObject MObject::kNullObj;
enum {inDM=1,outDM,aEnabled,aCurrentTime,aStartTime,aGravity,aSplitImpulse,
      aIsFractureMode,aIsPlane,aPlanePosition,aPlaneAxis,aPlaneRestitution,aPlaneFriction};
static MObject mesh(int value) {
    MObject o(DMeshData::id);auto p=std::make_shared<DMeshData>();p->value=value;o.payload=p;return o;
}
static std::vector<MObject> inputObjects;
struct MPlug {
    int value,index;
    MPlug(int v=0,int i=-1):value(v),index(i) {}
    int attribute() const {return value;}
    unsigned numConnectedElements(MStatus* s) const {*s=operation("count");return connectedCount;}
    unsigned numElements(MStatus* s) const {*s=operation("count");return connectedCount+2;}
    MPlug connectionByPhysicalIndex(unsigned i,MStatus* s) const {
        *s=operation("connection");assert(i<inputObjects.size());return MPlug(value,static_cast<int>(i));
    }
    MStatus getValue(MObject& o) const {
        MStatus s=operation("readMesh");if(!s)return s;
        assert(index>=0);o=nullInput ? MObject() : inputObjects.at(index);
        if(wrongInputType)o.value=999;
        if(wrongCppType)o.payload=std::make_shared<MPxData>();
        return MS::kSuccess;
    }
};
struct MFnDependencyNode {
    MFnDependencyNode(MObject,MStatus* s) {*s=operation("node");}
    MPlug findPlug(int a,bool,MStatus* s) const {*s=operation("find");return MPlug(a);}
};
struct MFnPluginData {
    MObject object;
    MFnPluginData() {}
    MFnPluginData(MObject o,MStatus* s):object(o) {*s=operation("openData");}
    MObject create(int,MStatus* s) {*s=operation("create");object=nullCreate?MObject():mesh(0);return object;}
    MPxData* data(MStatus* s) {*s=operation("data");return object.payload.get();}
    const MPxData* constData(MStatus* s) const {*s=operation("constData");return object.payload.get();}
    int typeId(MStatus* s) const {*s=operation("type");return object.value;}
};
struct MString {const char* asChar() const {return "test";} };
struct MTime {
    enum {kSeconds};double t;
    MTime(double v=0):t(v) {} double value() const {return t;}
    double as(int) const {return t*secondsPerFrame;}
    MTime operator-(MTime r) const {return MTime(t-r.t);}
    bool operator==(MTime r) const {return t==r.t;}
};
struct MVector {double x=0,y=0,z=0;double operator[](unsigned i) const {return i==0?x:i==1?y:z;} };
struct MDataHandle {
    double value=0;std::shared_ptr<std::vector<MObject>> staged;unsigned index=0;
    bool valid=true;
    void check() const {assert(valid && "failed Maya handle consumed");}
    bool asBool() const {check();return value!=0;} double asDouble() const {check();return value;}
    MTime asTime() const {check();return MTime(value);}
    MVector asVector() const {check();MVector v;v.x=value;return v;}
    MTypeId typeId() const {check();return DMeshData::id;}
    MPxData* asPluginData() const {check();return inputObjects[0].payload.get();}
    MStatus set(MObject o) {MStatus s=operation("elementSet");if(s)staged->at(index)=o;return s;}
    void setClean() {}
};
struct MDataBlock;
struct MArrayDataBuilder {
    std::shared_ptr<std::vector<MObject>> staged;
    MArrayDataBuilder(MDataBlock*,MObject,unsigned n,MStatus* s)
        :staged(new std::vector<MObject>(n)) {*s=operation("builder");}
    MDataHandle addElement(unsigned index,MStatus* s) {
        *s=operation("addElement");MDataHandle h;h.staged=staged;h.index=index;return h;
    }
};
struct MArrayDataHandle {
    MDataBlock* block;
    MStatus set(const MArrayDataBuilder&);
    MStatus setAllClean() {return operation("allClean");}
};
struct MDataBlock {
    std::map<int,double> values{{aEnabled,1},{aCurrentTime,1},{aStartTime,1},
        {aGravity,-9.81},{aIsFractureMode,1},{aIsPlane,1},{aPlaneAxis,1}};
    std::vector<MObject> published{mesh(99)};int cleanCalls=0;
    MDataHandle inputValue(MObject a,MStatus* s) {
        *s=operation("input"+std::to_string(a.value));MDataHandle h;h.value=values[a.value];h.valid=bool(*s);return h;
    }
    MArrayDataHandle outputArrayValue(MObject,MStatus* s) {*s=operation("outputArray");return {this};}
    MDataHandle outputValue(MObject,MStatus* s) {*s=operation("outputValue");return {};}
    MStatus setClean(const MPlug&) {MStatus s=operation("clean");if(s)++cleanCalls;return s;}
};
MStatus MArrayDataHandle::set(const MArrayDataBuilder& builder) {
    MStatus s=operation("arraySet");if(s)block->published=*builder.staged;return s;
}
#include "fracture_adapters.inc"
struct MConditionMessage {
    static bool getConditionState(const char*,MStatus* s) {*s=operation("playback");return playback;}
};
struct btVector3 {float x,y,z;btVector3(float a,float b,float c):x(a),y(b),z(c) {} };
inline bool btFuzzyZero(float v) {return std::abs(v)<std::numeric_limits<float>::epsilon();}
struct World {
    struct Info {bool m_splitImpulse=false;} info;
    unsigned steps=0,glues=0,fractures=0;bool fractureMode=true;btVector3 gravity{0,0,0};
    Info& getSolverInfo() {return info;}
    void setGravity(btVector3 g) {gravity=g;}
    void glueCallback() {++glues;}
    void stepSimulation(float,int) {
        ++steps;++nativeMutations;
        if(!operation("step"))throw std::runtime_error("step");
        // Model the dispatch performed by real solveConstraints; native tests
        // separately execute that dispatch against Bullet 2.88.
        if(fractureMode)++fractures;else glueCallback();
    }
};
struct DBulletFractureSolver {
    #include "fracture_state_members.inc"
    std::unique_ptr<World> m_dynamicsWorld;
    MObject thisMObject() const {return MObject(1);} MString name() const {return {};}
    void clearBullet() {++nativeMutations;m_dynamicsWorld.reset();}
    void initBullet(MDataBlock& block) {
        if(!operation("init"))throw std::runtime_error("init");
        m_dynamicsWorld.reset(new World);m_dynamicsWorld->fractureMode=block.values[aIsFractureMode]!=0;
    }
    MStatus compute(const MPlug&,MDataBlock&);
};
static void EchoWarning(const char*,const char*) {}
static std::set<int> dependencies;
static MStatus attributeAffects(MObject input,MObject output) {
    assert(output.value==outDM);MStatus s=operation("dependency");if(s)dependencies.insert(input.value);return s;
}
#include "fracture_dependencies.inc"
#include "fracture_compute.inc"

static MStatus compute(DBulletFractureSolver& node,MDataBlock& block) {
    return node.compute(MPlug(outDM),block);
}
static void initialize(DBulletFractureSolver& node,MDataBlock& block) {
    resetFailures();block.values[aCurrentTime]=block.values[aStartTime];
    assert(compute(node,block)==MS::kSuccess);assert(!node.m_requiresStartFrame);
    block.cleanCalls=0;resetFailures();
}
static void testFailureSweep(bool advance) {
    int count;
    {DBulletFractureSolver node;MDataBlock block;
     if(advance){initialize(node,block);block.values[aCurrentTime]=2;}
     resetFailures();assert(compute(node,block)==MS::kSuccess);count=calls;}
    for(int failure=1;failure<=count;++failure) {
        DBulletFractureSolver node;MDataBlock block;
        if(advance){initialize(node,block);block.values[aCurrentTime]=2;}
        const double previous=node.m_prevTime.t;
        const int before=nativeMutations;
        const int liveBefore=liveData;
        resetFailures();failAt=failure;
        const MStatus result=compute(node,block);
        if(result==MS::kSuccess)std::fprintf(stderr,"Swallowed failure %d: %s\n",failure,trace.at(failure-1).c_str());
        assert(result!=MS::kSuccess);
        assert(block.cleanCalls==0 && node.m_prevTime.t==previous);
        // Publication may have happened before clean failed, but staging does
        // not leak retained raw pointers or leave extra copied objects alive.
        assert(liveData<=liveBefore+2);
        resetFailures();
        if(nativeMutations!=before) {
            assert(node.m_requiresStartFrame);
            const int mutations=nativeMutations;
            block.values[aCurrentTime]=2;
            assert(compute(node,block)!=MS::kSuccess);
            assert(nativeMutations==mutations);
            initialize(node,block);
        } else if(advance) {
            assert(!node.m_requiresStartFrame);
            assert(compute(node,block)==MS::kSuccess);
        }
    }
    std::printf("Fracture %s: %d failure points rejected\n",advance?"advance":"initialize",count);
}
static void testTimeline() {
    DBulletFractureSolver node;MDataBlock block;
    block.values[aCurrentTime]=2;assert(compute(node,block)!=MS::kSuccess);
    initialize(node,block);
    block.values[aCurrentTime]=2;assert(compute(node,block)==MS::kSuccess);
    assert(node.m_dynamicsWorld->steps==1 && node.m_dynamicsWorld->glues==0);
    assert(compute(node,block)==MS::kSuccess && node.m_dynamicsWorld->steps==1);
    for(double time:{1.5,10.0}) {
        block.values[aCurrentTime]=time;assert(compute(node,block)!=MS::kSuccess);
        assert(node.m_prevTime.t==2 && node.m_requiresStartFrame);
        block.values[aCurrentTime]=3;assert(compute(node,block)!=MS::kSuccess);
        initialize(node,block);block.values[aCurrentTime]=2;assert(compute(node,block)==MS::kSuccess);
    }
    playback=true;block.values[aCurrentTime]=10;assert(compute(node,block)==MS::kSuccess);
    block.values[aCurrentTime]=110;assert(compute(node,block)!=MS::kSuccess);playback=false;
    initialize(node,block);
    for(int attr:{aStartTime,aIsFractureMode,aIsPlane,aPlanePosition,aPlaneAxis,aPlaneRestitution,aPlaneFriction}) {
        const double saved=block.values[attr];
        block.values[aCurrentTime]=2;
        block.values[attr]=(attr==aIsFractureMode || attr==aIsPlane)?0:saved+1;
        if(attr==aStartTime)block.values[aCurrentTime]=3;
        assert(compute(node,block)!=MS::kSuccess && node.m_requiresStartFrame);
        block.values[attr]=saved;initialize(node,block);
    }
    block.values[aIsFractureMode]=0;initialize(node,block);
    block.values[aCurrentTime]=2;assert(compute(node,block)==MS::kSuccess);
    assert(node.m_dynamicsWorld->steps==1 && node.m_dynamicsWorld->glues==1 && node.m_dynamicsWorld->fractures==0);
    block.values[aEnabled]=0;block.values[aCurrentTime]=99;
    assert(compute(node,block)==MS::kSuccess && node.m_prevTime.t==2);
    block.values[aEnabled]=1;assert(compute(node,block)!=MS::kSuccess);
}
static void testValidationAndCopies() {
    DBulletFractureSolver node;MDataBlock block;initialize(node,block);
    block.values[aCurrentTime]=2;
    for(int attr:{aCurrentTime,aStartTime,aGravity}) {
        const double saved=block.values[attr];
        for(double bad:{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
            block.values[attr]=bad;assert(compute(node,block)!=MS::kSuccess);
            assert(node.m_dynamicsWorld->steps==0 && node.m_prevTime.t==1);
        }
        block.values[attr]=saved;
    }
    block.values[aGravity]=1e100;assert(compute(node,block)!=MS::kSuccess);block.values[aGravity]=-9.81;
    // Use a zero start so changing the unit scale cannot itself change the
    // start-time snapshot: these cases must reach duration validation.
    block.values[aStartTime]=0;initialize(node,block);block.values[aCurrentTime]=1;
    for(double bad:{-1.0,0.0,1e-100,1e-40,1e-9,1e100,std::numeric_limits<double>::infinity()}) {
        secondsPerFrame=bad;assert(compute(node,block)!=MS::kSuccess);
        assert(node.m_dynamicsWorld->steps==0);
    }
    secondsPerFrame=1.0/24;
    for(bool* flag:{&throwBounds,&invalidCopy,&nullCreate,&wrongInputType,&nullInput,&wrongCppType}) {
        *flag=true;assert(compute(node,block)!=MS::kSuccess);*flag=false;
        assert(node.m_dynamicsWorld->steps==0);
    }
    connectedCount=1000001;assert(compute(node,block)!=MS::kSuccess);connectedCount=2;
    block.values[aEnabled]=0;
    assert(node.compute(MPlug(999),block)==MS::kUnknownParameter);
    assert(node.compute(MPlug(outDM,9),block)==MS::kSuccess);
    assert(block.published.size()==2);
    for(unsigned i=0;i<2;++i) {
        const auto* copied=dynamic_cast<const DMeshData*>(block.published[i].payload.get());
        assert(copied && copied!=inputObjects[i].payload.get());
        assert(copied->value==(i==0?2:9));
    }
    connectedCount=0;assert(compute(node,block)==MS::kSuccess);assert(block.published.empty());connectedCount=2;
}
int main() {
    inputObjects={mesh(2),mesh(9)};
    testFailureSweep(false);testFailureSweep(true);testTimeline();testValidationAndCopies();
    resetFailures();assert(fractureDependencies()==MS::kSuccess);
    const std::set<int> expected{inDM,aCurrentTime,aStartTime,aEnabled,aGravity,aSplitImpulse,
        aIsFractureMode,aIsPlane,aPlanePosition,aPlaneAxis,aPlaneRestitution,aPlaneFriction};
    assert(dependencies==expected);
    for(int failure=1;failure<=12;++failure) {resetFailures();failAt=failure;assert(fractureDependencies()!=MS::kSuccess);}
    resetFailures();inputObjects.clear();assert(liveData==0);
    std::puts("Fracture adapter: timeline/restart, numeric admission, object ownership, sparse inputs, dispatch and dependencies passed");
}
