#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <utility>
#include <vector>

#include "modules/dynamics/DBullet/Collision/DShapeFrameMath.h"
#include "modules/dynamics/DBullet/Collision/convex_decomposition_hacd.h"
#include "common/utility/DGenerationLimits.h"
#include "bullet_shape_progress_doubles.h"

static long allocationBudget = -1, allocationCount = 0;
void* operator new(std::size_t size) {
    if (allocationBudget == 0) throw std::bad_alloc();
    if (allocationBudget > 0) --allocationBudget;
    void* result = std::malloc(size ? size : 1);
    if (!result) throw std::bad_alloc();
    ++allocationCount;
    return result;
}
void operator delete(void* value) noexcept {
    if (value) --allocationCount;
    std::free(value);
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* value) noexcept { ::operator delete(value); }

using btScalar = float;
struct btVector3 {
    float values[4];
    btVector3(double x=0, double y=0, double z=0) : values{float(x),float(y),float(z),0} {}
    const float& getX() const { return values[0]; }
};
struct btTransform {
    void setIdentity() {}
    void setOrigin(const btVector3&) {}
};
class btConvexHullShape {
public:
    static int live;
    btConvexHullShape(const float*, int, int) { ++live; }
    virtual ~btConvexHullShape() { --live; }
    void setMargin(float) {}
};
int btConvexHullShape::live = 0;
class btCompoundShape {
    std::vector<btConvexHullShape*> children;
public:
    static int live;
    static int throwOnChild;
    btCompoundShape() { ++live; }
    virtual ~btCompoundShape() { --live; }
    void addChildShape(const btTransform&, btConvexHullShape* child) {
        children.push_back(child);
        // A partially completed insertion still must not orphan/double-free it.
        if (static_cast<int>(children.size()) == throwOnChild) throw std::bad_alloc();
    }
    int getNumChildShapes() const { return static_cast<int>(children.size()); }
};
int btCompoundShape::live = 0, btCompoundShape::throwOnChild = -1;
namespace HACD {
using Real = double;
template<typename T> struct Vec3 {
    T x, y, z;
    Vec3(T a=0, T b=0, T c=0) : x(a), y(b), z(c) {}
    T X() const { return x; } T Y() const { return y; } T Z() const { return z; }
};
// Bounded dependency double; malformed counts never allocate large buffers.
class HACD {
public:
    static int mode;
    void SetPoints(Vec3<Real>*) {} void SetNPoints(size_t) {}
    void SetTriangles(Vec3<long>*) {} void SetNTriangles(size_t) {}
    void SetCompacityWeight(double) {} void SetVolumeWeight(double) {}
    void SetNClusters(size_t) {} void SetNVerticesPerCH(size_t) {}
    void SetConcavity(double) {} void SetAddExtraDistPoints(bool) {}
    void SetAddNeighboursDistPoints(bool) {} void SetAddFacesPoints(bool) {}
    void SetCallBack(bool (*)(const char*, double, double, size_t)) {}
    bool Compute() { return mode != 1; }
    size_t GetNClusters() { return mode == 2 ? std::numeric_limits<size_t>::max() : 2; }
    size_t GetNPointsCH(size_t) { return mode == 3 ? std::numeric_limits<size_t>::max() : 4; }
    size_t GetNTrianglesCH(size_t) { return mode == 4 ? std::numeric_limits<size_t>::max() : 4; }
    bool GetCH(size_t, Vec3<Real>* p, Vec3<long>* t) {
        p[0]=Vec3<Real>(0,0,0); p[1]=Vec3<Real>(1,0,0);
        p[2]=Vec3<Real>(0,1,0); p[3]=Vec3<Real>(0,0,1);
        t[0]=Vec3<long>(0,2,1); t[1]=Vec3<long>(0,1,3);
        t[2]=Vec3<long>(0,3,2); t[3]=Vec3<long>(1,2,3);
        if (mode == 6) t[0].x = -1;
        if (mode == 7) p[0].x = std::numeric_limits<double>::infinity();
        return mode != 5;
    }
};
int HACD::mode = 0;
}

#include "bullet_hacd.inc"

static void noLeaks(long before) {
    assert(btCompoundShape::live == 0 && btConvexHullShape::live == 0);
    assert(MProgressWindow::activeCount() == 0);
    assert(allocationCount == before);
}

int main() {
    const float points[] = {0,0,0, 1,0,0, 0,1,0, 0,0,1};
    const unsigned int triangles[] = {0,2,1, 0,1,3, 0,3,2, 1,2,3};
    const long before = allocationCount;
    int failures = 0;
    bool succeeded = false;
    convex_decomposition_hacd builder;
    for (long failAt=0; failAt<100; ++failAt) {
        allocationBudget = failAt;
        try {
            std::unique_ptr<btCompoundShape> result(builder.ConvexDecomp(4, points, 12, triangles));
            allocationBudget = -1;
            assert(result && result->getNumChildShapes() == 2);
            succeeded = true;
        } catch (const std::bad_alloc&) { allocationBudget = -1; ++failures; }
        noLeaks(before);
        if (succeeded) break;
    }
    assert(succeeded && failures > 10);
    for (int child=1; child<=2; ++child) {
        btCompoundShape::throwOnChild = child;
        bool caught = false;
        try { std::unique_ptr<btCompoundShape> result(builder.ConvexDecomp(4, points, 12, triangles)); }
        catch (const std::bad_alloc&) { caught = true; }
        assert(caught); noLeaks(before);
    }
    btCompoundShape::throwOnChild = -1;
    for (int mode=1; mode<=7; ++mode) {
        HACD::HACD::mode = mode;
        std::unique_ptr<btCompoundShape> result(builder.ConvexDecomp(4, points, 12, triangles));
        assert(!result); noLeaks(before);
    }
    HACD::HACD::mode = 0;
    for (int retry=0; retry<3; ++retry) {
        { std::unique_ptr<btCompoundShape> result(builder.ConvexDecomp(4, points, 12, triangles));
          assert(result && result->getNumChildShapes() == 2); }
        noLeaks(before);
    }
    assert(!builder.ConvexDecomp(4, nullptr, 12, triangles));
    assert(!builder.ConvexDecomp(std::numeric_limits<int>::max(), points, 12, triangles));
    noLeaks(before);

    const mat3x3f inertia(2,0,0, 0,5,0, 0,0,7);
    const float a = deg2rad(30.0f);
    const quatf q(std::cos(a/2),0,0,std::sin(a/2));
    vec3f diagonal;
    assert(dommy::bullet::inertiaDiagonalInFrame(inertia, q, diagonal));
    assert(std::abs(diagonal[0]-2.75f)<1e-5f && std::abs(diagonal[1]-4.25f)<1e-5f);
    const vec3f normal = dommy::bullet::scaledDisplayNormal(vec3f(1,1,0), vec3f(2,1,3));
    assert(std::abs(normal[1] - 2*normal[0]) < 1e-6f);
    const vec3f zero = dommy::bullet::scaledDisplayNormal(vec3f(1,0,0), vec3f(0,1,1));
    assert(zero[0] == 0 && zero[1] == 0 && zero[2] == 0);
    const vec3f extreme = dommy::bullet::scaledDisplayNormal(vec3f(1,1,0),
        vec3f(std::numeric_limits<float>::denorm_min(), std::numeric_limits<float>::max(), 1));
    assert(std::isfinite(extreme[0]) && extreme[0] == 1);
    std::vector<vec3f> sourcePoints(1, vec3f(1,2,3));
    std::vector<vec3f> scaledPoints(1, vec3f(7,8,9));
    assert(!dommy::bullet::prepareScaledCollisionVertices(sourcePoints,
        vec3f(1,std::numeric_limits<float>::max(),1), scaledPoints));
    assert(scaledPoints.size()==1 && scaledPoints[0][0]==7 && scaledPoints[0][1]==8);
    assert(!dommy::bullet::prepareScaledCollisionVertices(sourcePoints,
        vec3f(std::numeric_limits<float>::quiet_NaN(),1,1), scaledPoints));
    assert(!dommy::bullet::isSafeCollisionCoordinate(1e10));
    assert(dommy::bullet::prepareScaledCollisionVertices(sourcePoints,
        vec3f(0,-2,3), scaledPoints));
    assert(scaledPoints[0][0]==0 && scaledPoints[0][1]==-4 && scaledPoints[0][2]==9);
    assert(dommy::bullet::isSingularScale(vec3f(0,1,1)));
    float boxVolume=7;
    vec3f boxInertia(8,9,10);
    assert(!dommy::bullet::boxMassFromDimensions(vec3f(-1,2,3),boxVolume,boxInertia));
    assert(boxVolume==7 && boxInertia[0]==8);
    assert(!dommy::bullet::boxMassFromDimensions(vec3f(FLT_MAX,2,3),boxVolume,boxInertia));
    assert(boxVolume==7 && boxInertia[0]==8);
    assert(dommy::bullet::boxMassFromDimensions(vec3f(2,4,6),boxVolume,boxInertia));
    assert(boxVolume==48 && std::abs(boxInertia[0]-52.0f/12)<1e-6f);
    assert(dommy::bullet::boxMassFromDimensions(vec3f(0,4,6),boxVolume,boxInertia));
    assert(boxVolume==0 && boxInertia[0]==0 && boxInertia[2]==0);
    vec3f weighted(7,8,9);
    for(float invalid : { -1.0f, std::numeric_limits<float>::quiet_NaN(), FLT_MAX }) {
        assert(!dommy::bullet::massScaledInertia(invalid,vec3f(1,2,3),weighted));
        assert(weighted[0]==7 && weighted[2]==9);
    }
    assert(dommy::bullet::massScaledInertia(2,vec3f(1,2,3),weighted));
    assert(weighted[0]==2 && weighted[1]==4 && weighted[2]==6);
    const vec3f savedWeighted=weighted;
    assert(!dommy::bullet::massScaledInertia(1e-40f,vec3f(1,1,1),weighted));
    assert(!dommy::bullet::massScaledInertia(1e-30f,vec3f(1e-30f,1e-30f,1e-30f),weighted));
    assert(!dommy::bullet::massScaledInertia(1,vec3f(1e-40f,1,1),weighted));
    assert(weighted[0]==savedWeighted[0]);
    assert(dommy::bullet::massScaledInertia(0,vec3f(1,2,3),weighted) && weighted[0]==0);
    assert(dommy::bullet::massScaledInertia(1,vec3f(0,0,0),weighted) && weighted[0]==0);
    dommy::bullet::RigidBodyParameters parameters;
    assert(!dommy::bullet::validateRigidBodyParameters(1e-50,0,0,0,0,parameters));
    assert(!dommy::bullet::validateRigidBodyParameters(1e-40,0,0,0,0,parameters));
    assert(dommy::bullet::isSafeInverseOperand(std::numeric_limits<float>::min()));
    assert(!dommy::bullet::isSafeInverseOperand(std::nextafter(std::numeric_limits<float>::min(),0.0f)));
    assert(dommy::bullet::safeMassGravity(1e37f,vec3f(0,-9.81f,0)));
    assert(!dommy::bullet::safeMassGravity(1e37f,vec3f(0,-100,0)));
    std::printf("HACD ownership: %d allocation failures, 2 partial insertions, 7 invalid outputs, 3 retries; frame math passed\n", failures);
}
