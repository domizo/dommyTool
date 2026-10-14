// Native Bullet bodies and the complete production creation path. Only Maya's
// mesh triangulation/transform adapter is doubled; it returns controlled data.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftBody.h"
#include "BulletSoftBody/btSoftBodyHelpers.h"
#include "BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"
#include "SoftBody/DSoftBodyPreparation.h"
#include "mvl/mathUtils.h"
#include "shared_ptr.h"
#include "maya/MStatus.h"

#define DM_BULLET_SOFT_DEBUG 0
#define DOMMY_MAYA_DEBUG(...) ((void)0)
struct customDBData {};
struct MFloatPoint { float x, y, z; void cartesianize() {} };
struct MFloatPointArray : std::vector<MFloatPoint> {
    using std::vector<MFloatPoint>::vector;
    unsigned length() const { return static_cast<unsigned>(size()); }
};
struct MIntArray : std::vector<int> {
    using std::vector<int>::vector;
    unsigned length() const { return static_cast<unsigned>(size()); }
};
struct MObject { bool null = false; bool isNull() const { return null; } };
using MMatrix = btTransform;
enum class Failure { None, Data, Create, Points, PointCount, Triangles, Transform };
static Failure failure = Failure::None;
static MStatus ReadBulletGeometryTransform(const MMatrix& matrix, btTransform& transform, MVector&) {
    transform = matrix;
    return failure == Failure::Transform ? MS::kFailure : MS::kSuccess;
}
struct MFnMeshData {
    MObject create(MStatus* status) {
        *status = failure == Failure::Data ? MS::kFailure : MS::kSuccess;
        return MObject();
    }
};
struct DMeshInternalData {
    MFloatPointArray points;
    MIntArray triangles;
    MMatrix transform = btTransform::getIdentity();
    std::size_t numberOfPoints() const { return points.size(); }
    std::size_t numFace() const { return triangles.size() / 3; }
    MFloatPointArray getMPosition() const { return points; }
    MIntArray getFaceList() const { return MIntArray(numFace(), 3); }
    MIntArray getConnectList() const { return triangles; }
    MMatrix getTransformMatrix() const { return transform; }
};
struct MFnMesh {
    MFloatPointArray points;
    MIntArray triangles;
    void create(std::size_t, std::size_t, const MFloatPointArray& p,
                const MIntArray&, const MIntArray& t, MObject, MStatus* status) {
        points = p; triangles = t;
        *status = failure == Failure::Create ? MS::kFailure : MS::kSuccess;
    }
    MStatus getPoints(MFloatPointArray& p) {
        p = points;
        if (failure == Failure::PointCount) p.pop_back();
        return failure == Failure::Points ? MS::kFailure : MS::kSuccess;
    }
    MStatus getTriangles(MIntArray& counts, MIntArray& t) {
        counts = MIntArray(triangles.size() / 3, 1); t = triangles;
        return failure == Failure::Triangles ? MS::kFailure : MS::kSuccess;
    }
};
#include "dm_soft_params.inc"
#include "dm_soft_owner.inc"
#include "dm_soft_creation.inc"

static DMeshInternalData tetrahedron() {
    DMeshInternalData mesh;
    mesh.points = {{0,0,0}, {1,0,0}, {0,1,0}, {0,0,1}};
    mesh.triangles = {0,2,1, 0,1,3, 0,3,2, 1,2,3};
    return mesh;
}
static DMeshInternalData cloth() {
    DMeshInternalData mesh;
    mesh.points = {{0,0,0}, {1,0,0}, {1,1,0}, {0,1,0}};
    mesh.triangles = {0,1,2, 0,2,3};
    return mesh;
}
static bool near(float a, float b) { return std::abs(a-b) <= 1e-5f * std::max(1.0f, std::abs(b)); }
static void finiteBody(const btSoftBody& body) {
    for (int axis = 0; axis < 3; ++axis)
        assert(std::isfinite(body.m_bounds[0][axis]) && std::isfinite(body.m_bounds[1][axis]));
    for (int i = 0; i < body.m_nodes.size(); ++i) {
        assert(std::isfinite(body.m_nodes[i].m_im));
        for (int axis = 0; axis < 3; ++axis) {
            assert(std::isfinite(body.m_nodes[i].m_x[axis]));
            assert(std::isfinite(body.m_nodes[i].m_v[axis]));
        }
    }
    for (int i = 0; i < body.m_links.size(); ++i) {
        assert(std::isfinite(body.m_links[i].m_c0));
        assert(std::isnormal(body.m_links[i].m_c1));
    }
    for (int i = 0; i < body.m_pose.m_wgh.size(); ++i)
        assert(std::isfinite(body.m_pose.m_wgh[i]));
    assert(dommy::bullet::validSoftBodyClusters(body));
}

int main() {
    btSoftBodyRigidBodyCollisionConfiguration config;
    btCollisionDispatcher dispatcher(&config);
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;
    btSoftRigidDynamicsWorld world(&dispatcher, &broadphase, &solver, &config);
    world.setGravity(btVector3(0,0,0));
    auto& info = world.getWorldInfo();
    info.m_gravity.setZero();
    info.m_sparsesdf.Initialize();
    auto rejected = [&](const btSoftParams& p, const DMeshInternalData& mesh) {
        bool caught = false;
        try { auto owner = createSoftBodyShape(&info, &p, mesh); }
        catch (const std::exception&) { caught = true; }
        assert(caught);
        assert(world.getSoftBodyArray().size() == 0);
    };
    auto step = [&](btSoftBody& body) {
        world.addSoftBody(&body);
        world.stepSimulation(1.0f / 60, 1, 1.0f / 60);
        world.removeSoftBody(&body);
        finiteBody(body);
    };

    for (bool clusters : {false, true}) {
        btSoftParams p;
        p.m_mass = 0; p.m_is_cluster = clusters;
        p.m_set_pose_frame = true; p.m_set_pose_volume = true;
        auto owner = createSoftBodyShape(&info, &p, tetrahedron());
        auto& body = *owner->body();
        assert(body.getTotalMass() == 0 && body.clusterCount() == 0);
        assert(!body.m_pose.m_bframe && !body.m_pose.m_bvolume);
        assert(body.m_cfg.collisions & btSoftBody::fCollision::SDF_RS);
        for (int i = 0; i < body.m_nodes.size(); ++i) assert(body.m_nodes[i].m_im == 0);
        step(body);
        for (int i = 0; i < body.m_nodes.size(); ++i)
            assert(body.m_nodes[i].m_x == btVector3(tetrahedron().points[i].x,
                tetrahedron().points[i].y, tetrahedron().points[i].z));
    }
    {
        btSoftParams p;
        p.m_mass = 100; p.m_scale = 2; p.m_is_cluster = true; p.m_numClusters = 1;
        p.m_set_pose_frame = true; p.m_set_pose_volume = true;
        auto mesh = tetrahedron();
        mesh.transform = btTransform(btQuaternion(btVector3(0,1,0), .4f), btVector3(3,4,5));
        auto owner = createSoftBodyShape(&info, &p, mesh);
        auto& body = *owner->body();
        assert(near(body.getTotalMass(), 100));
        assert(body.clusterCount() == 1);
        assert(near(body.m_clusters[0]->m_imass, .01f));
        assert(body.m_pose.m_bframe && body.m_pose.m_bvolume);
        for (int i = 0; i < body.m_nodes.size(); ++i) {
            const auto& point = mesh.points[i];
            const btVector3 expected = mesh.transform * (2 * btVector3(point.x, point.y, point.z));
            assert((body.m_nodes[i].m_x - expected).length() < 1e-5f);
        }
        std::vector<float> xyz, masses;
        std::vector<double> weights;
        for (int i = 0; i < body.m_nodes.size(); ++i)
            for (int axis = 0; axis < 3; ++axis) xyz.push_back(body.m_nodes[i].m_x[axis]);
        assert(dommy::bullet::softBodyMassWeights(xyz, mesh.triangles, weights));
        assert(dommy::bullet::prepareSoftBodyMass(100, weights, masses));
        for (int i = 0; i < body.m_nodes.size(); ++i)
            assert(near(1 / body.m_nodes[i].m_im, masses[i]));
        const btMatrix3x3 inertia = body.m_clusters[0]->m_locii;
        const btVector3 center = body.m_clusters[0]->m_com;
        body.generateClusters(1, dommy::bullet::kMaxSoftClusterGenerationIterations);
        for (int row = 0; row < 3; ++row) {
            assert(near(center[row], body.m_clusters[0]->m_com[row]));
            for (int column = 0; column < 3; ++column)
                assert(near(inertia[row][column], body.m_clusters[0]->m_locii[row][column]));
        }
        step(body);
    }
    for (float scale : {.5f, 1.0f, 2.0f}) for (bool bends : {false, true})
    for (bool clusters : {false, true}) {
        btSoftParams p;
        p.m_rest_length_scale = scale; p.m_scale = 2; p.m_is_bend_constraint = bends;
        p.m_is_cluster = clusters; p.m_numClusters = 1;
        p.m_positionIterations = 0; // Keep geometry fixed while exercising native predictMotion.
        p.m_velocityIterations = 0; p.m_driftIterations = 0;
        auto mesh = cloth();
        mesh.transform = btTransform(btQuaternion(btVector3(1,0,0), .3f), btVector3(2,3,4));
        auto owner = createSoftBodyShape(&info, &p, mesh);
        auto& body = *owner->body();
        assert(body.m_links.size() == (bends ? 6 : 5));
        assert((body.m_fdbvt.m_root != nullptr) == !clusters);
        std::vector<float> lengths;
        for (int i = 0; i < body.m_links.size(); ++i) {
            const auto& link = body.m_links[i];
            const float expected = (link.m_n[0]->m_x - link.m_n[1]->m_x).length() * scale;
            assert(near(link.m_rl, expected));
            lengths.push_back(expected);
        }
        for (int tick = 0; tick < 3; ++tick) {
            step(body);
            assert(body.getRestLengthScale() == scale);
            for (int i = 0; i < body.m_links.size(); ++i) {
                assert(near(body.m_links[i].m_rl, lengths[i]));
                assert(near(body.m_links[i].m_c1, lengths[i] * lengths[i]));
            }
        }
    }
    btSoftParams defaults;
    for (float margin : {-1.f,1e30f,std::numeric_limits<float>::max(),
            std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        auto p=defaults; p.m_collisionMargin=margin;
        rejected(p,cloth());
        auto owner=createSoftBodyShape(&info,&defaults,cloth());
        const btScalar before=owner->body()->getCollisionShape()->getMargin();
        owner->set_collision_margin(margin);
        assert(owner->body()->getCollisionShape()->getMargin()==before);
    }
    const float marginLimit=std::nextafter(static_cast<float>(std::sqrt(std::sqrt(
        static_cast<double>(std::numeric_limits<float>::max())))/64),0.f);
    for (float margin : {0.f,.07f,marginLimit}) {
        auto p=defaults; p.m_collisionMargin=margin; p.m_mass=0;
        auto owner=createSoftBodyShape(&info,&p,cloth());
        for (int tick=0;tick<3;++tick) { owner->body()->predictMotion(1.f/60); finiteBody(*owner->body()); }
    }
    for (Failure mode : {Failure::Data, Failure::Create, Failure::Points,
                         Failure::PointCount, Failure::Triangles, Failure::Transform}) {
        failure = mode; rejected(defaults, tetrahedron());
    }
    failure = Failure::None;
    for (int mode = 0; mode < 6; ++mode) {
        auto mesh = cloth();
        if (mode == 0) mesh.points[2] = {2,0,0};
        if (mode == 1) mesh.points[2] = mesh.points[1];
        if (mode == 2) mesh.points.push_back({2,2,2});
        if (mode == 3) mesh.triangles[0] = -1;
        if (mode == 4) mesh.triangles[0] = 99;
        if (mode == 5) mesh.points[0].x = std::numeric_limits<float>::infinity();
        rejected(defaults, mesh);
        std::vector<float> xyz;
        for (auto point : mesh.points) { xyz.push_back(point.x); xyz.push_back(point.y); xyz.push_back(point.z); }
        bool caught = false;
        try { db_softbody_base body(info, xyz, mesh.triangles); }
        catch (const std::invalid_argument&) { caught = true; }
        assert(caught);
    }
    for (float value : {1e-30f, 1e30f}) {
        auto p = defaults; p.m_scale = value; rejected(p, tetrahedron());
        p = defaults; p.m_rest_length_scale = value; rejected(p, tetrahedron());
    }
    {
        auto p = defaults; p.m_mass = std::numeric_limits<float>::min();
        rejected(p, tetrahedron());
        p = defaults; p.m_is_cluster = true; p.m_mass = 1e30f;
        rejected(p, tetrahedron());
        p = defaults; p.m_set_pose_frame = true;
        rejected(p, cloth());
        p = defaults; p.m_Pressure = 1;
        rejected(p, cloth());
        p = defaults; p.m_is_bend_constraint = true;
        p.m_stiffness = std::numeric_limits<float>::min();
        rejected(p, cloth());
        auto mesh = tetrahedron(); mesh.transform.setOrigin(btVector3(1e20f,1e20f,1e20f));
        rejected(defaults, mesh);
    }
    // Preserve the historical bending-only interpretation of stiffness.
    {
        auto p = defaults; p.m_stiffness = .1;
        auto owner = createSoftBodyShape(&info, &p, cloth());
        assert(owner->body()->m_links[0].m_material->m_kLST == 1);
        p.m_set_pose_volume = true;
        owner = createSoftBodyShape(&info, &p, tetrahedron());
        assert(owner->body()->m_pose.m_bvolume && !owner->body()->m_pose.m_bframe);
        step(*owner->body());
    }
    std::puts("DM native soft creation: zero/weighted mass, geometry/pose rejection, final clusters and first/repeated rest scale passed");
}
