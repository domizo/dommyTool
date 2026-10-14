// Real Bullet/HACD construction; OpenGL is a matrix/array recorder, not a GPU.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <utility>
#include <vector>
#include "btBulletDynamicsCommon.h"
#include "BulletCollision/Gimpact/btGImpactShape.h"
#include "HACD/hacdHACD.h"
#include "modules/dynamics/DBullet/Collision/DShapeFrameMath.h"
#include "common/utility/DGenerationLimits.h"
#include "bullet_shape_progress_doubles.h"

static long allocationBudget = -1, liveAllocations = 0, liveAligned = 0;
void* operator new(std::size_t size) {
    if (allocationBudget == 0) throw std::bad_alloc();
    if (allocationBudget > 0) --allocationBudget;
    void* result = std::malloc(size ? size : 1);
    if (!result) throw std::bad_alloc();
    ++liveAllocations;
    return result;
}
void operator delete(void* value) noexcept {
    if (value) --liveAllocations;
    std::free(value);
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* value) noexcept { ::operator delete(value); }
static void* countedAlignedAlloc(std::size_t size, int alignment) {
    void* result = nullptr;
    if (posix_memalign(&result, static_cast<std::size_t>(alignment), size) != 0) return nullptr;
    ++liveAligned;
    return result;
}
static void countedAlignedFree(void* value) {
    if (value) --liveAligned;
    std::free(value);
}

enum { GL_ENABLE_BIT, GL_NORMALIZE, GL_VERTEX_ARRAY, GL_NORMAL_ARRAY,
       GL_FRONT_AND_BACK, GL_FILL, GL_LINE, GL_FLOAT, GL_TRIANGLES, GL_UNSIGNED_INT };
struct Matrix {
    double values[4][4];
    Matrix() { for (int i=0;i<4;++i) for (int j=0;j<4;++j) values[i][j] = i==j ? 1 : 0; }
};
static Matrix matrix;
static std::vector<Matrix> matrices;
static bool normalizeNormals = false;
static std::vector<bool> enables;
static const float* vertexData;
static const float* normalData;
static std::vector<vec3f> drawnVertices, drawnNormals;
static void multiply(const Matrix& rhs) {
    Matrix output;
    for (int i=0;i<4;++i) for (int j=0;j<4;++j) {
        output.values[i][j]=0;
        for (int k=0;k<4;++k) output.values[i][j]+=matrix.values[i][k]*rhs.values[k][j];
    }
    matrix=output;
}
static void glPushMatrix() { matrices.push_back(matrix); }
static void glPopMatrix() { matrix=matrices.back(); matrices.pop_back(); }
static void glPushAttrib(int) { enables.push_back(normalizeNormals); }
static void glPopAttrib() { normalizeNormals=enables.back(); enables.pop_back(); }
static void glEnable(int value) { assert(value==GL_NORMALIZE); normalizeNormals=true; }
static void glTranslatef(float x, float y, float z) {
    Matrix m; m.values[0][3]=x; m.values[1][3]=y; m.values[2][3]=z; multiply(m);
}
static void glRotatef(float degrees, float x, float y, float z) {
    if (degrees == 0.0f) return;
    const btMatrix3x3 r(btQuaternion(btVector3(x,y,z),deg2rad(degrees)));
    Matrix m; for(int i=0;i<3;++i) for(int j=0;j<3;++j) m.values[i][j]=r[i][j]; multiply(m);
}
static void glScalef(float x, float y, float z) {
    Matrix m; m.values[0][0]=x; m.values[1][1]=y; m.values[2][2]=z; multiply(m);
}
static void glEnableClientState(int) {} static void glDisableClientState(int) {}
static void glPolygonMode(int,int) {}
static void glVertexPointer(int,int,int,const void* p) { vertexData=static_cast<const float*>(p); }
static void glNormalPointer(int,int,const void* p) { normalData=static_cast<const float*>(p); }
static void glDrawElements(int,size_t count,int,const void* data) {
    const auto* indices=static_cast<const unsigned int*>(data);
    btMatrix3x3 basis;
    for(int i=0;i<3;++i) for(int j=0;j<3;++j) basis[i][j]=matrix.values[i][j];
    const btMatrix3x3 normalMatrix=basis.inverse().transpose();
    for(size_t i=0;i<count;++i) {
        const float* p=vertexData+indices[i]*3;
        const float* n=normalData+indices[i]*3;
        vec3f out;
        for(int a=0;a<3;++a) out[a]=matrix.values[a][0]*p[0]+matrix.values[a][1]*p[1]+matrix.values[a][2]*p[2]+matrix.values[a][3];
        drawnVertices.push_back(out);
        btVector3 normal=normalMatrix*btVector3(n[0],n[1],n[2]);
        if(normalizeNormals && normal.length2()>0) normal.normalize();
        drawnNormals.push_back(vec3f(normal.x(),normal.y(),normal.z()));
    }
}

#include "dbCollisionShape_t.h"
#include "bt_convex_hull_shape.h"
#include "DB_MeshShape_t.h"
#include "DB_HacdShape_t.h"
#include "bt_composite_shape.h"
#include "BaseShape/Box/DB_BoxShape_t.h"
#include "bullet_sphere.inc"
#include "bullet_hacd.inc"

void wire_cube() {} void solid_cube() {}
void wire_sphere() {} void solid_sphere() {}
struct Box : DB_BoxShape_t { Box() : DB_BoxShape_t(vec3f(1,2,3)) {} };
struct Sphere : DB_SphereShape_t { Sphere() : DB_SphereShape_t(1) {} };

struct Hull : bt_convex_hull_shape_t {
    Hull(const vec3f* p,const vec3f* n,const unsigned int* t) : bt_convex_hull_shape_t(p,8,n,t,36) {}
};
struct Mesh : DB_MeshShape_t {
    Mesh(const vec3f* p,const vec3f* n,const unsigned int* t,bool dynamic) : DB_MeshShape_t(p,8,n,t,36,dynamic) {}
};
struct Hacd : DB_HacdShape_t {
    Hacd(const vec3f* p,const vec3f* n,const unsigned int* t) : DB_HacdShape_t(p,8,n,t,36,true) {}
};
struct Wrapped : dbCollisionShape_t {
    explicit Wrapped(collision_shape_impl_t* value) : dbCollisionShape_t(shared_ptr<collision_shape_impl_t>(value)) {}
};
struct Composite : bt_composite_shape_t {
    Composite(dbCollisionShape_t::pointer* child, vec3f* position, quatf* rotation, int count=1)
        : bt_composite_shape_t(child, position, rotation, count) {}
};
static bool near(const vec3f& a,const vec3f& b,float tolerance=3e-4f) {
    for(int i=0;i<3;++i) if(!std::isfinite(a[i]) || !std::isfinite(b[i]) || std::abs(a[i]-b[i])>tolerance) return false;
    return true;
}
static vec3f unit(const vec3f& v) {
    const double length=std::hypot(std::hypot(double(v[0]),double(v[1])),double(v[2]));
    if(length==0) return vec3f(0,0,0);
    return vec3f(v[0]/length,v[1]/length,v[2]/length);
}
static vec3f scaled(const vec3f& v,const vec3f& s) {
    return vec3f(v[0]*s[0],v[1]*s[1],v[2]*s[2]);
}
static void collectCollisionPoints(btCollisionShape* shape, const btTransform& parent,
                                   std::vector<vec3f>& points) {
    if(shape->getShapeType()==COMPOUND_SHAPE_PROXYTYPE) {
        auto* compound=static_cast<btCompoundShape*>(shape);
        for(int i=0;i<compound->getNumChildShapes();++i)
            collectCollisionPoints(compound->getChildShape(i),parent*compound->getChildTransform(i),points);
    } else if(shape->getShapeType()==CONVEX_HULL_SHAPE_PROXYTYPE) {
        auto* hull=static_cast<btConvexHullShape*>(shape);
        for(int i=0;i<hull->getNumPoints();++i) {
            const btVector3 p=parent*hull->getScaledPoint(i);
            points.push_back(vec3f(p.x(),p.y(),p.z()));
        }
    } else {
        struct Collector : btTriangleCallback {
            const btTransform& transform;
            std::vector<vec3f>& output;
            Collector(const btTransform& t,std::vector<vec3f>& o) : transform(t),output(o) {}
            void processTriangle(btVector3* triangle,int,int) override {
                for(int i=0;i<3;++i) {
                    const btVector3 p=transform*triangle[i];
                    output.push_back(vec3f(p.x(),p.y(),p.z()));
                }
            }
        } collector(parent,points);
        assert(shape->isConcave());
        static_cast<btConcaveShape*>(shape)->processAllTriangles(
            &collector,btVector3(-1e20f,-1e20f,-1e20f),btVector3(1e20f,1e20f,1e20f));
    }
}
static void compareBounds(const std::vector<vec3f>& a,const std::vector<vec3f>& b) {
    assert(!a.empty() && !b.empty());
    for(int axis=0;axis<3;++axis) {
        float amin=a[0][axis],amax=amin,bmin=b[0][axis],bmax=bmin;
        for(const auto& p:a) { amin=std::min(amin,p[axis]); amax=std::max(amax,p[axis]); }
        for(const auto& p:b) { bmin=std::min(bmin,p[axis]); bmax=std::max(bmax,p[axis]); }
        assert(std::abs(amin-bmin)<3e-4f && std::abs(amax-bmax)<3e-4f);
    }
}

static void assertEncloses(const btVector3& minimum, const btVector3& maximum,
                           const std::vector<vec3f>& points) {
    assert(!points.empty());
    for (const auto& point : points) for (int axis=0; axis<3; ++axis) {
        assert(std::isfinite(minimum[axis]) && std::isfinite(maximum[axis]));
        assert(point[axis] >= minimum[axis]-1e-3f && point[axis] <= maximum[axis]+1e-3f);
    }
}
static void checkNativeBounds(btCollisionShape* shape) {
    std::vector<vec3f> points;
    collectCollisionPoints(shape, btTransform::getIdentity(), points);
    btVector3 minimum, maximum;
    shape->getAabb(btTransform::getIdentity(), minimum, maximum);
    assertEncloses(minimum, maximum, points);
    if (shape->getShapeType() == COMPOUND_SHAPE_PROXYTYPE) {
        auto* compound = static_cast<btCompoundShape*>(shape);
        for (int i=0; i<compound->getNumChildShapes(); ++i) {
            checkNativeBounds(compound->getChildShape(i));
            const btDbvtNode* node = compound->getChildList()[i].m_node;
            assert(node);
            points.clear();
            collectCollisionPoints(compound->getChildShape(i), compound->getChildTransform(i), points);
            assertEncloses(node->volume.Mins(), node->volume.Maxs(), points);
        }
    }
}

static void checkScaleFailures(collision_shape_impl_t& shape) {
    vec3f savedScale;
    shape.get_scale(savedScale);
    const vec3f savedInertia = shape.local_inertia();
    const float savedVolume = shape.volume();
    btCollisionShape* const native = shape.getBulletCollisionShape();
    std::vector<vec3f> savedPoints;
    collectCollisionPoints(native, btTransform::getIdentity(), savedPoints);
    const auto preserved = [&]() {
        vec3f scale;
        shape.get_scale(scale);
        assert(near(scale, savedScale) && near(shape.local_inertia(), savedInertia));
        assert(shape.volume() == savedVolume && native == shape.getBulletCollisionShape());
        std::vector<vec3f> points;
        collectCollisionPoints(native, btTransform::getIdentity(), points);
        assert(points.size() == savedPoints.size());
        for (std::size_t i=0; i<points.size(); ++i) assert(near(points[i], savedPoints[i]));
        checkNativeBounds(native);
    };
    for (float invalid : {std::numeric_limits<float>::quiet_NaN(),
                           std::numeric_limits<float>::infinity(),
                           std::numeric_limits<float>::max(),
                           -std::numeric_limits<float>::max()}) {
        assert(!shape.set_scale(vec3f(1,invalid,1)));
        preserved();
    }
    int failures = 0;
    bool succeeded = false;
    for (long failAt=0; failAt<200 && !succeeded; ++failAt) {
        const long before = liveAllocations, alignedBefore = liveAligned;
        allocationBudget = failAt;
        try {
            assert(shape.set_scale(vec3f(2,3,4)));
            allocationBudget = -1;
            succeeded = true;
        } catch (const std::bad_alloc&) {
            allocationBudget = -1;
            ++failures;
            assert(liveAllocations == before && liveAligned == alignedBefore);
            preserved();
        }
    }
    assert(succeeded && failures > 0);
    shape.set_scale(savedScale);
    preserved();
}

static void checkWorldScaling(collision_shape_impl_t& shape) {
    btDefaultCollisionConfiguration configuration;
    btCollisionDispatcher dispatcher(&configuration);
    btDbvtBroadphase broadphase;
    btCollisionWorld world(&dispatcher, &broadphase, &configuration);
    btCollisionObject object;
    object.setCollisionShape(shape.getBulletCollisionShape());
    object.setWorldTransform(btTransform::getIdentity());
    world.addCollisionObject(&object);
    btCollisionShape* const native = object.getCollisionShape();
    for (const vec3f& scale : {vec3f(2,3,4), vec3f(0.5f,0.5f,0.5f), vec3f(1,1,1)}) {
        shape.set_scale(scale);
        assert(native == shape.getBulletCollisionShape());
        // The solver uses Bullet's default force-update-all-AABBs behavior.
        world.updateAabbs();
        std::vector<vec3f> points;
        collectCollisionPoints(native, btTransform::getIdentity(), points);
        assertEncloses(object.getBroadphaseHandle()->m_aabbMin,
                       object.getBroadphaseHandle()->m_aabbMax, points);
        const btVector3 from(0,0,20), to(0,0,0);
        btCollisionWorld::ClosestRayResultCallback hit(from, to);
        world.rayTest(from, to, hit);
        assert(hit.hasHit());
        assert(std::abs(hit.m_hitPointWorld.z()-3*scale[2]) < 0.1f);
    }
    world.removeCollisionObject(&object);
}

static void checkScaleRestoration(collision_shape_impl_t& shape) {
    for(const vec3f& original:{vec3f(1,1,1),vec3f(0,1,1)}) {
        assert(shape.set_scale(original));
        const vec3f inertia=shape.local_inertia();
        std::vector<vec3f> before,after;
        collectCollisionPoints(shape.getBulletCollisionShape(),btTransform::getIdentity(),before);
        {
            collision_shape_scale_transaction<collision_shape_impl_t> transaction(shape,vec3f(2,3,4));
            assert(transaction);
            vec3f scale; shape.get_scale(scale); assert(near(scale,vec3f(2,3,4)));
        }
        vec3f scale; shape.get_scale(scale); assert(near(scale,original));
        assert(near(shape.local_inertia(),inertia));
        collectCollisionPoints(shape.getBulletCollisionShape(),btTransform::getIdentity(),after);
        assert(before.size()==after.size());
        for(std::size_t i=0;i<before.size();++i) assert(near(before[i],after[i]));
        checkNativeBounds(shape.getBulletCollisionShape());
        int failures=0;
        bool completed=false;
        for(long budget=0;budget<500 && !completed;++budget) {
            const long allocations=liveAllocations, aligned=liveAligned;
            allocationBudget=budget;
            try {
                {
                    collision_shape_scale_transaction<collision_shape_impl_t> transaction(shape,vec3f(2,3,4));
                    assert(transaction);
                }
                allocationBudget=-1;
                completed=true;
            } catch(const std::bad_alloc&) {
                allocationBudget=-1;
                ++failures;
                assert(liveAllocations==allocations && liveAligned==aligned);
            }
            shape.get_scale(scale); assert(near(scale,original));
            assert(near(shape.local_inertia(),inertia));
            after.clear();
            collectCollisionPoints(shape.getBulletCollisionShape(),btTransform::getIdentity(),after);
            assert(before.size()==after.size());
            for(std::size_t i=0;i<before.size();++i) assert(near(before[i],after[i]));
            checkNativeBounds(shape.getBulletCollisionShape());
        }
        assert(completed && failures>0);
    }
    assert(shape.set_scale(vec3f(1,1,1)));
}

static collision_shape_impl_t* makeMeshShape(int kind, const vec3f* points,
                                            const vec3f* normals, const unsigned int* triangles) {
    if (kind==0) return new Hull(points,normals,triangles);
    if (kind==1) return new Mesh(points,normals,triangles,true);
    if (kind==2) return new Hacd(points,normals,triangles);
    return new Mesh(points,normals,triangles,false);
}

static void checkCompoundScaling(const vec3f* points, const vec3f* normals, const unsigned int* triangles) {
    for (int kind=0; kind<4; ++kind) {
        dbCollisionShape_t::pointer children[2] = {
            dbCollisionShape_t::pointer(new Wrapped(makeMeshShape(kind,points,normals,triangles))),
            dbCollisionShape_t::pointer(new Wrapped(makeMeshShape(kind,points,normals,triangles)))};
        vec3f positions[2]={vec3f(-3,1,0),vec3f(3,-1,0)};
        const float angle=deg2rad(25.0f);
        quatf rotations[2]={mvl::qidentity<float>(),quatf(std::cos(angle/2),0,0,std::sin(angle/2))};
        Composite compound(children,positions,rotations,2);
        collision_shape_impl_t& shape=compound;
        assert(shape.getBulletCollisionShape());
        for (const vec3f& scale : {vec3f(2,3,4),vec3f(0,1,1),vec3f(0,0,0),vec3f(-1,2,1),vec3f(1,1,1)}) {
            vec3f before; shape.get_scale(before);
            auto prepared=shape.prepare_scale(scale);
            assert(prepared);
            vec3f afterPrepare; shape.get_scale(afterPrepare);
            assert(near(before,afterPrepare));
            prepared->commit();
            prepared->commit(); // A repeated commit must not swap buffers back.
            vec3f accepted; shape.get_scale(accepted);
            assert(near(accepted,scale));
            checkNativeBounds(shape.getBulletCollisionShape());
            for (int axis=0;axis<3;++axis) {
                if (shape.local_inertia()[axis] > prepared->inertia[axis]+1e-2f)
                    std::fprintf(stderr,"kind=%d scale=(%g,%g,%g) axis=%d actual=%g prepared=%g\n",kind,scale[0],scale[1],scale[2],axis,shape.local_inertia()[axis],prepared->inertia[axis]);
                assert(shape.local_inertia()[axis] <= prepared->inertia[axis]+1e-2f);
            }
            for (auto& child : children) {
                child->get_scale(accepted);
                assert(near(accepted,scale));
                if (!dommy::bullet::isSingularScale(scale)) {
                    drawnVertices.clear(); drawnNormals.clear(); child->gl_draw();
                    std::vector<vec3f> geometry;
                    collectCollisionPoints(child->getBulletCollisionShape(),btTransform::getIdentity(),geometry);
                    compareBounds(geometry,drawnVertices);
                }
            }
        }
        checkScaleFailures(shape); // Includes failure after earlier children prepared.
        checkScaleRestoration(shape);
        // Already retained children must not be adopted by another compound.
        Composite duplicate(children,positions,rotations,2);
        collision_shape_impl_t& invalid=duplicate;
        assert(!invalid.getBulletCollisionShape());
    }

    // A real later-child rejection must discard an already prepared first child.
    vec3f large[8]; for(int i=0;i<8;++i) large[i]=points[i]*1.0e7f;
    dbCollisionShape_t::pointer children[2]={
        dbCollisionShape_t::pointer(new Wrapped(new Hull(points,normals,triangles))),
        dbCollisionShape_t::pointer(new Wrapped(new Hull(large,normals,triangles)))};
    vec3f positions[2]={vec3f(0,0,0),vec3f(0,0,0)};
    quatf rotations[2]={mvl::qidentity<float>(),mvl::qidentity<float>()};
    Composite compound(children,positions,rotations,2);
    assert(!compound.set_scale(vec3f(2,3,4)));
    for(auto& child:children) { vec3f scale; child->get_scale(scale); assert(near(scale,vec3f(1,1,1))); }
    std::puts("Compound private-child staging, zero/mirror recovery, bounds, display and failure preservation passed");
}

static void checkPrimitives() {
    for(int kind=0;kind<2;++kind) {
        std::unique_ptr<collision_shape_impl_t> shape(kind==0
            ? static_cast<collision_shape_impl_t*>(new Box()) : static_cast<collision_shape_impl_t*>(new Sphere()));
        btCollisionShape* native=shape->getBulletCollisionShape();
        const vec3f initialInertia=shape->local_inertia();
        const float initialVolume=shape->volume();
        for(const vec3f& scale:{vec3f(2,3,4),vec3f(0,1,1),vec3f(0,0,0),vec3f(-1,2,1),vec3f(1,1,1)}) {
            assert(shape->set_scale(scale));
            assert(native==shape->getBulletCollisionShape());
            vec3f actual; shape->get_scale(actual); assert(near(actual,scale));
            assert(std::isfinite(shape->volume()) && shape->volume()>=0);
            for(int axis=0;axis<3;++axis) assert(std::isfinite(shape->local_inertia()[axis]) && shape->local_inertia()[axis]>=0);
            if(scale[0]==0) assert(shape->volume()==0 && near(shape->local_inertia(),vec3f(0,0,0)));
            btVector3 minimum,maximum;
            native->getAabb(btTransform::getIdentity(),minimum,maximum);
            const vec3f half=kind==0 ? vec3f(std::abs(scale[0]),2*std::abs(scale[1]),3*std::abs(scale[2]))
                                    : vec3f(std::abs(scale[0]),std::abs(scale[0]),std::abs(scale[0]));
            assert(near(vec3f(minimum.x(),minimum.y(),minimum.z()),-half));
            assert(near(vec3f(maximum.x(),maximum.y(),maximum.z()),half));
        }
        assert(near(initialInertia,shape->local_inertia()) && std::abs(initialVolume-shape->volume())<1e-4f);
        for(float invalid:{std::numeric_limits<float>::quiet_NaN(),FLT_MAX,-FLT_MAX}) {
            assert(!shape->set_scale(vec3f(invalid,1,1)));
            assert(near(initialInertia,shape->local_inertia()) && initialVolume==shape->volume());
        }
        auto staged=shape->prepare_scale(vec3f(2,2,2));
        assert(staged && near(initialInertia,shape->local_inertia()));
        staged.reset();
        assert(near(initialInertia,shape->local_inertia()));
        bool succeeded=false;
        int failures=0;
        for(long budget=0;budget<20 && !succeeded;++budget) {
            const long before=liveAllocations,alignedBefore=liveAligned;
            allocationBudget=budget;
            try {
                auto candidate=shape->prepare_scale(vec3f(2,2,2));
                allocationBudget=-1;
                assert(candidate);
                succeeded=true;
            } catch(const std::bad_alloc&) { allocationBudget=-1; ++failures; }
            assert(liveAllocations==before && liveAligned==alignedBefore);
            vec3f actual; shape->get_scale(actual);
            assert(near(actual,vec3f(1,1,1)) && near(initialInertia,shape->local_inertia()));
        }
        assert(succeeded && failures>0);
    }
    for(int kind=0;kind<2;++kind) {
        dbCollisionShape_t::pointer child(new Wrapped(kind==0
            ? static_cast<collision_shape_impl_t*>(new Box()) : static_cast<collision_shape_impl_t*>(new Sphere())));
        vec3f position(0,0,0);
        quatf rotation=mvl::qidentity<float>();
        Composite compound(&child,&position,&rotation);
        for(int i=1;i<=100;++i) {
            auto candidate=compound.prepare_scale(vec3f(i*0.013f,i*0.019f,i*0.027f));
            assert(candidate);
            candidate->commit();
            for(int axis=0;axis<3;++axis)
                assert(compound.local_inertia()[axis]<=candidate->inertia[axis]);
        }
    }
    std::puts("Box/Sphere checked absolute dimensions, invalid rejection and zero-to-unit recovery passed");
}
int main() {
    btAlignedAllocSetCustomAligned(countedAlignedAlloc, countedAlignedFree);
    const vec3f corners[]={vec3f(-1,-2,-3),vec3f(1,-2,-3),vec3f(1,2,-3),vec3f(-1,2,-3),
                          vec3f(-1,-2,3),vec3f(1,-2,3),vec3f(1,2,3),vec3f(-1,2,3)};
    const unsigned int triangles[]={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,3,7,6,3,6,2,0,4,7,0,7,3,1,2,6,1,6,5};
    const vec3f scales[]={vec3f(1,1,1),vec3f(2,1,3),vec3f(1,1,1),vec3f(-1,2,1)};
    unsigned int soupIndices[36]; for(unsigned int i=0;i<36;++i) soupIndices[i]=i;
    int cases=0;
    for (float angle : {0.0f,30.0f,70.0f}) for(int translated=0;translated<2;++translated) {
        const float a=deg2rad(angle), c=std::cos(a), s=std::sin(a);
        mat3x3f rotation(c,-s,0,s,c,0,0,0,1);
        if(angle==70.0f) {
            const float tilt=deg2rad(20.0f), ct=std::cos(tilt), st=std::sin(tilt);
            rotation=prod(rotation,mat3x3f(1,0,0,0,ct,-st,0,st,ct));
        }
        vec3f points[8], normals[8];
        for(int i=0;i<8;++i) {
            points[i]=prod(rotation,corners[i])+vec3f(10*translated,20*translated,30*translated);
            normals[i]=prod(rotation,unit(corners[i]));
        }
        for(int kind=0;kind<4;++kind) {
            std::unique_ptr<collision_shape_impl_t> shape;
            if(kind==0) shape.reset(new Hull(points,normals,triangles));
            if(kind==1) shape.reset(new Mesh(points,normals,triangles,true));
            if(kind==2) shape.reset(new Hacd(points,normals,triangles));
            if(kind==3) shape.reset(new Mesh(points,normals,triangles,false));
            assert(shape->getBulletCollisionShape());
            const vec3f initialInertia=shape->local_inertia();
            mat3x3f Q,Qinv;
            q_to_mat(shape->rotation(),Q); q_to_mat(qconj(shape->rotation()),Qinv);
            const vec3f center=shape->center();
            for(size_t scaleIndex=0;scaleIndex<4;++scaleIndex) {
                const vec3f scale=scales[scaleIndex];
                auto prepared = shape->prepare_scale(scale);
                assert(prepared);
                prepared->commit();
                std::vector<vec3f> preparedPoints;
                collectCollisionPoints(shape->getBulletCollisionShape(),btTransform::getIdentity(),preparedPoints);
                assertEncloses(btVector3(prepared->minimum[0],prepared->minimum[1],prepared->minimum[2]),
                               btVector3(prepared->maximum[0],prepared->maximum[1],prepared->maximum[2]),preparedPoints);
                if(scaleIndex==0 && kind!=3) assert(near(initialInertia,shape->local_inertia()));
                drawnVertices.clear(); drawnNormals.clear();
                shape->gl_draw(dbCollisionShape_t::kDSSolid);
                assert(drawnVertices.size()==36 && drawnNormals.size()==36);
                assert(matrices.empty() && enables.empty() && !normalizeNormals);
                for(size_t i=0;i<36;++i) {
                    const unsigned int index=triangles[i];
                    vec3f expected=scaled(points[index],scale);
                    vec3f expectedNormal=unit(vec3f(normals[index][0]/scale[0],normals[index][1]/scale[1],normals[index][2]/scale[2]));
                    if(kind==1) {
                        expected=center+prod(Q,scaled(prod(Qinv,points[index]-center),scale));
                        const vec3f localNormal=prod(Qinv,normals[index]);
                        expectedNormal=unit(prod(Q,vec3f(localNormal[0]/scale[0],localNormal[1]/scale[1],localNormal[2]/scale[2])));
                    }
                    assert(near(expected,drawnVertices[i]));
                    assert(near(expectedNormal,drawnNormals[i]));
                }
                dommy::bullet::MassProperties properties;
                if(!dommy::bullet::computeMassProperties(drawnVertices.data(),36,soupIndices,36,properties)) {
                    std::fprintf(stderr,"Mass failed kind=%d angle=%f translated=%d scale=%zu\n",kind,angle,translated,scaleIndex);
                    assert(false);
                }
                assert(near(diag(properties.inertia),shape->local_inertia(),1e-3f));
                assert(std::abs(properties.volume-shape->volume())<1e-2f);
                std::vector<vec3f> collisionPoints;
                collectCollisionPoints(shape->getBulletCollisionShape(),btTransform::getIdentity(),collisionPoints);
                compareBounds(collisionPoints,drawnVertices);
                checkNativeBounds(shape->getBulletCollisionShape());
                ++cases;
            }
            const vec3f savedInertia=shape->local_inertia();
            shape->set_scale(vec3f(std::numeric_limits<float>::quiet_NaN(),1,1));
            vec3f actualScale;
            shape->get_scale(actualScale);
            assert(near(actualScale,scales[3]) && near(shape->local_inertia(),savedInertia));
            if(kind==0) {
                shape->set_scale(vec3f(0,1,1));
                drawnVertices.clear(); drawnNormals.clear();
                shape->gl_draw(dbCollisionShape_t::kDSSolid);
                for(const auto& normal:drawnNormals) assert(near(normal,vec3f(0,0,0)));
                assert(shape->volume()==0);
                shape->set_scale(vec3f(1,1,1));
                assert(near(shape->local_inertia(),initialInertia));
            }
            for (const vec3f& scale : {vec3f(0,1,1), vec3f(0,0,0),
                                       vec3f(-1,2,1), vec3f(1,1,1)}) {
                shape->set_scale(scale);
                shape->get_scale(actualScale);
                assert(near(actualScale, scale));
                if (dommy::bullet::isSingularScale(scale)) {
                    assert(shape->volume() == 0 && near(shape->local_inertia(), vec3f(0,0,0)));
                }
                checkNativeBounds(shape->getBulletCollisionShape());
            }
            checkScaleFailures(*shape);
            checkScaleRestoration(*shape);
            shape->set_scale(vec3f(1.00001f,1,1));
            shape->get_scale(actualScale);
            assert(actualScale[0] == 1.00001f);
            checkNativeBounds(shape->getBulletCollisionShape());
            shape->set_scale(vec3f(1,1,1));
            if (angle == 0 && translated == 0) checkWorldScaling(*shape);
        }
    }
    {
        vec3f normals[8];
        for (int i=0; i<8; ++i) normals[i]=unit(corners[i]);
        dbCollisionShape_t::pointer child(new Wrapped(new Hull(corners,normals,triangles)));
        vec3f position(0,0,0);
        quatf rotation=mvl::qidentity<float>();
        bool succeeded=false;
        int failures=0;
        for (long failAt=0; failAt<30 && !succeeded; ++failAt) {
            const long before=liveAllocations, alignedBefore=liveAligned;
            allocationBudget=failAt;
            try {
                Composite compound(&child,&position,&rotation);
                allocationBudget=-1;
                succeeded=true;
            } catch (const std::bad_alloc&) { allocationBudget=-1; ++failures; }
            assert(liveAllocations==before && liveAligned==alignedBefore);
        }
        assert(succeeded && failures>=4);
        std::printf("Composite ownership: %d allocation failures without native/std leaks\n",failures);
    }
    assert(MProgressWindow::activeCount()==0);
    {
        vec3f normals[8]; for(int i=0;i<8;++i) normals[i]=unit(corners[i]);
        checkCompoundScaling(corners,normals,triangles);
        checkPrimitives();
    }
    assert(liveAligned==0);
    std::printf("Bullet 2.88 native shapes: %d rotation/translation/scale cases; constructor-update inertia and recorded GL positions/normals passed\n",cases);
    std::puts("Scale zero/recovery/rejection/allocation failures; native AABBs, compound trees, world broadphase and rays passed");
}
