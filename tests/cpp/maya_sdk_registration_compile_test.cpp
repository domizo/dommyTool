#include "registration/PluginRegistration.h"

namespace {
void* createObject() {
    return 0;
}

MStatus initializeObject() {
    return MS::kSuccess;
}
} // namespace

void instantiateRegistrationApi(MFnPlugin& plugin) {
    dommy::PluginLifetime lifetime;
    dommy::PluginRegistration registration(plugin, lifetime);
    registration.registerNode(MString("dommyCompileTestNode"), MTypeId(0x0012ff01), createObject,
                              initializeObject);
    registration.registerCommand(MString("dommyCompileTestCommand"), createObject);
    registration.registerData(MString("dommyCompileTestData"), MTypeId(0x0012ff02), createObject);
    registration.registerCallback([](MStatus& status) {
        status = MS::kSuccess;
        return MCallbackId(1);
    });
}
