// Actual registration transaction and solver callback bodies; no live Maya.
#include <cassert>
#include <cstdlib>
#include <functional>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>
#include "maya/MStatus.h"
static bool denyAllocation = false;
void* operator new(std::size_t size) {
    if (denyAllocation) throw std::bad_alloc();
    void* value = std::malloc(size ? size : 1);
    if (!value) throw std::bad_alloc();
    return value;
}
void operator delete(void* value) noexcept { std::free(value); }
#include "common/utility/DCommandSyntax.h"
using MTypeId = int;
using MCallbackId = int;
static int events[256], count = 0;
static void event(int value) { assert(count < 256); events[count++] = value; }
static int calls = 0, failAt = 0, throwAt = 0;
static bool allocateAfterSetup = false, zeroCallback = false, nonzeroFailedCallback = false;
static MStatus host(int code) {
    event(code);
    ++calls;
    if (calls == throwAt) throw std::runtime_error("host exception");
    if (calls == failAt) return MS::kFailure;
    return MS::kSuccess;
}
struct MFnPlugin {
    template <class... Args> MStatus registerNode(Args...) {
        MStatus status = host(1); if (allocateAfterSetup) denyAllocation = true; return status;
    }
    MStatus deregisterNode(int) { return host(-1); }
    template <class... Args> MStatus registerCommand(Args...) { return host(2); }
    MStatus deregisterCommand(MString) { return host(-2); }
    template <class... Args> MStatus registerData(Args...) { return host(3); }
    MStatus deregisterData(int) { return host(-3); }
};
struct MMessage {
    static MStatus removeCallback(int id) { return host(-id); }
};
namespace MHWRender { struct MDrawRegistry {
    template <class... Args> static MStatus registerGeometryOverrideCreator(Args...) { return host(4); }
    static MStatus deregisterGeometryOverrideCreator(MString, MString) { return host(-4); }
    template <class... Args> static MStatus registerDrawOverrideCreator(Args...) { return host(5); }
    static MStatus deregisterDrawOverrideCreator(MString, MString) { return host(-5); }
}; }
static MCallbackId callback(int id, MStatus* status) {
    *status = host(id);
    if (zeroCallback || (!*status && !nonzeroFailedCallback)) return 0;
    return id;
}
struct MDGMessage {
    template <class Function> static int addForceUpdateCallback(Function, void*, MStatus* status) {
        return callback(10, status);
    }
};
struct MSceneMessage {
    enum { kAfterOpen };
    template <class Function> static int addCallback(int, Function, void*, MStatus* status) {
        return callback(11, status);
    }
};
static void updateSceneCB() {}
static void sceneLoadedCB() {}
struct DBSolverNode { static MStatus registerCallbacks(); static MStatus removeCallbacks(); };
static int updateSceneCallbackId = 0, sceneLoadedCallbackId = 0;
#include "plugin_registration_methods.inc"

static void reset() {
    denyAllocation = false; calls = count = failAt = throwAt = 0;
    allocateAfterSetup = zeroCallback = nonzeroFailedCallback = false;
    assert(updateSceneCallbackId == 0 && sceneLoadedCallbackId == 0);
}
static MStatus setupAll(dommy::PluginRegistration& registration) {
    MStatus status = registration.registerNode("node", 1);
    if (!status) return status;
    status = registration.registerCommand(MString("command")); if (!status) return status;
    status = registration.registerData(MString("data"), 2); if (!status) return status;
    status = registration.registerGeometryOverrideCreator(MString(), MString(), nullptr); if (!status) return status;
    status = registration.registerDrawOverrideCreator(MString(), MString(), nullptr); if (!status) return status;
    status = registration.registerCallback([](MStatus& out) { return callback(6, &out); });
    return status ? registration.commit() : status;
}

int main() {
    MFnPlugin plugin;
    for (int point = 0; point <= 6; ++point) {
        reset(); failAt = point;
        dommy::PluginLifetime lifetime;
        { dommy::PluginRegistration registration(plugin, lifetime);
          MStatus status = setupAll(registration);
          assert((status == MS::kSuccess) == (point == 0));
          if (point) {
              const int previous = calls;
              assert(registration.registerNode("later", 99) == MS::kFailure);
              assert(calls == previous && registration.commit() == MS::kFailure);
          }
        }
        if (!point) assert(lifetime.cleanup(plugin) == MS::kSuccess);
        assert(lifetime.empty());
        const int created = point ? point - 1 : 6;
        const int registrations = point ? point : 6;
        assert(count == registrations + created);
        for (int i = 0; i < created; ++i) assert(events[registrations+i] == -(created-i));
    }
    // Bookkeeping allocation must fail before any host mutation.
    reset();
    { dommy::PluginLifetime lifetime; dommy::PluginRegistration registration(plugin,lifetime);
      denyAllocation = true;
      assert(registration.registerNode("node",1) == MS::kFailure);
      denyAllocation = false; assert(count == 0 && lifetime.empty()); }
    // Callback ownership allocation also happens before the callback factory.
    reset();
    { dommy::PluginLifetime lifetime;
      { dommy::PluginRegistration registration(plugin,lifetime);
        denyAllocation = true;
        bool caught = false;
        try { registration.registerCallback([](MStatus& out){return callback(6,&out);}); }
        catch (const std::bad_alloc&) { caught = true; }
        denyAllocation = false;
        assert(caught && count == 0 && lifetime.empty()); }
    }
    // No allocation is required after successful host setup, commit or cleanup.
    reset(); allocateAfterSetup = true;
    { dommy::PluginLifetime lifetime;
      { dommy::PluginRegistration registration(plugin,lifetime);
        assert(registration.registerNode("node",1) == MS::kSuccess);
        assert(registration.commit() == MS::kSuccess); }
      assert(lifetime.cleanup(plugin) == MS::kSuccess && lifetime.empty()); }
    denyAllocation = false;
    // A failed-load rollback must preserve a failing cleanup for later retry.
    reset(); int attempts = 0;
    { dommy::PluginLifetime lifetime;
      { dommy::PluginRegistration registration(plugin,lifetime);
        assert(registration.registerResource([](){return MStatus(MS::kFailure);},
            [&](MFnPlugin&){ return MStatus(++attempts == 1 ? MS::kFailure : MS::kSuccess); }) == MS::kFailure);
        assert(registration.commit() == MS::kFailure); }
      assert(attempts == 1 && !lifetime.empty());
      { dommy::PluginRegistration blocked(plugin,lifetime);
        assert(blocked.registerNode("node",1) == MS::kFailure); }
      assert(attempts == 1 && count == 0);
      assert(lifetime.cleanup(plugin) == MS::kSuccess && attempts == 2 && lifetime.empty()); }
    // Throwing setup retains cleanup because host mutation may be partial.
    reset(); throwAt = 1;
    { dommy::PluginLifetime lifetime;
      { dommy::PluginRegistration registration(plugin,lifetime);
        assert(registration.registerNode("node",1) == MS::kFailure); }
      assert(lifetime.empty() && count == 2 && events[1] == -1); }
    // Removal exceptions/status failures are retained; successful entries never repeat.
    for (bool throwing : {false,true}) {
        reset(); dommy::PluginLifetime lifetime;
        { dommy::PluginRegistration registration(plugin,lifetime); assert(setupAll(registration)); }
        if (throwing) throwAt = calls+2; else failAt = calls+2;
        denyAllocation = true;
        // Do not construct a throwing host exception while allocator is denied.
        if (throwing) denyAllocation = false;
        assert(lifetime.cleanup(plugin) == MS::kFailure && !lifetime.empty());
        assert(count == 12);
        failAt = throwAt = 0;
        assert(lifetime.cleanup(plugin) == MS::kSuccess && lifetime.empty());
        assert(count == 13 && events[12] == -5);
        denyAllocation = false;
    }
    // Callback zero-ID and nonzero-ID failure paths retain the right inverse.
    for (bool nonzero : {false,true}) {
        reset(); failAt=1; nonzeroFailedCallback=nonzero;
        { dommy::PluginLifetime lifetime; dommy::PluginRegistration registration(plugin,lifetime);
          assert(registration.registerCallback([](MStatus& out){return callback(6,&out);}) == MS::kFailure); }
        assert(count == (nonzero ? 2 : 1));
    }
    reset(); zeroCallback=true;
    { dommy::PluginLifetime lifetime; dommy::PluginRegistration registration(plugin,lifetime);
      assert(registration.registerCallback([](MStatus& out){return callback(6,&out);}) == MS::kFailure); }
    assert(count == 1);
    // Execute the actual solver callback group, including partial setup and retry.
    for (int point : {1,2}) {
        reset(); failAt=point; nonzeroFailedCallback=true;
        dommy::PluginLifetime lifetime;
        { dommy::PluginRegistration registration(plugin,lifetime);
          assert(registration.registerResource([](){return DBSolverNode::registerCallbacks();},
              [](MFnPlugin&){return DBSolverNode::removeCallbacks();}) == MS::kFailure);
          failAt=calls+1; }
        assert(!lifetime.empty() && updateSceneCallbackId == 10);
        assert(sceneLoadedCallbackId == 0);
        failAt=0;
        assert(lifetime.cleanup(plugin) == MS::kSuccess && lifetime.empty());
        assert(updateSceneCallbackId == 0 && sceneLoadedCallbackId == 0);
    }
    reset();
    assert(DBSolverNode::registerCallbacks() == MS::kSuccess);
    failAt=calls+1;
    assert(DBSolverNode::removeCallbacks() == MS::kFailure);
    assert(updateSceneCallbackId == 10 && sceneLoadedCallbackId == 0);
    failAt=0;
    assert(DBSolverNode::removeCallbacks() == MS::kSuccess);
}
