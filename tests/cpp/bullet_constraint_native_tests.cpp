#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include "btBulletDynamicsCommon.h"
#include "DConstraintFrame.h"

struct ProbeBase {
    std::shared_ptr<btTypedConstraint> m_constraint;
    bool changed=false;
    explicit ProbeBase(btTypedConstraint* value):m_constraint(value) {}
    void setPivotChanged(bool value) { changed=value; }
};
#include "constraint_native.inc"

static bool near(const btVector3& a,const btVector3& b) {
    return dommy::bullet::isFiniteVec3(a) && (a-b).length2()<1e-8f;
}
static btVector3 originA(const ProbeNail& p) {
    return static_cast<btPoint2PointConstraint*>(p.m_constraint.get())->getPivotInA();
}
static btVector3 originB(const ProbeNail& p) {
    return static_cast<btPoint2PointConstraint*>(p.m_constraint.get())->getPivotInB();
}
static btVector3 originA(const ProbeHinge& p) {
    return static_cast<btHingeConstraint*>(p.m_constraint.get())->getAFrame().getOrigin();
}
static btVector3 originB(const ProbeHinge& p) {
    return static_cast<btHingeConstraint*>(p.m_constraint.get())->getBFrame().getOrigin();
}
#define FRAME_ORIGINS(Name, Native) \
static btVector3 originA(const Probe##Name& p) { return static_cast<Native*>(p.m_constraint.get())->getFrameOffsetA().getOrigin(); } \
static btVector3 originB(const Probe##Name& p) { return static_cast<Native*>(p.m_constraint.get())->getFrameOffsetB().getOrigin(); }
FRAME_ORIGINS(Slider,btSliderConstraint)
FRAME_ORIGINS(Sixdof,btGeneric6DofConstraint)
#undef FRAME_ORIGINS
static bool setWorld(ProbeNail& p,const vec3f& v) { return p.set_world(v); }
template<class Probe> bool setWorld(Probe& p,const vec3f& v) { return p.set_world(v,quatf(1,0,0,0)); }

template<class Probe>
void test(Probe& probe,btRigidBody& a,btRigidBody& b) {
    const vec3f desired(2,3,4);
    assert(setWorld(probe,desired));
    assert(near(originA(probe),a.getWorldTransform().inverse()*btVector3(2,3,4)));
    assert(near(originB(probe),b.getWorldTransform().inverse()*btVector3(2,3,4)));
    const btVector3 oldA=originA(probe),oldB=originB(probe);
    for(float bad:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        assert(!setWorld(probe,vec3f(bad,0,0)));
        assert(near(originA(probe),oldA) && near(originB(probe),oldB));
    }
    const btTransform beforeA=a.getWorldTransform(),beforeB=b.getWorldTransform();
    btTransform identity; identity.setIdentity(); a.setWorldTransform(identity);
    btTransform distant=identity;
    distant.setOrigin(btVector3(-std::numeric_limits<float>::max(),0,0)); b.setWorldTransform(distant);
    assert(!setWorld(probe,vec3f(std::numeric_limits<float>::max(),0,0)));
    assert(near(originA(probe),oldA) && near(originB(probe),oldB));
    a.setWorldTransform(beforeA); b.setWorldTransform(beforeB);
}
int main() {
    btBoxShape shape(btVector3(1,1,1));
    btRigidBody a(0,nullptr,&shape),b(0,nullptr,&shape);
    a.setWorldTransform(btTransform(btQuaternion(btVector3(0,1,0),.3f),btVector3(1,2,0)));
    b.setWorldTransform(btTransform(btQuaternion(btVector3(0,0,1),-.5f),btVector3(-2,1,0)));
    btTransform identity; identity.setIdentity();
    ProbeNail nail(new btPoint2PointConstraint(a,b,btVector3(0,0,0),btVector3(0,0,0)));
    ProbeHinge hinge(new btHingeConstraint(a,b,identity,identity,false));
    ProbeSlider slider(new btSliderConstraint(a,b,identity,identity,false));
    ProbeSixdof sixdof(new btGeneric6DofConstraint(a,b,identity,identity,false));
    test(nail,a,b); test(hinge,a,b); test(slider,a,b); test(sixdof,a,b);
    const btVector3 old=originA(hinge);
    assert(!hinge.set_world(vec3f(0,0,0),quatf(0,0,0,0)) && near(originA(hinge),old));
    assert(!slider.set_world(vec3f(0,0,0),quatf(0,0,0,0)));
    assert(!sixdof.set_world(vec3f(0,0,0),quatf(0,0,0,0)));
    assert(hinge.set_world(vec3f(2,3,4),quatf(std::numeric_limits<float>::max(),0,0,0)));
    assert(dommy::bullet::finiteConstraintTransform(
        static_cast<btHingeConstraint*>(hinge.m_constraint.get())->getAFrame()));
    std::puts("Four real Bullet constraints: ordinary transformed frames, invalid inputs, second-side overflow and atomic frame preservation passed");
}
