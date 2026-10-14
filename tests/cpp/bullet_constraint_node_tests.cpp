#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "mathUtils.h"
#include "common/file/DSequencePath.h"
#include "common/utility/DNumericValidation.h"
#include "Collision/DCollisionMeshInput.h"
#include "Solver/DPassiveTransformMath.h"

namespace MS { enum Code { kSuccess, kFailure, kInvalidParameter }; }
struct MStatus {
    MS::Code code;
    MStatus(MS::Code c=MS::kSuccess):code(c) {}
    explicit operator bool() const { return code==MS::kSuccess; }
};
using MObject = int;
using float3 = float[3];
namespace MSpace { enum Space { kTransform }; }
struct MEulerRotation;
struct MQuaternion {
    double x=0,y=0,z=0,w=1;
    MQuaternion()=default;
    MQuaternion(double a,double b,double c,double d):x(a),y(b),z(c),w(d) {}
    MEulerRotation asEulerRotation() const;
};
struct MEulerRotation {
    double x=0,y=0,z=0;
    MEulerRotation(double a=0,double b=0,double c=0):x(a),y(b),z(c) {}
    MQuaternion asQuaternion() const { return MQuaternion(0,0,std::sin(z/2),std::cos(z/2)); }
};
MEulerRotation MQuaternion::asEulerRotation() const { return MEulerRotation(0,0,2*std::atan2(z,w)); }
namespace fixture {
double scalar[256]; float3 vectors[256];
int failInput=-1, failOutput=-1, published=0, clean=0, parentChanges=0, commits=0;
bool failFactory=false, failCommit=false, failNative=false, failLock=false;
bool start=true, unconfigured=false, twoBodies=true;
MVector translation;
MQuaternion rotation;
void reset() {
    std::fill(scalar,scalar+256,0);
    for(auto& v:vectors) for(float& x:v) x=0;
    failInput=failOutput=-1; published=clean=parentChanges=commits=0;
    failFactory=failCommit=failNative=failLock=unconfigured=false; start=twoBodies=true;
    translation=MVector(); rotation=MQuaternion();
}
}
struct MPlug {
    MPlug()=default; MPlug(MObject,MObject) {}
    MStatus getValue(MObject&) const { return MS::kSuccess; }
};
struct MDataHandle {
    int id;
    explicit MDataHandle(int i):id(i) {}
    double asDouble() const { return fixture::scalar[id]; }
    bool asBool() const { return fixture::scalar[id]!=0; }
    float3& asFloat3() const { return fixture::vectors[id]; }
    void setBool(bool) { ++fixture::published; }
    void setClean() { ++fixture::clean; }
};
struct MDataBlock {
    MDataHandle inputValue(MObject id,MStatus* status) {
        *status=id==fixture::failInput?MS::kFailure:MS::kSuccess; return MDataHandle(id);
    }
    MDataHandle outputValue(MObject id,MStatus* status) {
        *status=id==fixture::failOutput?MS::kFailure:MS::kSuccess; return MDataHandle(id);
    }
    MStatus setClean(const MPlug&) { ++fixture::clean; return MS::kSuccess; }
};
struct MFnDagNode {
    MFnDagNode(MObject,MStatus* s) { *s=MS::kSuccess; }
    unsigned parentCount(MStatus* s) const { *s=MS::kSuccess; return 1; }
    MObject parent(unsigned,MStatus* s) const { *s=MS::kSuccess; return 0; }
};
struct MFnTransform {
    MFnTransform(MObject,MStatus* s) { *s=MS::kSuccess; }
    MStatus setScale(const double*) { ++fixture::parentChanges; return MS::kSuccess; }
    MStatus setRotation(const MEulerRotation&) { ++fixture::parentChanges; return MS::kSuccess; }
    MStatus setRotation(const MQuaternion& q) { fixture::rotation=q; return MS::kSuccess; }
    MStatus getRotation(MQuaternion& q,MSpace::Space) { q=fixture::rotation; return MS::kSuccess; }
    MVector getTranslation(MSpace::Space,MStatus* s) { *s=MS::kSuccess; return fixture::translation; }
    MStatus setTranslation(MVector p,MSpace::Space) { fixture::translation=p; return MS::kSuccess; }
};
struct FakeBody { using pointer=std::shared_ptr<FakeBody>; };
using dbRigidBody_t=FakeBody;
struct FakeConstraint {
    using pointer=std::shared_ptr<FakeConstraint>;
    vec3f pivotA=vec3f(0,0,0),pivotB=vec3f(0,0,0),world=vec3f(0,0,0);
    quatf rotationA=quatf(1,0,0,0),rotationB=quatf(1,0,0,0);
    bool changed=false, motor=false;
    int writes=0;
    float threshold=-1,damping=-1,lower=0,upper=0,velocity=0,impulse=0;
    void set_breakThreshold(float v) { threshold=v; ++writes; }
    void set_damping(float v) { damping=v; ++writes; }
    void set_limit(float lo,float hi,float,float,float) { lower=lo; upper=hi; ++writes; }
    void set_LinLimit(float lo,float hi) { lower=lo; upper=hi; ++writes; }
    void set_LinLimit(vec3f& lo,vec3f& hi) { set_LinLimit(lo[0],hi[0]); }
    void set_AngLimit(float,float) { ++writes; }
    void set_AngLimit(vec3f&,vec3f&) { ++writes; }
    void enable_motor(bool b,float v,float i) { motor=b; velocity=v; impulse=i; ++writes; }
    bool getPivotChanged() const { return changed; }
    void setPivotChanged(bool v) { changed=v; }
    bool set_world(const vec3f& p) {
        if(fixture::failNative) return false;
        world=p; pivotA=p; pivotB=p; changed=true; return true;
    }
    bool set_world(const vec3f& p,const quatf&) { return set_world(p); }
    void get_world(vec3f& p) const { p=world; }
    void get_world(vec3f& p,quatf& r) const { p=world; r=quatf(1,0,0,0); }
    void get_pivotA(vec3f& p) const { p=pivotA; }
    void get_pivotB(vec3f& p) const { p=pivotB; }
    void get_frameA(vec3f& p,quatf& r) const { p=pivotA; r=rotationA; }
    void get_frameB(vec3f& p,quatf& r) const { p=pivotB; r=rotationB; }
    void get_local_frameA(vec3f& p,quatf& r) const { get_frameA(p,r); }
    void get_local_frameB(vec3f& p,quatf& r) const { get_frameB(p,r); }
};
using dbConstraint_t=FakeConstraint;
using dbNail_constraint_t=FakeConstraint;
using dbHinge_constraint_t=FakeConstraint;
using dbSlider_constraint_t=FakeConstraint;
using dbSixdof_constraint_t=FakeConstraint;
struct dbSolver_t {
    static bool lock_solver() { return !fixture::failLock; }
    static FakeConstraint::pointer make(const vec3f& p,const quatf& r) {
        if(fixture::failFactory) return {};
        auto c=std::make_shared<FakeConstraint>(); c->pivotA=p; c->rotationA=r; return c;
    }
    static FakeConstraint::pointer create_nail_constraint(FakeBody::pointer&,const vec3f& p) {
        return make(p,quatf(1,0,0,0));
    }
    static FakeConstraint::pointer create_nail_constraint(FakeBody::pointer& a,FakeBody::pointer&,const vec3f& p,const vec3f& q) {
        auto c=create_nail_constraint(a,p); if(c)c->pivotB=q; return c;
    }
#define FRAME_FACTORY(name) \
    static FakeConstraint::pointer create_##name##_constraint(FakeBody::pointer&,const vec3f& p,const quatf& r) { return make(p,r); } \
    static FakeConstraint::pointer create_##name##_constraint(FakeBody::pointer& a,const vec3f& p,const quatf& r,FakeBody::pointer&,const vec3f& q,const quatf& s) { \
        auto c=create_##name##_constraint(a,p,r); if(c){c->pivotB=q;c->rotationB=s;} return c; }
    FRAME_FACTORY(hinge) FRAME_FACTORY(slider) FRAME_FACTORY(sixdof)
#undef FRAME_FACTORY
    static bool add_constraint(FakeConstraint::pointer& c,bool) {
        fixture::unconfigured=c->writes==0;
        if(fixture::failCommit) return false;
        ++fixture::commits; return true;
    }
    static void remove_constraint(FakeConstraint::pointer&) {}
};
struct Time { double raw=0; double value() const { return raw; } };
struct DBSolverNode {
    Time m_prevTime;
    static bool isStartFrame() { return fixture::start; }
    bool expandFileExpression(const std::string&,std::string&,std::string&);
};
namespace dommy { namespace bullet { namespace constraint_node {
MStatus connectedRigidBody(MObject,MObject,bool required,FakeBody::pointer& result) {
    result=(required || fixture::twoBodies)?std::make_shared<FakeBody>():FakeBody::pointer(); return MS::kSuccess;
}
#include "constraint_helpers.inc"
} } }
#include "constraint_declarations.inc"
#include "constraint_defaults.inc"
#include "constraint_methods.inc"
#include "constraint_writer.inc"

template<std::size_t N>
bool reaches(const std::pair<int,int> (&edges)[N],int from,int to) {
    std::set<int> seen{from}; bool changed=true;
    while(changed) { changed=false; for(const auto& edge:edges)
        if(seen.count(edge.first)) changed=seen.insert(edge.second).second || changed; }
    return seen.count(to)!=0;
}
template<class Node,std::size_t N>
void testNode(void (*defaults)(),const std::pair<int,int> (&edges)[N]) {
    MPlug plug; MDataBlock data;
    defaults(); Node node;
    assert(node.computeConstraint(plug,data));
    assert(node.m_constraint && node.m_constraint->threshold==7 && !fixture::unconfigured);
    assert(reaches(edges,Node::ia_disableCollide,Node::ca_constraintParam));
    assert(reaches(edges,Node::ia_pivotInA,Node::ca_constraint));
    auto previous=node.m_constraint;
    fixture::vectors[Node::ia_pivotInA][0]=4;
    if(reaches(edges,Node::ia_pivotInA,Node::ca_constraint)) assert(node.computeConstraint(plug,data));
    assert(node.m_constraint!=previous && node.m_constraint->pivotA[0]==4 && node.m_constraint->threshold==7);
    previous=node.m_constraint;
    fixture::vectors[Node::ia_pivotInB][0]=5;
    assert(reaches(edges,Node::ia_pivotInB,Node::ca_constraint));
    if(reaches(edges,Node::ia_pivotInB,Node::ca_constraint)) assert(node.computeConstraint(plug,data));
    assert(node.m_constraint!=previous && node.m_constraint->pivotB[0]==5);
    fixture::scalar[Node::ia_disableCollide]=0;
    if(reaches(edges,Node::ia_disableCollide,Node::ca_constraint)) assert(node.computeConstraint(plug,data));
    assert(!fixture::unconfigured && node.m_constraint->threshold==7);
    for(int failure=0;failure<5;++failure) {
        defaults(); previous=node.m_constraint; const int published=fixture::published;
        if(failure==0) fixture::scalar[Node::ia_breakThreshold]=-1;
        if(failure==1) fixture::failOutput=Node::ca_constraint;
        if(failure==2) fixture::failFactory=true;
        if(failure==3) fixture::failCommit=true;
        if(failure==4) fixture::failInput=Node::ia_breakThreshold;
        assert(!node.computeConstraint(plug,data));
        assert(node.m_constraint==previous && fixture::published==published);
    }
    defaults(); assert(node.computeConstraint(plug,data));
    const int writes=node.m_constraint->writes;
    fixture::failOutput=Node::ca_constraintParam;
    assert(!node.computeConstraintParam(plug,data) && node.m_constraint->writes==writes);
    fixture::failOutput=-1; fixture::scalar[Node::ia_breakThreshold]=11;
    assert(node.computeConstraintParam(plug,data) && node.m_constraint->threshold==11);
    fixture::failLock=true;
    assert(!node.computeConstraintParam(plug,data)); fixture::failLock=false;
    fixture::translation=MVector(1e100,0,0); const int parent=fixture::parentChanges;
    assert(!node.computeWorldMatrix(plug,data));
    assert(node.m_constraint->world[0]==0 && fixture::parentChanges==parent);
    fixture::translation=MVector(3,2,1); fixture::failNative=true;
    assert(!node.computeWorldMatrix(plug,data) && node.m_constraint->world[0]==0);
    fixture::failNative=false;
    assert(node.computeWorldMatrix(plug,data) && node.m_constraint->world[0]==3);
    defaults(); fixture::twoBodies=false;
    assert(node.computeConstraint(plug,data) && !fixture::unconfigured);
}
template<class Node,std::size_t N>
void testRotation(void (*defaults)(),const std::pair<int,int> (&edges)[N]) {
    defaults(); Node node; MPlug plug; MDataBlock data;
    assert(node.computeConstraint(plug,data));
    const int attributes[]={Node::ia_rotationInA,Node::ia_rotationInB};
    for(int attribute:attributes) {
        auto before=node.m_constraint;
        fixture::vectors[attribute][2]=60;
        assert(reaches(edges,attribute,Node::ca_constraint));
        assert(node.computeConstraint(plug,data) && node.m_constraint!=before);
        const quatf& q=attribute==Node::ia_rotationInA?node.m_constraint->rotationA:node.m_constraint->rotationB;
        assert(std::abs(q[3]-.5f)<1e-5f && node.m_constraint->threshold==7);
    }
}
int main() {
    testNode<DBNailConstraintNode>(defaultsNail,edgesNail);
    testNode<DBHingeConstraintNode>(defaultsHinge,edgesHinge);
    testNode<DBSliderConstraintNode>(defaultsSlider,edgesSlider);
    testNode<DBSixdofConstraintNode>(defaultsSixdof,edgesSixdof);
    testRotation<DBHingeConstraintNode>(defaultsHinge,edgesHinge);
    testRotation<DBSliderConstraintNode>(defaultsSlider,edgesSlider);
    testRotation<DBSixdofConstraintNode>(defaultsSixdof,edgesSixdof);
    DBSolverNode solver; std::string base="old",extension="old";
    for(const auto& test:std::vector<std::pair<double,std::string>>{
            {12.75,"body.12.0012"},{-3.75,"body.-3.00-3"},
            {2147483648.0,"body.2147483648.2147483648"},
            {-9223372036854775808.0,"body.-9223372036854775808.-9223372036854775808"}}) {
        solver.m_prevTime.raw=test.first;
        assert(solver.expandFileExpression("body.${frame}.${waveFrame}.pdb",base,extension));
        assert(base==test.second && extension=="pdb");
    }
    for(double invalid:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),
                        9223372036854775808.0,-1e30}) {
        solver.m_prevTime.raw=invalid; const std::string oldBase=base,oldExtension=extension;
        assert(!solver.expandFileExpression("body.${frame}.pdb",base,extension));
        assert(base==oldBase && extension==oldExtension);
    }
    std::puts("Four constraint nodes: 16 full callbacks/helpers, dependency edges, configured replacement/failure preservation, checked world updates and frame filenames passed");
}
