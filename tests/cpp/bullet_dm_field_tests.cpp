// Production field helper + real Bullet bodies. Maya fields/arrays are doubles.
#include "Solver/DBulletDynamicsWorld.h"
#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h"
#include "common/utility/DParticleFieldMath.h"
#include "common/utility/DStatusOperation.h"
#include <cassert>
#include <cstdio>
#include <limits>

static int calls=0,failAt=0;
static MStatus operation() {return ++calls==failAt ? MS::kFailure : MS::kSuccess;}
struct MObject {std::size_t index;};
struct MVector {
    double x,y,z;
    MVector(double a=0,double b=0,double c=0):x(a),y(b),z(c){}
    double& operator[](unsigned i){return i==0?x:(i==1?y:z);}
    double operator[](unsigned i)const{return i==0?x:(i==1?y:z);}
    MVector operator/(double n)const{return MVector(x/n,y/n,z/n);}
};
template<class T>struct Array : std::vector<T> {
    MStatus append(const T& t){MStatus s=operation();if(s)this->push_back(t);return s;}
    MStatus setLength(unsigned n){MStatus s=operation();if(s)this->resize(n);return s;}
    unsigned length()const{return this->size();}
};
using MVectorArray=Array<MVector>;using MDoubleArray=Array<double>;
struct Field {MVector value;bool succeeds;bool wrongCount;};
static std::vector<Field> fields;
namespace MFn {enum {kField};}
struct MItDag {
    enum {kDepthFirst};std::size_t index=0;
    MItDag(int,int,MStatus* s){*s=operation();}
    bool isDone(MStatus* s)const{*s=operation();return index==fields.size();}
    MObject currentItem(MStatus* s){*s=operation();return {index};}
    MStatus next(){++index;return operation();}
};
struct MFnField {
    std::size_t index;
    MFnField(MObject o,MStatus* s):index(o.index){*s=operation();}
    MStatus getForceAtPoint(const MVectorArray& p,const MVectorArray& v,
                           const MDoubleArray& m,MVectorArray& out,float){
        assert(p.size()==v.size() && p.size()==m.size());
        MStatus s=operation();
        // A failed host call is allowed to leave a populated result behind.
        out.assign(p.size()+(fields[index].wrongCount?1:0),fields[index].value);
        return fields[index].succeeds?s:MStatus(MS::kFailure);
    }
};
#include "dm_fields.inc"

int main(){
    btSoftBodyRigidBodyCollisionConfiguration config;
    btCollisionDispatcher dispatcher(&config);btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;
    dommy::bullet::DBoundedSoftDynamicsWorld world(&dispatcher,&broadphase,&solver,&config);
    world.setGravity(btVector3(0,0,0));
    btSphereShape shape(1);
    btRigidBody::btRigidBodyConstructionInfo ci(1,nullptr,&shape,btVector3(1,1,1));
    btRigidBody first(ci),second(ci);world.addRigidBody(&first);world.addRigidBody(&second);
    btSoftBodyWorldInfo info;info.m_broadphase=&broadphase;info.m_dispatcher=&dispatcher;
    const btVector3 positions[]={btVector3(0,0,0),btVector3(1,0,0)};
    const btScalar masses[]={1,1};btSoftBody soft(&info,2,positions,masses);world.addSoftBody(&soft);
    using dommy::bullet::SoftBodyForces;
    SoftBodyForces result={{&soft,btVector3(8,0,0)}};
    auto seed=[&](){first.clearForces();second.clearForces();first.applyCentralForce(btVector3(9,0,0));second.applyCentralForce(btVector3(9,0,0));result={{&soft,btVector3(8,0,0)}};};
    auto unchanged=[&](){assert(first.getTotalForce().x()==9 && second.getTotalForce().x()==9);assert(result.size()==1 && result[0].force.x()==8);};
    const double maximum=std::numeric_limits<float>::max();
    for(const auto& bad : std::vector<std::vector<Field>>{
            {{{1e100,0,0},true,false}},
            {{{maximum*.75,0,0},true,false},{{maximum*.75,0,0},true,false}},
            {{{std::numeric_limits<double>::quiet_NaN(),0,0},true,false}},
            {{{3,0,0},true,false},{{7,0,0},false,false}},
            {{{3,0,0},true,true}}}) {
        seed();fields=bad;assert(!dommy::bullet::applyWorldFields(&world,1.f/60,result));unchanged();
    }
    seed();first.setLinearFactor(btVector3(2,1,1));fields={{{maximum,0,0},true,false}};
    assert(!dommy::bullet::applyWorldFields(&world,1.f/60,result));unchanged();first.setLinearFactor(btVector3(1,1,1));
    fields={{{3,2,1},true,false},{{4,3,2},true,false}};
    seed();calls=0;assert(dommy::bullet::applyWorldFields(&world,1.f/60,result));const int total=calls;
    assert(first.getTotalForce().x()==7 && second.getTotalForce().x()==7 && result[0].force.x()==7);
    for(int failure=1;failure<=total;++failure){
        seed();calls=0;failAt=failure;assert(!dommy::bullet::applyWorldFields(&world,1.f/60,result));unchanged();
    }
    failAt=0;fields.clear();assert(dommy::bullet::applyWorldFields(&world,1.f/60,result));
    assert(first.getTotalForce().length2()==0 && result[0].force.length2()==0);
    seed();assert(!dommy::bullet::applyWorldFields(&world,0,result));unchanged();
    world.removeSoftBody(&soft);world.removeRigidBody(&second);world.removeRigidBody(&first);
    std::printf("Native DM fields: %d host failures, overflow/NaN/count rejection and unchanged force batches passed\n",total);
}
