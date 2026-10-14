#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <new>
#include "modules/dynamics/DBullet/Collision/DCollisionMeshInput.h"
#include "modules/dynamics/DBullet/mvl/vec.h"
using vec3f=mvl::vec<float,3>;
using quatf=mvl::vec<float,4>;
template<class T> using shared_ptr=std::shared_ptr<T>;
static long budget=-1,live=0;
void* operator new(std::size_t size) {
    if(budget==0) throw std::bad_alloc(); if(budget>0) --budget;
    void* p=std::malloc(size?size:1); if(!p) throw std::bad_alloc(); ++live; return p;
}
void operator delete(void* p) noexcept { if(p) --live; std::free(p); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
static int implementations=0;
struct collision_shape_impl_t { collision_shape_impl_t(){++implementations;} virtual ~collision_shape_impl_t(){--implementations;} };
struct rigid_body_impl_t:collision_shape_impl_t {};
struct nail_constraint_impl_t:collision_shape_impl_t {};
struct hinge_constraint_impl_t:collision_shape_impl_t {};
struct slider_constraint_impl_t:collision_shape_impl_t {};
struct sixdof_constraint_impl_t:collision_shape_impl_t {};
template<class Impl> struct Owner {
    shared_ptr<Impl> owner;
    template<class... Args> explicit Owner(const shared_ptr<Impl>& p,Args&&...):owner(p) {}
    template<class... Args> explicit Owner(Impl* p,Args&&...):owner(p) {} // Legacy mutation only.
    Impl* impl() { return owner.get(); }
};
#define WRAPPER(Name,Impl) struct Name:Owner<Impl> { using pointer=shared_ptr<Name>; using Owner<Impl>::Owner; };
WRAPPER(dbCollisionShape_t,collision_shape_impl_t)
WRAPPER(dbSphere_shape_t,collision_shape_impl_t)
WRAPPER(dbPlane_shape_t,collision_shape_impl_t)
WRAPPER(dbBox_shape_t,collision_shape_impl_t)
WRAPPER(convex_hull_shape_t,collision_shape_impl_t)
WRAPPER(dbHacdShape_t,collision_shape_impl_t)
WRAPPER(composite_shape_t,collision_shape_impl_t)
WRAPPER(dbMeshShape_t,collision_shape_impl_t)
WRAPPER(dbRigidBody_t,rigid_body_impl_t)
WRAPPER(dbNail_constraint_t,nail_constraint_impl_t)
WRAPPER(dbHinge_constraint_t,hinge_constraint_impl_t)
WRAPPER(dbSlider_constraint_t,slider_constraint_impl_t)
WRAPPER(dbSixdof_constraint_t,sixdof_constraint_impl_t)
#define BACKEND(Name,Impl) template<class... Args> Impl* Name(Args&&...) { return new Impl(); }
struct Backend {
    BACKEND(create_sphere_shape,collision_shape_impl_t)
    BACKEND(create_plane_shape,collision_shape_impl_t)
    BACKEND(create_box_shape,collision_shape_impl_t)
    BACKEND(create_convex_hull_shape,collision_shape_impl_t)
    BACKEND(create_hacd_shape,collision_shape_impl_t)
    BACKEND(create_composite_shape,collision_shape_impl_t)
    BACKEND(create_dynamic_mesh_shape,collision_shape_impl_t)
    BACKEND(create_static_mesh_shape,collision_shape_impl_t)
    BACKEND(create_rigid_body,rigid_body_impl_t)
    BACKEND(create_nail_constraint,nail_constraint_impl_t)
    BACKEND(create_hinge_constraint,hinge_constraint_impl_t)
    BACKEND(create_slider_constraint,slider_constraint_impl_t)
    BACKEND(create_sixdof_constraint,sixdof_constraint_impl_t)
};
struct dbSolver_t {
    static std::recursive_mutex m_mutex;
    static shared_ptr<Backend> m_impl;
    #include "bullet_factory_declarations.inc"
};
std::recursive_mutex dbSolver_t::m_mutex;
shared_ptr<Backend> dbSolver_t::m_impl(new Backend());
#include "bullet_factories.inc"

template<class F> static void allocations(F run) {
    bool done=false; int failures=0;
    for(int i=0;i<256 && !done;++i) {
        const long count=live; const int objects=implementations; budget=i;
        try { run(); budget=-1; done=true; }
        catch(const std::bad_alloc&) { budget=-1; ++failures; }
        assert(live==count && implementations==objects);
    }
    assert(done && failures>0);
}
int main() {
    using namespace dommy::bullet;
    SoftBodyParameters parameters;
    assert(prepareSoftBodyParameters(0,.2,.07,1,parameters));
    assert(parameters.mass==0 && parameters.clusters==1);
    for(double invalid:{-1.,1e-50,1e-40,std::numeric_limits<double>::infinity()}) {
        assert(!prepareSoftBodyParameters(invalid,.2,.07,1,parameters));
        assert(parameters.mass==0 && parameters.clusters==1);
    }
    assert(!prepareSoftBodyParameters(1,1.000000001,.07,1,parameters));
    assert(!prepareSoftBodyParameters(1,.2,-1,1,parameters));
    assert(!prepareSoftBodyParameters(1,.2,1e30,1,parameters));
    assert(!isSafeSoftBodyCollisionMargin(std::numeric_limits<float>::max()));
    assert(!isSafeSoftBodyCollisionMargin(std::numeric_limits<double>::quiet_NaN()));
    assert(isSafeSoftBodyCollisionMargin(0) && isSafeSoftBodyCollisionMargin(.07));
    assert(!prepareSoftBodyParameters(1,.2,.07,0,parameters));
    assert(!prepareSoftBodyParameters(1,.2,.07,8193,parameters));
    std::size_t work=42;
    assert(!softBodyClusterWorkBudget(65535,21845,100,work) && work==42);
    assert(softBodyClusterWorkBudget(65535,21845,1,work) && work<=kMaxSoftBodyWork);
    assert(!softBodyClusterWorkBudget(4,0,1,work));
    assert(!softBodyClusterWorkBudget(4,std::numeric_limits<std::size_t>::max(),1,work));
    assert(!softBodyClusterWorkBudget(4,2,std::numeric_limits<std::size_t>::max(),work));
    const std::vector<float> xyz={0,0,0,1,0,0,1,1,0,0,1,0};
    const std::vector<int> faces={0,1,2,0,2,3};
    SoftBodyTopology topology;
    assert(hasValidSoftBodyMeshInput(xyz,faces) && prepareSoftBodyTopology(xyz,faces,topology));
    assert(topology.edges.size()==5 && topology.bends==std::vector<SoftBodyEdge>({{1,3}}));
    allocations([&] { SoftBodyTopology value; assert(prepareSoftBodyTopology(xyz,faces,value)); });
    for(const auto& invalid:std::vector<std::vector<float>>{{0,0,0,1,0,0,2,0,0},{0,0,0,1e-20f,0,0,0,1e-20f,0}})
        assert(!hasValidSoftBodyMeshInput(invalid,{0,1,2}));
    assert(!prepareSoftBodyTopology({0,0,0,1,0,0,0,1,0,2,2,2},{0,1,2},topology));
    std::vector<float> masses={7};
    assert(!prepareSoftBodyMass(1e-40f,topology.weights,masses) && masses==std::vector<float>({7}));
    for(float mass:{0.f,1.f,100.f}) assert(prepareSoftBodyMass(mass,topology.weights,masses));
    {
        std::vector<float> large; std::vector<int> indices;
        for(int i=0;i<50001;i+=3) {
            large.insert(large.end(),{float(i/3),0,0,float(i/3)+.5f,0,0,float(i/3),.5f,0});
            indices.insert(indices.end(),{i,i+1,i+2});
        }
        SoftBodyTopology value;
        assert(hasValidSoftBodyMeshInput(large,indices) && prepareSoftBodyTopology(large,indices,value));
        assert(value.edges.size()==50001 && value.bends.empty());
    }
    {
        std::vector<float> star={0,0,0}; std::vector<int> indices;
        const int count=11000;
        for(int i=0;i<count;++i) {
            const double angle=6.283185307179586*i/count;
            star.insert(star.end(),{float(std::cos(angle)),float(std::sin(angle)),0});
            indices.insert(indices.end(),{0,i+1,(i+1)%count+1});
        }
        SoftBodyTopology value;
        assert(hasValidSoftBodyMeshInput(star,indices) && !prepareSoftBodyTopology(star,indices,value));
    }
    const vec3f vertices[]={vec3f(0,0,0),vec3f(1,0,0),vec3f(0,1,0)}, normals[]={vec3f(0,0,1),vec3f(0,0,1),vec3f(0,0,1)};
    const unsigned int triangles[]={0,1,2}; const vec3f p(1,1,1); const quatf q(1,0,0,0);
    dbCollisionShape_t::pointer shape(new dbCollisionShape_t(shared_ptr<collision_shape_impl_t>(new collision_shape_impl_t())));
    dbRigidBody_t::pointer a(new dbRigidBody_t(shared_ptr<rigid_body_impl_t>(new rigid_body_impl_t()))),b(new dbRigidBody_t(shared_ptr<rigid_body_impl_t>(new rigid_body_impl_t())));
    allocations([&] { auto v=dbSolver_t::create_sphere_shape(1); assert(v); });
    allocations([&] { auto v=dbSolver_t::create_plane_shape(p,0); assert(v); });
    allocations([&] { auto v=dbSolver_t::create_box_shape(p); assert(v); });
    allocations([&] { auto v=dbSolver_t::create_convex_hull_shape(vertices,3,normals,triangles,3); assert(v); });
    allocations([&] { auto v=dbSolver_t::create_hacd_shape(vertices,3,normals,triangles,3,true); assert(v); });
    vec3f pos=p; quatf rot=q;
    allocations([&] { auto v=dbSolver_t::create_composite_shape(&shape,&pos,&rot,1); assert(v); });
    for(bool dynamic:{false,true}) allocations([&] { auto v=dbSolver_t::create_mesh_shape(vertices,3,normals,triangles,3,dynamic); assert(v); });
    allocations([&] { auto v=dbSolver_t::create_rigid_body(shape); assert(v); });
    allocations([&] { auto v=dbSolver_t::create_nail_constraint(a,p); assert(v); });
    allocations([&] { auto v=dbSolver_t::create_nail_constraint(a,b,p,p); assert(v); });
#define BOTH(Name) allocations([&]{ auto v=dbSolver_t::Name(a,p,q); assert(v); }); allocations([&]{ auto v=dbSolver_t::Name(a,p,q,b,p,q); assert(v); });
    BOTH(create_hinge_constraint) BOTH(create_slider_constraint) BOTH(create_sixdof_constraint)
    std::puts("Soft-body topology: 50001 vertices, bounded distance-2 work, degenerate/orphan rejection, zero/inverse mass checks passed");
    std::puts("All 16 production factories / 17 paths: allocation-failure ownership checks passed");
}
