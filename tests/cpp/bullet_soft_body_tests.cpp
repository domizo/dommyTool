#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include "btBulletDynamicsCommon.h"
#include "DB_SoftBody_t.h"
#include "Collision/DCollisionMeshInput.h"
#include "BulletSoftBody/btSoftBodyHelpers.h"
#include "BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"

static long allocationBudget=-1,liveStandard=0,liveAligned=0;
void* operator new(std::size_t size) {
    if (allocationBudget==0) throw std::bad_alloc();
    if (allocationBudget>0) --allocationBudget;
    void* p=std::malloc(size?size:1); if(!p) throw std::bad_alloc(); ++liveStandard; return p;
}
void operator delete(void* p) noexcept { if(p) --liveStandard; std::free(p); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
static void* alignedAlloc(std::size_t size,int alignment) {
    void* p=nullptr;
    if(posix_memalign(&p,std::size_t(alignment),size)!=0) return nullptr;
    ++liveAligned; return p;
}
static void alignedFree(void* p) { if(p) --liveAligned; std::free(p); }

static bool near(float a,float b) { return std::abs(a-b)<=1e-4f*std::max(1.0f,std::max(std::abs(a),std::abs(b))); }
static void finite(const btSoftBody& body) {
    for (int i=0;i<body.m_nodes.size();++i) {
        assert(std::isfinite(body.m_nodes[i].m_im));
        for(int axis=0;axis<3;++axis) assert(std::isfinite(body.m_nodes[i].m_x[axis]));
    }
    for(int i=0;i<body.m_clusters.size();++i) {
        assert(std::isfinite(body.m_clusters[i]->m_imass));
        for(int a=0;a<3;++a) for(int b=0;b<3;++b)
            assert(std::isfinite(body.m_clusters[i]->m_locii[a][b]));
    }
    for (int axis=0;axis<3;++axis)
        assert(std::isfinite(body.m_bounds[0][axis]) && std::isfinite(body.m_bounds[1][axis]));
    for (int i=0;i<body.m_links.size();++i) {
        assert(std::isfinite(body.m_links[i].m_c0));
        assert(std::isnormal(body.m_links[i].m_c1));
    }
}

static void runtimeParameters(btSoftRigidDynamicsWorld& world, btSoftBodyWorldInfo& info,
                              const std::vector<float>& xyz, const std::vector<int>& faces) {
    const btVector3 gravity=info.m_gravity;
    const btScalar dt=btScalar(1)/60;
    {
        DB_SoftBody_t owner(info,xyz,faces,dommy::bullet::SoftBodyParameters(10,.2f,.07f,1));
        assert(owner.body()); auto& body=*owner.body();
        for (int i=0;i<body.m_nodes.size();++i) body.m_nodes[i].m_v=btVector3(2,0,0);
        world.addSoftBody(&body); world.stepSimulation(dt,1,dt);
        for (int i=0;i<body.m_nodes.size();++i) body.m_nodes[i].m_f=btVector3(11,12,13);
        assert(owner.set_mass(0));
        std::vector<btVector3> stopped;
        for (int i=0;i<body.m_nodes.size();++i) {
            const auto& node=body.m_nodes[i]; stopped.push_back(node.m_x);
            assert(node.m_im==0 && node.m_v==btVector3(0,0,0) && node.m_f==btVector3(0,0,0));
            assert(node.m_q==node.m_x);
        }
        for (int tick=0;tick<5;++tick) {
            world.stepSimulation(dt,1,dt); finite(body);
            for (int i=0;i<body.m_nodes.size();++i) assert(body.m_nodes[i].m_x==stopped[i]);
        }
        assert(owner.set_mass(10));
        world.stepSimulation(dt,1,dt); finite(body);
        for (int i=0;i<body.m_nodes.size();++i) {
            assert(near(body.m_nodes[i].m_x.x(),stopped[i].x())); // No old horizontal velocity.
            assert(body.m_nodes[i].m_x.y()<stopped[i].y()); // Gravity resumes.
        }
        world.removeSoftBody(&body);
    }
    info.m_gravity.setZero();
    for (int edit=0;edit<4;++edit) {
        DB_SoftBody_t owner(info,xyz,faces,dommy::bullet::SoftBodyParameters(10,.2f,.07f,1));
        assert(owner.body()); auto& body=*owner.body();
        assert(!body.m_bUpdateRtCst);
        std::vector<btScalar> lengths,squared,areas;
        for (int i=0;i<body.m_links.size();++i) {
            lengths.push_back(body.m_links[i].m_rl); squared.push_back(body.m_links[i].m_c1);
        }
        for (int i=0;i<body.m_faces.size();++i) areas.push_back(body.m_faces[i].m_ra);
        body.m_nodes[1].m_x+=btVector3(1,0,0);
        if(edit==0) assert(owner.set_mass(20));
        if(edit==1) assert(owner.set_num_clusters(2));
        if(edit==2) assert(owner.set_mass(10,true));
        if(edit==3) assert(owner.set_mass(0));
        assert(!body.m_bUpdateRtCst);
        for (int i=0;i<body.m_links.size();++i) {
            const auto& link=body.m_links[i];
            assert(link.m_c0==(link.m_n[0]->m_im+link.m_n[1]->m_im)/link.m_material->m_kLST);
        }
        world.addSoftBody(&body);
        for (int tick=0;tick<3;++tick) {
            world.stepSimulation(dt,1,dt); finite(body);
            for (int i=0;i<body.m_links.size();++i)
                assert(body.m_links[i].m_rl==lengths[i] && body.m_links[i].m_c1==squared[i]);
            for (int i=0;i<body.m_faces.size();++i) assert(body.m_faces[i].m_ra==areas[i]);
        }
        world.removeSoftBody(&body);
    }
    {
        DB_SoftBody_t owner(info,xyz,faces,dommy::bullet::SoftBodyParameters(10,.2f,.07f,1));
        auto& body=*owner.body();
        body.m_nodes[0].m_v=btVector3(2,0,0); body.m_nodes[0].m_f=btVector3(3,0,0);
        for (float margin:{-1.f,1e30f,std::numeric_limits<float>::max(),
                std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
            const dommy::bullet::SoftBodyParameters invalid(0,.5f,margin,2);
            DB_SoftBody_t rejected(info,xyz,faces,invalid); assert(!rejected.body());
            assert(!owner.set_parameters(invalid));
            owner.set_collision_margin(margin);
            assert(body.getCollisionShape()->getMargin()==.07f && near(owner.get_total_mass(),10));
            assert(body.m_nodes[0].m_v==btVector3(2,0,0) && body.m_nodes[0].m_f==btVector3(3,0,0));
            assert(!body.m_bUpdateRtCst);
        }
    }
    const float marginLimit=std::nextafter(static_cast<float>(std::sqrt(std::sqrt(
        static_cast<double>(std::numeric_limits<float>::max())))/64),0.f);
    for (float margin:{0.f,.07f,marginLimit}) {
        assert(dommy::bullet::isSafeSoftBodyCollisionMargin(margin));
        DB_SoftBody_t owner(info,xyz,faces,dommy::bullet::SoftBodyParameters(0,.2f,margin,1));
        assert(owner.body());
        for (int tick=0;tick<3;++tick) { owner.body()->predictMotion(dt); finite(*owner.body()); }
    }
    info.m_gravity=gravity;
    std::puts("Soft runtime parameters: stopped positions/velocity/forces, resumed gravity, retained rest lengths/areas, updated coefficients and bounded margins passed");
}
int main() {
    btAlignedAllocSetCustomAligned(alignedAlloc,alignedFree);
    btSoftBodyRigidBodyCollisionConfiguration config;
    btCollisionDispatcher dispatcher(&config);
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;
    btSoftRigidDynamicsWorld world(&dispatcher,&broadphase,&solver,&config);
    btSoftBodyWorldInfo& info=world.getWorldInfo();
    info.m_dispatcher=&dispatcher; info.m_broadphase=&broadphase;
    info.m_sparsesdf.Initialize(); info.m_gravity=btVector3(0,-9.81f,0);
    const std::vector<float> xyz={0,0,0,2,0,0,2,1,0,0,1,0};
    const std::vector<int> faces={0,1,2,0,2,3};
    runtimeParameters(world,info,xyz,faces);
    int constructorFailures=0; bool constructed=false;
    for(int limit=0;limit<256 && !constructed;++limit) {
        const long standard=liveStandard,aligned=liveAligned;
        allocationBudget=limit;
        try { { DB_SoftBody_t probe(info,xyz,faces); assert(probe.body()); }
            allocationBudget=-1; constructed=true; }
        catch(const std::bad_alloc&) { allocationBudget=-1; ++constructorFailures; }
        assert(liveStandard==standard && liveAligned==aligned);
    }
    assert(constructed && constructorFailures>0);
    DB_SoftBody_t body(info,xyz,faces);
    assert(body.body()); finite(*body.body());
    {
        dommy::bullet::SoftBodyParameters parameters(0,.2f,.07f,1);
        DB_SoftBody_t configured(info,xyz,faces,parameters);
        assert(configured.body() && configured.body()->m_clusters.size()==0);
        assert(configured.get_total_mass()==0 && configured.body()->m_cfg.kDF==.2f);
        assert(configured.body()->getCollisionShape()->getMargin()==.07f);
        parameters.mass=20;
        assert(configured.set_parameters(parameters));
        assert(near(configured.get_total_mass(),20) && configured.body()->m_clusters.size()==1);
        auto* cluster=configured.body()->m_clusters[0];
        assert(configured.set_parameters(parameters) && configured.body()->m_clusters[0]==cluster);
        parameters.mass=40; parameters.clusters=8193; parameters.friction=.9f;
        assert(!configured.set_parameters(parameters));
        assert(near(configured.get_total_mass(),20) && configured.body()->m_cfg.kDF==.2f);
    }
    {
        // Sparse meshes remain usable without paying for default dynamic clusters.
        std::vector<float> large; std::vector<int> triangles;
        for(int i=0;i<3000;i+=3) {
            large.insert(large.end(),{float(i/3),0,0,float(i/3)+.5f,0,0,float(i/3),.5f,0});
            triangles.insert(triangles.end(),{i,i+1,i+2});
        }
        DB_SoftBody_t rejected(info,large,triangles); assert(!rejected.body());
        dommy::bullet::SoftBodyParameters parameters(0,.2f,.07f,100);
        DB_SoftBody_t sparse(info,large,triangles,parameters);
        assert(sparse.body() && sparse.body()->m_clusters.size()==0);
        assert(!sparse.set_mass(100) && sparse.get_total_mass()==0);
        parameters.mass=100; parameters.clusters=1;
        assert(sparse.set_parameters(parameters)); finite(*sparse.body());
        assert(near(sparse.get_total_mass(),100) && sparse.body()->m_clusters.size()==1);
        assert(!sparse.set_num_clusters(100)); finite(*sparse.body());
        assert(sparse.body()->m_clusters.size()==1);
    }
    std::unique_ptr<btSoftBody> old(btSoftBodyHelpers::CreateFromTriMesh(info,xyz.data(),faces.data(),2));
    auto* material=old->appendMaterial(); material->m_kLST=.5f;
    old->generateBendingConstraints(2,material); old->setTotalMass(100,true); old->randomizeConstraints();
    assert(old->m_links.size()==body.body()->m_links.size());
    for(int i=0;i<old->m_links.size();++i) {
        const auto& a=old->m_links[i]; const auto& b=body.body()->m_links[i];
        for(int j=0;j<2;++j) assert(a.m_n[j]-&old->m_nodes[0]==b.m_n[j]-&body.body()->m_nodes[0]);
        assert(a.m_bbending==b.m_bbending && near(a.m_rl,b.m_rl));
    }
    for(int i=0;i<4;++i) assert(near(old->m_nodes[i].m_im,body.body()->m_nodes[i].m_im));
    for(float invalid:{-1.f,1e-40f,1e-30f,1e37f,std::numeric_limits<float>::infinity()}) {
        const float before=body.body()->m_nodes[0].m_im;
        assert(!body.set_mass(invalid)); assert(body.body()->m_nodes[0].m_im==before); finite(*body.body());
    }
    for(float mass:{0.f,100.f,0.f,10.f,100.f}) {
        assert(body.set_mass(mass)); finite(*body.body());
        assert(near(body.get_total_mass(),mass));
        if(mass==0) assert(body.body()->m_clusters.size()==0);
        else assert(body.body()->m_clusters.size()>0);
        world.addSoftBody(body.body()); world.stepSimulation(1.f/60,1,1.f/60); world.removeSoftBody(body.body());
        finite(*body.body());
    }
    const int clusters=body.body()->m_clusters.size();
    assert(!body.set_num_clusters(0) && !body.set_num_clusters(8193));
    assert(body.body()->m_clusters.size()==clusters);
    assert(body.set_num_clusters(1) && body.set_mass(25,true)); finite(*body.body());
    int failures=0; bool done=false;
    for(int budget=0;budget<100 && !done;++budget) {
        const float before=body.body()->m_nodes[0].m_im;
        allocationBudget=budget;
        try { assert(body.set_mass(50)); allocationBudget=-1; done=true; }
        catch(const std::bad_alloc&) { allocationBudget=-1; ++failures; assert(body.body()->m_nodes[0].m_im==before); }
    }
    assert(done && failures>0);
    const std::vector<float> collinear={0,0,0,1,0,0,2,0,0};
    DB_SoftBody_t invalid(info,collinear,{0,1,2}); assert(!invalid.body());
    DB_SoftBody_t orphan(info,{0,0,0,1,0,0,0,1,0,2,2,2},{0,1,2}); assert(!orphan.body());
    std::printf("Native soft bodies: %d constructor allocation failures without standard/native leaks; legacy link ordering, finite masses/clusters, zero recovery, stepping and staging failures passed\n",constructorFailures);
    info.m_sparsesdf.Reset();
}
