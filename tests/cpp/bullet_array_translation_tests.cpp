// Actual array callbacks and shape resolver with failure-injectable host doubles.
// Optional native mode also executes DB_RigidBody_t's real transform methods.
#include "maya/MStatus.h"
#include "modules/dynamics/DBullet/Collision/DShapeFrameMath.h"
#include "modules/dynamics/DBullet/Collision/DCollisionMeshInput.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <memory>
#include <stdexcept>

#ifdef DOMMY_NATIVE_BULLET
#include <btBulletDynamicsCommon.h>
#else
struct btTransform {
    vec3f position = vec3f(0, 0, 0);
    quatf rotation = mvl::qidentity<float>();
};
struct btRigidBody {
    btTransform world, interpolation;
    const btTransform& getWorldTransform() const { return world; }
    const btTransform& getInterpolationWorldTransform() const { return interpolation; }
    void setWorldTransform(const btTransform& value) { world = value; }
    void updateInertiaTensor() {} // Native tensor behavior is covered by bullet_pose_native_tests.
    void setInterpolationWorldTransform(const btTransform& value) { interpolation = value; }
};
#endif

using float3 = float[3];
struct Entry { unsigned logical; float3 value; };
namespace fixture {
std::vector<Entry> entries;
int checks = 0, failAt = 0, throwingChecks = 0, throwAt = 0;
int shapeFault = 0, defaultShapes = 0, removed = 0, created = 0;
bool connected = false, failLock = false;
double translation = 5;
void mayThrow() {
    if (++throwingChecks == throwAt) throw std::runtime_error("injected failure");
}
MStatus check() {
    mayThrow();
    return ++checks == failAt ? MS::kFailure : MS::kSuccess;
}
void reset() {
    entries = {{0, {100, 0, 0}}, {1, {200, 0, 0}}};
    checks = failAt = throwingChecks = throwAt = shapeFault = defaultShapes = removed = created = 0;
    connected = failLock = false;
    translation = 5;
}
}
struct MObject {};
struct MMatrix {
    double translation = 0;
    MMatrix inverse() const { MMatrix result; result.translation = -translation; return result; }
    MMatrix operator*(MMatrix right) const { right.translation += translation; return right; }
};
namespace MSpace { enum Space { kTransform }; }
struct MTransformationMatrix {
    MMatrix matrix;
    MTransformationMatrix() = default;
    explicit MTransformationMatrix(MMatrix value) : matrix(value) {}
    MMatrix asMatrix() const { return matrix; }
    MVector getTranslation(MSpace::Space, MStatus* status) const {
        *status = fixture::check(); return MVector(matrix.translation, 0, 0);
    }
};
namespace dommy { namespace maya {
bool isFiniteMatrix(const MMatrix& value) { return std::isfinite(value.translation); }
bool isFloatRepresentableVector(const MVector& value) {
    float result;
    return numeric::checkedFloatCast(value.x, result) &&
        numeric::checkedFloatCast(value.y, result) && numeric::checkedFloatCast(value.z, result);
}
} }
struct MPlugArray;
struct MPlug {
    MPlug() = default;
    MPlug(MObject, int) {}
    MStatus getValue(MObject&) const {
        return fixture::shapeFault == 1 ? MS::kFailure : fixture::check();
    }
    bool connectedTo(MPlugArray&, bool, bool, MStatus*) const;
    MObject node(MStatus* status) const {
        *status = fixture::shapeFault == 5 ? MS::kFailure : fixture::check(); return {};
    }
};
struct MPlugArray : std::vector<MPlug> {
    unsigned length() const { return static_cast<unsigned>(size()); }
};
bool MPlug::connectedTo(MPlugArray& out, bool destination, bool source, MStatus* status) const {
    assert(destination && !source);
    *status = fixture::shapeFault == 2 ? MS::kFailure : fixture::check();
    if (fixture::connected && fixture::shapeFault != 3) out.emplace_back();
    if (fixture::shapeFault == 4) out.emplace_back();
    return fixture::connected && fixture::shapeFault != 12;
}
struct MDataHandle {
    Entry* entry = nullptr;
    int asInt() const { return 2; }
    const float3& asFloat3() const { return entry->value; }
    void set3Float(float x, float y, float z) {
        entry->value[0] = x; entry->value[1] = y; entry->value[2] = z;
        fixture::mayThrow(); // Includes an exception after a host write.
    }
    void set(bool) {}
    void setClean() {}
};
struct MArrayDataHandle {
    unsigned cursor = 0;
    unsigned elementCount(MStatus* status) const {
        *status = fixture::check(); return static_cast<unsigned>(fixture::entries.size());
    }
    MStatus jumpToArrayElement(unsigned position) {
        cursor = position; return position < fixture::entries.size() ? fixture::check() : MS::kFailure;
    }
    MStatus jumpToElement(unsigned logical) {
        const auto status = fixture::check();
        if (!status) return status;
        for (unsigned i = 0; i < fixture::entries.size(); ++i) {
            if (fixture::entries[i].logical == logical) { cursor = i; return MS::kSuccess; }
        }
        return MS::kFailure;
    }
    MDataHandle outputValue(MStatus* status) {
        *status = fixture::check(); MDataHandle handle; handle.entry = &fixture::entries.at(cursor); return handle;
    }
    MStatus setAllClean() { return fixture::check(); }
};
struct MDataBlock {
    MDataHandle inputValue(int, MStatus* status) { *status = fixture::check(); return {}; }
    MDataHandle outputValue(int, MStatus* status) { *status = fixture::check(); return {}; }
    MArrayDataHandle outputArrayValue(int, MStatus* status) { *status = fixture::check(); return {}; }
    MStatus setClean(const MPlug&) { return fixture::check(); }
};
struct MFnDependencyNode {
    MFnDependencyNode(MObject, MStatus* status) {
        *status = fixture::shapeFault == 6 ? MS::kFailure : fixture::check();
    }
};
struct MFnDagNode {
    MFnDagNode(MObject, MStatus* status) { *status = fixture::check(); }
    MObject parent(int, MStatus* status) { *status = fixture::check(); return {}; }
};
struct MFnTransform {
    MFnTransform(MObject, MStatus* status) { *status = fixture::check(); }
    MTransformationMatrix transformation(MStatus* status) {
        *status = fixture::check(); MTransformationMatrix result;
        result.matrix.translation = fixture::translation; return result;
    }
};
struct dbCollisionShape_t { using pointer = std::shared_ptr<dbCollisionShape_t>; bool fallback = false; };
struct BodyImplementation {
    virtual ~BodyImplementation() = default;
    virtual void get_transform(vec3f&, quatf&) const = 0;
    virtual void set_transform(const vec3f&, const quatf&) = 0;
};
struct DB_RigidBody_t : BodyImplementation {
#ifdef DOMMY_NATIVE_BULLET
    struct Shift {
        void getCenterOfMassTransformShift(btTransform& result) {
            result.setIdentity(); result.setOrigin(btVector3(2, 3, 4));
        }
    } shift;
    Shift* m_collisionShapeObserver = &shift;
    std::unique_ptr<btRigidBody> m_body;
    DB_RigidBody_t() : m_body(new btRigidBody(btRigidBody::btRigidBodyConstructionInfo(0, nullptr, nullptr))) {}
#include "bullet_array_native_transforms.inc"
#else
    std::unique_ptr<btRigidBody> m_body{new btRigidBody()};
    void get_transform(vec3f& position, quatf& rotation) const override {
        position = m_body->world.position; rotation = m_body->world.rotation;
    }
    void set_transform(const vec3f& position, const quatf& rotation) override {
        btTransform result; result.position = position; result.rotation = rotation;
        m_body->setWorldTransform(result); m_body->setInterpolationWorldTransform(result);
    }
#endif
    btRigidBody* body() { return m_body.get(); }
};
struct dbRigidBody_t {
    using pointer = std::shared_ptr<dbRigidBody_t>;
    DB_RigidBody_t implementation;
    dbCollisionShape_t::pointer shape;
    bool missingImplementation = false;
    bool invalidRotation = false, invalidPosition = false;
    BodyImplementation* impl() { return missingImplementation ? nullptr : &implementation; }
    void get_transform(vec3f& position, quatf& rotation) {
        fixture::mayThrow(); implementation.get_transform(position, rotation);
        if (invalidRotation) rotation = quatf(0, 0, 0, 0);
        if (invalidPosition) position[0] = std::numeric_limits<float>::infinity();
    }
    void set_transform(const vec3f& position, const quatf& rotation) {
        fixture::mayThrow(); implementation.set_transform(position, rotation); fixture::mayThrow();
    }
};
struct DBCollisionShapeNode {
    enum { typeId = 1 };
    MStatus evaluate_collision_shape(dbCollisionShape_t::pointer& result) {
        if (fixture::shapeFault == 10) return MS::kFailure;
        if (fixture::shapeFault != 11) result = std::make_shared<dbCollisionShape_t>();
        return fixture::check();
    }
};
namespace dommy { namespace maya {
template<class T> T* checkedUserNode(const MFnDependencyNode&, int, MStatus* status) {
    static T node;
    *status = fixture::shapeFault == 7 ? MS::kInvalidParameter :
        fixture::shapeFault == 8 || fixture::shapeFault == 9 ? MS::kFailure : fixture::check();
    return *status ? &node : nullptr;
}
} }
struct dbSolver_t {
    static bool lock_solver() { fixture::mayThrow(); return !fixture::failLock; }
    static dbCollisionShape_t::pointer create_sphere_shape() {
        ++fixture::defaultShapes;
        if (fixture::shapeFault == 13) return {};
        auto result = std::make_shared<dbCollisionShape_t>(); result->fallback = true; return result;
    }
    static dbRigidBody_t::pointer create_rigid_body(const dbCollisionShape_t::pointer& shape) {
        ++fixture::created; auto result = std::make_shared<dbRigidBody_t>(); result->shape = shape; return result;
    }
    static bool add_rigid_body(const dbRigidBody_t::pointer&, const char*) { return true; }
    static void remove_rigid_body(const dbRigidBody_t::pointer&) { ++fixture::removed; }
};
void EchoWarning(const char*, const char*) {}
struct Name { const char* asChar() const { return "array"; } };
struct DBRigidBodyArrayNode {
    enum { ca_rigidBodies, ca_rigidBodyParam, ia_initialPosition, ia_collisionShape, ia_numBodies };
    std::vector<dbRigidBody_t::pointer> m_rigid_bodies;
    MMatrix m_worldMatrix;
    std::size_t m_numRigidBodies = 2;
    DBRigidBodyArrayNode() {
        for (int i = 0; i < 2; ++i) {
            auto body = std::make_shared<dbRigidBody_t>();
            body->implementation.set_transform(vec3f(float(i * 10 + 1), 2, 3), mvl::qidentity<float>());
            const btTransform interpolation = body->implementation.body()->getWorldTransform();
            body->implementation.set_transform(vec3f(float(i * 10), 0, 0), mvl::qidentity<float>());
            body->implementation.body()->setInterpolationWorldTransform(interpolation);
            m_rigid_bodies.push_back(body);
        }
    }
    MObject thisMObject() const { return {}; }
    Name name() const { return {}; }
    MStatus computeWorldMatrix(const MPlug&, MDataBlock&);
    MStatus computeRigidBodies(const MPlug&, MDataBlock&);
};

#include "bullet_array_methods.inc"

bool equalTransform(const btTransform& a, const btTransform& b) {
#ifdef DOMMY_NATIVE_BULLET
    return a.getOrigin() == b.getOrigin() && a.getBasis() == b.getBasis();
#else
    for (int i = 0; i < 3; ++i) if (a.position[i] != b.position[i]) return false;
    for (int i = 0; i < 4; ++i) if (a.rotation[i] != b.rotation[i]) return false;
    return true;
#endif
}
float bodyPosition(const dbRigidBody_t::pointer& body) {
    vec3f position; quatf rotation; body->implementation.get_transform(position, rotation); return position[0];
}
void assertTranslated(DBRigidBodyArrayNode& node) {
    assert(bodyPosition(node.m_rigid_bodies[0]) == 5 && bodyPosition(node.m_rigid_bodies[1]) == 15);
    assert(fixture::entries[0].value[0] == 105 && fixture::entries[1].value[0] == 205);
    assert(node.m_worldMatrix.translation == 5);
}

int main() {
    MPlug plug; MDataBlock data;
    fixture::reset(); DBRigidBodyArrayNode good;
    assert(good.computeWorldMatrix(plug, data)); assertTranslated(good);
    const int statusCount = fixture::checks, exceptionCount = fixture::throwingChecks;
    assert(good.computeWorldMatrix(plug, data)); assertTranslated(good);
    for (bool exception : {false, true}) {
        for (int failure = 1; failure <= (exception ? exceptionCount : statusCount); ++failure) {
            fixture::reset(); DBRigidBodyArrayNode node;
            const auto owners = node.m_rigid_bodies;
            std::array<btTransform, 2> world, interpolation;
            for (int i = 0; i < 2; ++i) {
                world[i] = owners[i]->implementation.body()->getWorldTransform();
                interpolation[i] = owners[i]->implementation.body()->getInterpolationWorldTransform();
            }
            if (exception) fixture::throwAt = failure; else fixture::failAt = failure;
            bool succeeded = false;
            try { succeeded = bool(node.computeWorldMatrix(plug, data)); }
            catch (const std::runtime_error&) { assert(exception); }
            assert(!succeeded && node.m_rigid_bodies == owners && node.m_worldMatrix.translation == 0);
            for (int i = 0; i < 2; ++i) {
                assert(equalTransform(world[i], owners[i]->implementation.body()->getWorldTransform()));
                assert(equalTransform(interpolation[i], owners[i]->implementation.body()->getInterpolationWorldTransform()));
                assert(fixture::entries[i].value[0] == float((i + 1) * 100));
            }
            fixture::failAt = fixture::throwAt = 0;
            assert(node.computeWorldMatrix(plug, data)); assertTranslated(node);
            assert(node.computeWorldMatrix(plug, data)); assertTranslated(node);
        }
    }
    for (int invalid = 0; invalid < 10; ++invalid) {
        fixture::reset(); DBRigidBodyArrayNode node;
        if (invalid == 0) fixture::entries[1].logical = 7;
        if (invalid == 1) fixture::entries.resize(1);
        if (invalid == 2) node.m_rigid_bodies[1].reset();
        if (invalid == 3) node.m_rigid_bodies[1]->missingImplementation = true;
        if (invalid == 4) fixture::entries[1].value[0] = std::numeric_limits<float>::infinity();
        if (invalid == 5) fixture::translation = std::numeric_limits<double>::infinity();
        if (invalid == 6) fixture::failLock = true;
        if (invalid == 7) node.m_rigid_bodies[1]->invalidRotation = true;
        if (invalid == 8) node.m_rigid_bodies[1]->invalidPosition = true;
        if (invalid == 9) node.m_rigid_bodies[1]->implementation.m_body.reset();
        assert(!node.computeWorldMatrix(plug, data));
        assert(bodyPosition(node.m_rigid_bodies[0]) == 0 && fixture::entries[0].value[0] == 100);
        assert(node.m_worldMatrix.translation == 0);
    }
    fixture::reset(); DBRigidBodyArrayNode sparse;
    fixture::entries = {{7, {700, 0, 0}}, {1, {200, 0, 0}}, {0, {100, 0, 0}}};
    assert(sparse.computeWorldMatrix(plug, data));
    assert(fixture::entries[0].value[0] == 700 && fixture::entries[1].value[0] == 205 && fixture::entries[2].value[0] == 105);
    fixture::reset(); DBRigidBodyArrayNode empty;
    empty.m_rigid_bodies.clear(); fixture::entries.clear();
    assert(empty.computeWorldMatrix(plug, data));
    fixture::reset(); DBRigidBodyArrayNode oversized;
    oversized.m_rigid_bodies.resize(dommy::generation::kMaxGeneratedTransformCount + 1U);
    assert(!oversized.computeWorldMatrix(plug, data));
    assert(bodyPosition(oversized.m_rigid_bodies[0]) == 0 && oversized.m_worldMatrix.translation == 0);

    for (int fault = 1; fault <= 13; ++fault) {
        fixture::reset(); fixture::connected = fault != 13; fixture::shapeFault = fault;
        auto shape = std::make_shared<dbCollisionShape_t>(); const auto previousShape = shape;
        assert(!dommy::bullet::resolveCollisionShapeInput(plug, shape));
        assert(shape == previousShape);
        DBRigidBodyArrayNode node; const auto previousBodies = node.m_rigid_bodies;
        assert(!node.computeRigidBodies(plug, data));
        assert(node.m_rigid_bodies == previousBodies && fixture::removed == 0);
        if (fault != 13) assert(fixture::defaultShapes == 0);
    }
    for (bool connected : {false, true}) {
        fixture::reset(); fixture::connected = connected; DBRigidBodyArrayNode node;
        assert(node.computeRigidBodies(plug, data));
        assert(fixture::defaultShapes == (connected ? 0 : 1) && fixture::removed == 2);
        assert(node.m_rigid_bodies[0]->shape->fallback == !connected);
    }
    for (int invalid = 0; invalid < 5; ++invalid) {
        fixture::reset(); DBRigidBodyArrayNode node;
        if (invalid == 0) node.m_rigid_bodies[1]->invalidPosition = true;
        if (invalid == 1) node.m_rigid_bodies[1]->invalidRotation = true;
        if (invalid == 2) node.m_rigid_bodies[1]->missingImplementation = true;
        if (invalid == 3) node.m_rigid_bodies[1].reset();
        if (invalid == 4) fixture::failLock = true;
        const auto previous = node.m_rigid_bodies;
        assert(!node.computeRigidBodies(plug, data));
        assert(node.m_rigid_bodies == previous && fixture::created == 0 && fixture::removed == 0);
        assert(bodyPosition(node.m_rigid_bodies[0]) == 0);
    }
    std::printf("Bullet array translation: %d status failures, %d exceptions/retries; sparse indices and 13 shape-input failures passed%s\n",
        statusCount, exceptionCount,
#ifdef DOMMY_NATIVE_BULLET
        " with native Bullet transforms"
#else
        " (host/native doubles)"
#endif
    );
}
