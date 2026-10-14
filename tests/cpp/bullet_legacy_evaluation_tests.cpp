// Execute complete production lifecycle/compute/output bodies with host and
// world doubles. This covers control flow, not Maya DG or Bullet physics.
#include "maya/MStatus.h"
#include "Solver/DBulletEvaluation.h"
#include "Solver/DBulletStep.h"
#include "Solver/DPassiveTransformMath.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

using dommy::bullet::BulletWorldMutationGuard;
using double3 = double[3];
namespace fixture {
int calls = 0, failAt = 0, steps = 0, initializations = 0;
int pointWrites = 0, clean = 0, resets = 0, errors = 0;
bool playback = false, disconnected = false, wrongTarget = false;
bool missingBody = false, missingNode = false, invalidPoint = false;
bool throwStep = false, hasBody = true, failClean = false, staleBody = false;
bool contactData = false, failContacts = false, failRigidOutput = false;
MStatus operation() { return ++calls == failAt ? MS::kFailure : MS::kSuccess; }
void reset() {
    calls = failAt = steps = initializations = pointWrites = clean = resets = errors = 0;
    playback = disconnected = wrongTarget = missingBody = missingNode = invalidPoint = throwStep = false;
    hasBody = true;
    failClean = false;
    staleBody = false;
    contactData = failContacts = failRigidOutput = false;
}
void resetCalls() { calls = failAt = clean = 0; }
}
enum { aEnabled = 1, aRigidBodies, aCurrentTime, aStartTime, aSubsteps,
       aFixedPhysicsRate, aSplitImpulse, aContactData, aGravity };
namespace MFn { enum Type { kMesh }; }
namespace MSpace { enum Space { kTransform }; }
struct MObject {
    int type;
    explicit MObject(int t = 0) : type(t) {}
    bool hasFn(MFn::Type) const { return type == 30; }
};
struct MTime {
    enum { kSeconds }; double t, unitScale;
    MTime(double v = 0, double scale = 1) : t(v), unitScale(scale) {}
    double value() const { return t; }
    double as(int) const { return t * unitScale / 60; }
    bool operator==(MTime rhs) const { return t * unitScale == rhs.t * rhs.unitScale; }
    bool operator!=(MTime rhs) const { return !(*this == rhs); }
    MTime operator-(MTime rhs) const { return MTime(t * unitScale - rhs.t * rhs.unitScale); }
};
struct MPlugArray;
struct MPlug {
    MObject object; int attribute;
    MPlug(int a = 0) : attribute(a) {}
    MPlug(MObject o, int a) : object(o), attribute(a) {}
    bool operator!=(int a) const { return attribute != a; }
    MObject node() const { return object; }
    MStatus getValue(MObject&) const { return fixture::operation(); }
    bool connectedTo(MPlugArray&, bool, bool, MStatus* = nullptr) const;
};
struct MPlugArray : std::vector<MPlug> {
    unsigned length() const { return static_cast<unsigned>(size()); }
};
bool MPlug::connectedTo(MPlugArray& array, bool, bool, MStatus* status) const {
    const MStatus result = fixture::operation();
    if (status) *status = result;
    if (!result) return false;
    if (attribute == aRigidBodies && fixture::hasBody)
        array.emplace_back(MObject(20), 0);
    if (attribute == 21 && !fixture::disconnected)
        array.emplace_back(MObject(fixture::wrongTarget ? 0 : 30), 0);
    return !array.empty();
}
struct MDataHandle {
    int attribute = 0, substeps = 1, rate = 60;
    bool enabled = true; double time = 0, start = 0;
    double startUnitScale = 1;
    bool asBool() const { return attribute == aEnabled ? enabled : attribute == aContactData && fixture::contactData; }
    int asInt() const { return attribute == aSubsteps ? substeps : rate; }
    MTime asTime() const { return attribute == aStartTime ? MTime(start, startUnitScale) : MTime(time); }
    const double3& asDouble3() const { static const double3 gravity = {0, 0, 0}; return gravity; }
    void set(bool) {}
};
struct MDataBlock {
    bool enabled = true; double time = 0, start = 0;
    double startUnitScale = 1;
    int substeps = 1, rate = 60;
    MDataHandle inputValue(int attribute, MStatus* status) {
        *status = fixture::operation();
        MDataHandle handle; handle.attribute = attribute; handle.enabled = enabled;
        handle.time = time; handle.start = start; handle.substeps = substeps; handle.rate = rate;
        handle.startUnitScale = startUnitScale;
        return handle;
    }
    MDataHandle outputValue(int attribute, MStatus* status) { return inputValue(attribute, status); }
    MStatus setClean(const MPlug&) {
        if (fixture::failClean) return MS::kFailure;
        const MStatus status = fixture::operation();
        if (status) ++fixture::clean;
        return status;
    }
};
struct MFnDagNode {
    MObject object;
    MFnDagNode(MObject value, MStatus* status) : object(value) { *status = fixture::operation(); }
    int typeId() const { return object.type; }
    unsigned parentCount(MStatus* status) const { *status = fixture::operation(); return 1; }
};
struct MFloatPointArray {
    MStatus append(float, float, float, float) { return fixture::operation(); }
};
struct MFnMesh {
    MFnMesh(MObject, MStatus* status) { *status = fixture::operation(); }
    MStatus setPoints(const MFloatPointArray&, MSpace::Space) {
        const MStatus status = fixture::operation();
        if (status) ++fixture::pointWrites;
        return status;
    }
};
struct dbSoftBody_t {
    using pointer = std::shared_ptr<dbSoftBody_t>;
    bool valid() const { return !fixture::staleBody; }
    void get_mesh(std::vector<int>&, std::vector<vec3f>& xyz) {
        xyz.push_back(vec3f(fixture::invalidPoint ? std::numeric_limits<float>::infinity() : 1, 2, 3));
    }
};
struct DBSoftBodyNode {
    enum { typeId = 20, outputMesh = 21 };
    dbSoftBody_t::pointer cached_soft_body() const {
        return fixture::missingBody ? dbSoftBody_t::pointer() : std::make_shared<dbSoftBody_t>();
    }
    MObject thisMObject() const { return MObject(); }
};
namespace dommy { namespace maya {
template<class T> T* checkedUserNode(const MFnDagNode&, int) {
    static T node; return fixture::missingNode ? nullptr : &node;
}
} }
struct dbSolver_t {
    using SoftBodyForces = std::vector<int>;
    static void destroyWorld() { ++fixture::resets; }
    static void createWorld() {}
    static void set_split_impulse(bool) {}
    static bool set_gravity(vec3f) { return bool(fixture::operation()); }
    static bool can_step_simulation(float, float, int) { return bool(fixture::operation()); }
    static bool step_simulation(float, float, const SoftBodyForces&) {
        if (!fixture::operation()) return false;
        ++fixture::steps;
        if (fixture::throwStep) throw std::runtime_error("native step failure");
        return true;
    }
};
struct MConditionMessage {
    static bool getConditionState(const char*, MStatus* status = nullptr) {
        const MStatus result = fixture::operation();
        if (status) *status = result;
        return fixture::playback;
    }
};
struct MGlobal { static void displayError(const char*) { ++fixture::errors; } };
struct DBSolverNode {
    struct xforms_t {};
    bool m_requiresStartFrame, m_startFrameErrorReported, m_reInitialize, m_ownsSharedWorld;
    MTime m_prevTime, m_initializedStartTime;
    std::atomic<int> m_debugDrawMode;
    static std::atomic<bool> s_isStartTime;
    static std::atomic<DBSolverNode*> s_activeSolver;
    struct ContactNodes { void clear() {} } m_contactNodes;
    DBSolverNode(); ~DBSolverNode();
    MObject thisMObject() const { return MObject(); }
    bool acquireSharedWorld();
    MStatus updateDebugDrawMode(MDataBlock&) { return fixture::operation(); }
    MStatus initRigidBodies(MPlugArray&, MDataBlock&) {
        const MStatus status = fixture::operation();
        if (status) ++fixture::initializations;
        return status;
    }
    MStatus clearContactRelatedAttributes(MPlugArray&) { return fixture::operation(); }
    MStatus gatherPassiveTransforms(MPlugArray&, std::vector<xforms_t>&) { return fixture::operation(); }
    MStatus updatePassiveRigidBodies(MPlugArray&, const std::vector<xforms_t>&, float) { return fixture::operation(); }
    MStatus applyFields(MPlugArray&, float, dbSolver_t::SoftBodyForces&) { return fixture::operation(); }
    MStatus updateActiveRigidBodies(MPlugArray&) { return fixture::failRigidOutput ? MS::kFailure : fixture::operation(); }
    MStatus refreshContactNodes(const MPlugArray&) { return fixture::operation(); }
    MStatus updateContactInfo(MPlugArray&) { return fixture::failContacts ? MS::kFailure : fixture::operation(); }
    MStatus updateActiveSoftBodies(MPlugArray&);
    MStatus computeRigidBodies(const MPlug&, MDataBlock&);
    MStatus compute(const MPlug&, MDataBlock&);
};
std::atomic<bool> DBSolverNode::s_isStartTime(false);
std::atomic<DBSolverNode*> DBSolverNode::s_activeSolver(nullptr);
#include "bullet_legacy_evaluation.inc"

static const MPlug output(aRigidBodies);
static void initialize(DBSolverNode& solver, MDataBlock& data) {
    data.enabled = true; data.time = data.start;
    assert(solver.compute(output, data));
    assert(!solver.m_requiresStartFrame && solver.m_prevTime.t == data.start);
    assert(solver.m_initializedStartTime.t == data.start);
}
int main() {
    for (bool contactFailure : {false, true}) {
        fixture::reset(); DBSolverNode solver; MDataBlock data;
        initialize(solver, data); data.time = 1;
        fixture::contactData = true;
        fixture::failContacts = contactFailure;
        fixture::failRigidOutput = !contactFailure;
        assert(!solver.compute(output, data));
        assert(fixture::steps == 1 && solver.m_requiresStartFrame && solver.m_prevTime.t == 0);
        fixture::failContacts = fixture::failRigidOutput = false;
        data.time = 2; assert(!solver.compute(output, data));
        initialize(solver, data);
    }
    // The host double models unit-aware MTime equality: equivalent physical
    // times are not settings edits merely because their numeric values differ.
    {
        fixture::reset(); DBSolverNode solver; MDataBlock data;
        data.start = 60; initialize(solver, data);
        data.time = 61; assert(solver.compute(output, data));
        data.start = 1; data.startUnitScale = 60;
        assert(solver.compute(output, data) && !solver.m_requiresStartFrame);
        assert(fixture::initializations == 1 && fixture::steps == 1);
        data.startUnitScale = 30;
        assert(!solver.compute(output, data) && solver.m_requiresStartFrame);
    }
    // Earlier/later StartTime edits, including while disabled or at a reused
    // frame, require a fresh initialization. Reverting an observed edit does
    // not clear the restart latch.
    for (double start : {-10., 10.}) for (bool disabled : {false, true})
    for (double current : {1., 2.}) {
        fixture::reset(); DBSolverNode solver; MDataBlock data;
        initialize(solver, data); data.time = 1; assert(solver.compute(output, data));
        const int steps = fixture::steps, initialized = fixture::initializations;
        data.start = start; data.time = current; data.enabled = !disabled;
        if (disabled) {
            assert(solver.compute(output, data));
            assert(solver.m_initializedStartTime.t == 0 && !solver.m_requiresStartFrame);
            data.enabled = true;
        }
        assert(!solver.compute(output, data) && solver.m_requiresStartFrame);
        assert(solver.m_initializedStartTime.t == 0 && solver.m_prevTime.t == 1);
        assert(fixture::steps == steps && fixture::initializations == initialized);
        data.start = 0; assert(!solver.compute(output, data));
        data.start = start; initialize(solver, data);
        data.time = start + 1; assert(solver.compute(output, data));
        assert(fixture::steps == steps + 1);
    }
    {
        fixture::reset(); DBSolverNode solver; MDataBlock data;
        initialize(solver, data); data.start = data.time = 10;
        fixture::failClean = true;
        assert(!solver.compute(output, data) && solver.m_requiresStartFrame);
        assert(solver.m_initializedStartTime.t == 0);
        fixture::failClean = false; data.time = 11;
        assert(!solver.compute(output, data));
        initialize(solver, data);
    }
    // Constructor readiness, disabled StartTime and disabled recovery cannot
    // authorize a native step. A successful enabled initialization is required.
    for (bool afterFailure : {false, true}) {
        fixture::reset(); DBSolverNode solver; MDataBlock data;
        assert(solver.m_requiresStartFrame && solver.m_reInitialize);
        if (afterFailure) {
            initialize(solver, data); data.time = 1; fixture::throwStep = true;
            assert(!solver.compute(output, data) && solver.m_requiresStartFrame);
            fixture::throwStep = false;
        }
        const int initialized = fixture::initializations, steps = fixture::steps;
        data.enabled = false; data.time = data.start;
        assert(solver.compute(output, data) && solver.m_requiresStartFrame);
        data.time = 5; assert(solver.compute(output, data) && solver.m_requiresStartFrame);
        assert(!DBSolverNode::s_isStartTime.load());
        data.enabled = true; data.time = 1;
        assert(!solver.compute(output, data));
        assert(fixture::steps == steps && fixture::initializations == initialized);
        initialize(solver, data); data.time = 1;
        assert(solver.compute(output, data) && fixture::steps == steps + 1);
    }
    // Invalid frame movement must latch restart even if the next request would
    // otherwise be adjacent to the last successful native frame.
    for (bool playback : {false, true}) for (double delta : {-1., .0, .5, 1., 2., 99., 100.}) {
        fixture::reset(); DBSolverNode solver; MDataBlock data;
        initialize(solver, data); data.time = 1; assert(solver.compute(output, data));
        data.time = 2; assert(solver.compute(output, data));
        fixture::playback = playback; fixture::resetCalls(); const int steps = fixture::steps;
        data.time = 2 + delta;
        const bool allowed = delta >= 0 && (delta <= 1 || (playback && delta < 100));
        assert(bool(solver.compute(output, data)) == allowed);
        assert(solver.m_requiresStartFrame == !allowed);
        assert(fixture::steps == steps + (allowed && delta > 0 ? 1 : 0));
        assert(solver.m_prevTime.t == (allowed ? data.time : 2));
        if (!allowed) {
            assert(fixture::clean == 0); data.time = 3;
            assert(!solver.compute(output, data)); initialize(solver, data);
        }
    }
    int failures = 0;
    // Exercise every observed host/native failure before and after mutation.
    for (bool advance : {false, true}) {
        int operations;
        {
            fixture::reset(); DBSolverNode solver; MDataBlock data;
            if (!advance) data.time = data.start = 7;
            if (advance) { initialize(solver, data); data.time = 1; }
            fixture::resetCalls(); assert(solver.compute(output, data)); operations = fixture::calls;
        }
        for (int failure = 1; failure <= operations; ++failure) {
            fixture::reset(); DBSolverNode solver; MDataBlock data;
            if (!advance) data.time = data.start = 7;
            if (advance) { initialize(solver, data); data.time = 1; }
            fixture::resetCalls(); fixture::failAt = failure;
            assert(!solver.compute(output, data)); ++failures;
            assert(solver.m_prevTime.t == 0 && fixture::clean == 0);
            assert(solver.m_initializedStartTime.t == 0);
            if (!advance || fixture::steps != 0) assert(solver.m_requiresStartFrame);
            fixture::resetCalls();
            if (solver.m_requiresStartFrame) {
                data.time = data.start + 1; assert(!solver.compute(output, data));
                initialize(solver, data);
            }
        }
    }
    // Output-only failures must also invalidate continuation after stepping.
    for (int failure = 0; failure < 5; ++failure) {
        fixture::reset(); DBSolverNode solver; MDataBlock data;
        initialize(solver, data); data.time = 1;
        fixture::missingNode = failure == 0; fixture::missingBody = failure == 1;
        fixture::wrongTarget = failure == 2; fixture::invalidPoint = failure == 3;
        fixture::staleBody = failure == 4;
        assert(!solver.compute(output, data) && solver.m_requiresStartFrame);
        assert(fixture::steps == 1 && fixture::pointWrites == 0 && solver.m_prevTime.t == 0);
    }
    {
        fixture::reset(); DBSolverNode solver; MDataBlock data;
        initialize(solver, data); fixture::disconnected = true; data.time = 1;
        assert(solver.compute(output, data) && !solver.m_requiresStartFrame);
        assert(fixture::pointWrites == 0 && fixture::steps == 1);
        fixture::disconnected = false; data.time = 2;
        assert(solver.compute(output, data) && fixture::pointWrites == 1);
    }
    {
        fixture::reset(); DBSolverNode solver; MDataBlock data;
        fixture::hasBody = false; initialize(solver, data); data.time = 1;
        assert(solver.compute(output, data));
        assert(fixture::pointWrites == 0 && fixture::steps == 1);
    }
    {
        fixture::reset(); DBSolverNode solver; MDataBlock data;
        initialize(solver, data); data.time = 1; assert(solver.compute(output, data));
        data.enabled = false; data.time = data.start;
        assert(solver.compute(output, data));
        data.time = 10; assert(solver.compute(output, data));
        assert(!solver.m_requiresStartFrame && solver.m_prevTime.t == 1);
        assert(fixture::steps == 1 && fixture::initializations == 1);
        data.enabled = true; data.time = 1; assert(solver.compute(output, data));
        assert(fixture::steps == 1); data.time = 2;
        assert(solver.compute(output, data) && fixture::steps == 2);
    }
    {
        fixture::reset(); MDataBlock data;
        std::unique_ptr<DBSolverNode> owner(new DBSolverNode);
        DBSolverNode successor;
        assert(!successor.compute(output, data)); owner.reset();
        data.time = 1; assert(!successor.compute(output, data));
        assert(successor.m_requiresStartFrame);
        initialize(successor, data);
    }
    std::printf("Legacy solver: readiness, disabled/recovery, timeline, ownership, output and %d injected failures passed\n", failures);
}
