// Execute complete production lifecycle callbacks with Maya/world doubles.
// Native parameter application is covered separately by bullet_soft_body_tests.
#include <cassert>
#include <cstdio>
#include <memory>
#include <set>
#include <vector>
#include "SoftBody/DSoftBodyPreparation.h"

namespace MS { enum Code { kSuccess, kFailure, kInvalidParameter }; }
struct MStatus {
    MS::Code code;
    MStatus(MS::Code c=MS::kSuccess):code(c) {}
    operator bool() const { return code==MS::kSuccess; }
    bool operator!=(MS::Code c) const { return code!=c; }
};
namespace MFn { enum Type { kMesh }; }
namespace MSpace { enum Space { kTransform }; }
namespace fixture {
double mass=0, friction=.2, margin=.1;
int clusters=1, failRead=-1, published=0, created=0;
bool failConnections=false, failOutput=false, failCreate=false, failAdd=false;
bool failPoints=false, failApply=false;
int failConnectionQuery=-1,failObjectRead=-1;
bool connectionValue=true;
std::set<const void*> world;
}
struct MObject { bool hasFn(MFn::Type) const { return true; } };
struct MPlugArray;
struct MPlug {
    int attribute;
    MPlug():attribute(-1) {} MPlug(MObject,int a):attribute(a) {}
    MStatus getValue(MObject&) const { return attribute==fixture::failObjectRead?MS::kInvalidParameter:MS::kSuccess; }
    MStatus getValue(double& value) const {
        if(attribute==fixture::failRead) return MS::kFailure;
        value=attribute==2 ? fixture::mass : attribute==3 ? fixture::friction : fixture::margin;
        return MS::kSuccess;
    }
    MStatus getValue(int& value) const {
        if(attribute==fixture::failRead) return MS::kFailure;
        value=fixture::clusters; return MS::kSuccess;
    }
    bool isConnected(MStatus* status=nullptr) const {
        if(status)*status=attribute==fixture::failConnectionQuery?MS::kInvalidParameter:MS::kSuccess;
        return attribute==fixture::failConnectionQuery?fixture::connectionValue:true;
    }
    bool connectedTo(MPlugArray&,bool,bool,MStatus* s) const {
        *s=fixture::failConnections?MS::kFailure:MS::kSuccess; return !fixture::failConnections;
    }
    MObject node() const { return {}; }
};
struct MPlugArray { unsigned length() const { return 1; } MPlug operator[](unsigned) const { return {}; } };
struct MFloatPoint {
    float x,y,z,w;
    MFloatPoint(float a=0,float b=0,float c=0,float d=1):x(a),y(b),z(c),w(d) {}
};
struct MFloatPointArray:std::vector<MFloatPoint> {
    MStatus setLength(unsigned n) { resize(n); return MS::kSuccess; }
};
struct MFnMesh {
    MFnMesh(MObject,MStatus* s) { *s=MS::kSuccess; }
    MStatus setPoints(const MFloatPointArray&,MSpace::Space) {
        return fixture::failPoints?MS::kFailure:MS::kSuccess;
    }
};
struct MFnDagNode { MFnDagNode(MObject,MStatus* s) { *s=MS::kSuccess; } };
struct Name { const char* asChar() const { return "soft"; } };
struct MDataHandle {
    int attribute;
    double asDouble() const { return attribute==2?fixture::mass:fixture::friction; }
    float asFloat() const { return static_cast<float>(fixture::margin); }
    int asInt() const { return fixture::clusters; }
    void set(bool) { ++fixture::published; } void setClean() {}
};
struct MDataBlock {
    MDataHandle inputValue(int a,MStatus* s) {
        *s=a==fixture::failRead?MS::kFailure:MS::kSuccess; return {a};
    }
    MDataHandle outputValue(int a,MStatus* s) {
        *s=fixture::failOutput?MS::kFailure:MS::kSuccess; return {a};
    }
    MStatus setClean(const MPlug&) { return MS::kSuccess; }
};
struct dbSoftBody_t {
    typedef std::shared_ptr<dbSoftBody_t> pointer;
    dommy::bullet::SoftBodyParameters parameters;
    bool candidate=true;
    bool worldAlive=true;
    bool valid() const { return worldAlive; }
    bool set_parameters(const dommy::bullet::SoftBodyParameters& p) {
        if(fixture::failApply || !dommy::bullet::validSoftBodyParameters(p)) return false;
        parameters=p; return true;
    }
};
static void configured(const dbSoftBody_t::pointer& body) {
    assert(body && body->parameters.mass==fixture::mass);
    assert(body->parameters.friction==static_cast<float>(fixture::friction));
    assert(body->parameters.collisionMargin==static_cast<float>(fixture::margin));
    assert(body->parameters.clusters==std::size_t(fixture::clusters));
}
struct dbSolver_t {
    static bool lock_solver() { return true; }
    static dbSoftBody_t::pointer create_soft_body(const std::vector<float>&,const std::vector<int>&,
        const dommy::bullet::SoftBodyParameters& p=dommy::bullet::SoftBodyParameters()) {
        if(fixture::failCreate) return {};
        dbSoftBody_t::pointer body=std::make_shared<dbSoftBody_t>();
        if(!body->set_parameters(p)) return {};
        ++fixture::created; return body;
    }
    static bool add_soft_body(const dbSoftBody_t::pointer& body,const char*) {
        if(fixture::failAdd) return false;
        // Assert before world registration, not after a parameter callback.
        if(body->candidate) configured(body);
        body->candidate=false;
        return fixture::world.insert(body.get()).second;
    }
    static void remove_soft_body(const dbSoftBody_t::pointer& body) { fixture::world.erase(body.get()); }
};
struct DBSoftBodyNode {
    enum {ca_softBody,ca_softBodyParam,ia_mass,ia_dynamicFrictionCoeff,ia_collisionMargin,ia_numClusters,inputMesh,outputMesh,typeId};
    dbSoftBody_t::pointer m_soft_body;
    MObject thisMObject() const { return {}; }
    Name name() const { return {}; }
    void destroySoftBody();
    dbSoftBody_t::pointer cached_soft_body() const { return m_soft_body; }
    static MStatus createHelperMesh(MFnMesh&,std::vector<int>& faces,std::vector<float>& xyz,MSpace::Space) {
        faces={0,1,2,0,1,3}; xyz={0,0,0,1,0,0,0,1,0,0,0,1}; return MS::kSuccess;
    }
    MStatus readSoftBodyParameters(dommy::bullet::SoftBodyParameters&,MDataBlock* = nullptr) const;
    MStatus computeSoftBody(const MPlug&,MDataBlock&);
    MStatus computeSoftBodyParam(const MPlug&,MDataBlock&);
    void set_soft_body(dbSoftBody_t::pointer);
};
static DBSoftBodyNode* node;
namespace dommy { namespace maya {
template<class T> T* checkedUserNode(const MFnDagNode&,int) { return node; }
} }
struct DBSolverNode { MStatus initSoftBody(MObject&); };
#include "bullet_soft_lifecycle.inc"

int main() {
    DBSoftBodyNode soft; node=&soft; MPlug plug; MDataBlock data; MObject object; DBSolverNode solver;
    assert(soft.computeSoftBody(plug,data)); configured(soft.m_soft_body);
    for(double mass:{0.,20.,0.,10.}) {
        fixture::mass=mass;
        assert(soft.computeSoftBodyParam(plug,data)); configured(soft.m_soft_body);
        auto old=soft.m_soft_body;
        assert(soft.computeSoftBody(plug,data)); configured(soft.m_soft_body);
        assert(soft.m_soft_body!=old && fixture::world.size()==1);
        old=soft.m_soft_body;
        // No parameter callback after StartTime: its DG token may already be clean.
        assert(solver.initSoftBody(object)); configured(soft.m_soft_body);
        assert(soft.m_soft_body!=old && fixture::world.size()==1);
    }
    for(int attribute=2;attribute<=5;++attribute) {
        fixture::failRead=attribute;
        auto old=soft.m_soft_body; const int count=fixture::created, published=fixture::published;
        assert(!soft.computeSoftBody(plug,data) && !solver.initSoftBody(object));
        assert(!soft.computeSoftBodyParam(plug,data));
        assert(soft.m_soft_body==old && fixture::world.count(old.get())==1);
        assert(fixture::created==count && fixture::published==published);
    }
    fixture::failRead=-1;
    for(int attribute:{DBSoftBodyNode::inputMesh,DBSoftBodyNode::outputMesh})for(bool value:{false,true}) {
        auto old=soft.m_soft_body;const int created=fixture::created,published=fixture::published;
        fixture::failConnectionQuery=attribute;fixture::connectionValue=value;
        assert(solver.initSoftBody(object).code==MS::kInvalidParameter);
        if(attribute==DBSoftBodyNode::inputMesh)
            assert(soft.computeSoftBody(plug,data).code==MS::kInvalidParameter);
        assert(soft.m_soft_body==old&&fixture::world.count(old.get())==1);
        assert(fixture::created==created&&fixture::published==published);
        fixture::failConnectionQuery=-1;fixture::connectionValue=true;
        assert(solver.initSoftBody(object)&&soft.computeSoftBody(plug,data));
    }
    fixture::failObjectRead=DBSoftBodyNode::inputMesh;
    assert(soft.computeSoftBody(plug,data).code==MS::kInvalidParameter);
    fixture::failObjectRead=-1;
    for(int failure=0;failure<7;++failure) {
        auto old=soft.m_soft_body; const int published=fixture::published;
        fixture::failConnections=failure==0; fixture::failCreate=failure==1;
        fixture::failAdd=failure==2; fixture::failApply=failure==3;
        if(failure==4) fixture::mass=-1;
        if(failure==5) fixture::friction=1.1;
        if(failure==6) fixture::clusters=0;
        assert(!soft.computeSoftBody(plug,data) && !solver.initSoftBody(object));
        assert(soft.m_soft_body==old && fixture::world.count(old.get())==1 && fixture::world.size()==1);
        assert(fixture::published==published);
        fixture::mass=10; fixture::friction=.2; fixture::clusters=1;
        fixture::failConnections=fixture::failCreate=fixture::failAdd=fixture::failApply=false;
    }
    auto old=soft.m_soft_body;
    fixture::mass=20; fixture::clusters=2;
    fixture::failPoints=true;
    assert(!solver.initSoftBody(object) && soft.m_soft_body==old);
    assert(fixture::world.size()==1 && fixture::world.count(old.get())==1);
    assert(old->parameters.mass==10 && old->parameters.clusters==1);
    fixture::failPoints=false; fixture::failOutput=true;
    assert(!soft.computeSoftBody(plug,data) && !soft.computeSoftBodyParam(plug,data));
    assert(soft.m_soft_body==old && fixture::world.size()==1);
    // StartTime teardown invalidates a retained cached body. A failed new
    // output must clear it, never re-register it in the replacement world.
    fixture::failOutput=false; fixture::failPoints=true;
    old->worldAlive=false; fixture::world.clear();
    assert(!solver.initSoftBody(object));
    assert(!soft.m_soft_body && fixture::world.empty());
    fixture::failPoints=false;
    assert(solver.initSoftBody(object) && soft.m_soft_body->valid());
    soft.destroySoftBody(); assert(fixture::world.empty());
    std::puts("Soft-body lifecycle: configured mesh/StartTime recreation, checked reads, failed candidates and world rollback passed");
}
