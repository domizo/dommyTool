#include <cassert>
#include <cstdlib>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>
#include "common/utility/DCommandSyntax.h"
#include "common/utility/DCommandGuard.h"
#include "common/utility/DStatusOperation.h"

namespace syntax_host {
int calls = 0, failAt = 0, throwAt = 0;
bool throwConstructor = false, throwUnknown = false;
int constructorCalls = 0, failConstructorAt = 0, copyCalls = 0, failCopyAt = 0;
bool persistentConstructorFailure = false, persistentCopyFailure = false;
}
static bool denyNextAllocation = false;
void* operator new(std::size_t size) {
    if (denyNextAllocation) { denyNextAllocation = false; throw std::bad_alloc(); }
    if (void* value = std::malloc(size ? size : 1)) return value;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept { std::free(value); }

namespace argument_host {
int calls = 0, failAt = 0, throwAt = 0, failCopyAt = -1;
bool exportMode = false, absentFlags = false, failReserve = false;
MStatus operation() {
    ++calls;
    if (calls == throwAt) throw std::bad_alloc();
    return calls == failAt ? MStatus(MS::kFailure) : MStatus(MS::kSuccess);
}
}
struct MArgList {
    unsigned int index = 0;
    unsigned int length() const { return 1; }
    MStatus get(unsigned int, MString& result) const {
        const MStatus status = argument_host::operation();
        if (!status) return status;
        result = MString((std::string(64, 'n') + std::to_string(index)).c_str());
        if (static_cast<int>(index) == argument_host::failCopyAt) denyNextAllocation = true;
        return MS::kSuccess;
    }
};
struct MArgDatabase {
    MArgDatabase(const MSyntax&, const MArgList&, MStatus* status = nullptr) {
        const MStatus binding = argument_host::operation();
        if (status) *status = binding;
    }
    bool isFlagSet(const char* flag) const {
        if (argument_host::absentFlags) return false;
        return !argument_host::exportMode || std::string(flag) == "-f";
    }
    MStatus getFlagArgument(const char*, int, double& result) const {
        const MStatus status = argument_host::operation();
        if (status) result = 2.5;
        return status;
    }
    MStatus getFlagArgument(const char*, int, MString& result) const {
        const MStatus status = argument_host::operation();
        if (status) result = MString(argument_host::exportMode ? "/safe/export.txt" : "curveA");
        return status;
    }
    unsigned int numberOfFlagUses(const char*) const {
        if (argument_host::failReserve) denyNextAllocation = true;
        return argument_host::absentFlags ? 0 : 3;
    }
    MStatus getFlagArgumentList(const char*, unsigned int index, MArgList& list) const {
        const MStatus status = argument_host::operation();
        if (status) list.index = index;
        return status;
    }
};
using MTypeId = int;
using MCallbackId = int;
static int registered = 0, removed = 0;
static bool failNativeSyntax = false;
struct MFnPlugin {
    template <typename Creator, typename Syntax>
    MStatus registerCommand(const MString&, Creator, Syntax syntax) {
        ++registered;
        if (failNativeSyntax) {
            syntax_host::failAt = syntax_host::calls + 1;
            (void)syntax();
        }
        return MS::kSuccess;
    }
    MStatus deregisterCommand(const MString&) { ++removed; return MS::kSuccess; }
    template <typename... Args> MStatus registerNode(Args...) { return MS::kSuccess; }
    MStatus deregisterNode(int) { return MS::kSuccess; }
    template <typename... Args> MStatus registerData(Args...) { return MS::kSuccess; }
    MStatus deregisterData(int) { return MS::kSuccess; }
};
struct MMessage { static MStatus removeCallback(int) { return MS::kSuccess; } };
namespace MHWRender { struct MDrawRegistry {
    template <typename... Args> static MStatus registerGeometryOverrideCreator(Args...) { return MS::kSuccess; }
    static MStatus deregisterGeometryOverrideCreator(MString, MString) { return MS::kSuccess; }
    template <typename... Args> static MStatus registerDrawOverrideCreator(Args...) { return MS::kSuccess; }
    static MStatus deregisterDrawOverrideCreator(MString, MString) { return MS::kSuccess; }
}; }
namespace dommy { class PluginRegistration; }
struct SyntaxCase {
    const char* name;
    std::vector<std::string> expected;
    MStatus (*build)(MSyntax&);
    MSyntax (*create)();
    MStatus (*validate)();
    MStatus (*status)();
    MStatus (*registerCommand)(dommy::PluginRegistration&);
};
#include "command_syntax_methods.inc"

static void resetSyntax() {
    syntax_host::calls = syntax_host::failAt = syntax_host::throwAt = 0;
    syntax_host::throwConstructor = syntax_host::throwUnknown = false;
    syntax_host::constructorCalls = syntax_host::failConstructorAt = 0;
    syntax_host::copyCalls = syntax_host::failCopyAt = 0;
    syntax_host::persistentConstructorFailure = syntax_host::persistentCopyFailure = false;
    registered = removed = 0;
    failNativeSyntax = false;
}
static void resetArguments(bool exporter = false) {
    argument_host::calls = argument_host::failAt = argument_host::throwAt = 0;
    argument_host::failCopyAt = -1;
    argument_host::exportMode = exporter;
    argument_host::absentFlags = argument_host::failReserve = false;
    denyNextAllocation = false;
}
template <typename Command>
MStatus runParse(Command& command) {
    try { return command.parseArgs(MArgList()); }
    DOMMY_COMMAND_CATCH("parser fixture")
}

int main() {
    MFnPlugin plugin;
    for (const SyntaxCase& item : syntaxCases) {
        resetSyntax(); assert(item.validate());
        resetSyntax(); MSyntax baseline; assert(item.build(baseline));
        assert(baseline.operations == item.expected);
        const int count = syntax_host::calls;
        for (int index = 1; index <= count; ++index) {
            resetSyntax(); syntax_host::failAt = index;
            MSyntax partial; assert(!item.build(partial));
            assert(syntax_host::calls == index);
            resetSyntax(); syntax_host::throwAt = index;
            MSyntax throwing; assert(!item.build(throwing));
            assert(syntax_host::calls == index);
            for (bool exception : {false, true}) {
                resetSyntax(); assert(item.validate()); resetSyntax();
                if (exception) syntax_host::throwAt = index;
                else syntax_host::failAt = index;
                const MSyntax empty = item.create();
                assert(empty.operations.empty() && !item.status());
                const int previous = syntax_host::calls;
                MSyntax blocked; assert(!item.build(blocked));
                assert(syntax_host::calls == previous);
                resetSyntax(); assert(item.validate());
            }
            resetSyntax(); syntax_host::failAt = index;
            dommy::PluginLifetime lifetime;
            { dommy::PluginRegistration registration(plugin, lifetime);
              assert(!item.registerCommand(registration));
              assert(!registration.commit() && registered == 0); }
            assert(lifetime.empty() && removed == 0);
            resetSyntax(); assert(item.validate());
        }
        resetSyntax(); syntax_host::throwConstructor = true;
        assert(!item.validate() && !item.status());
        resetSyntax(); assert(item.validate());
        resetSyntax(); syntax_host::throwAt = 1; syntax_host::throwUnknown = true;
        assert(item.create().operations.empty() && !item.status());
        resetSyntax(); assert(item.validate());
        // Disable copy elision in this fixture to exercise both plug-in return
        // copies and the caller-owned receive copy, rather than hiding them.
        resetSyntax();
        const MSyntax returned = item.create();
        assert(returned.operations == item.expected);
        const int returnCopies = syntax_host::copyCalls;
        assert(returnCopies == 3);
        for (int point = 1; point <= returnCopies; ++point) {
            resetSyntax(); assert(item.validate()); resetSyntax();
            syntax_host::failCopyAt = point;
            bool escaped = false;
            try {
                const MSyntax recovery = item.create();
                assert(recovery.operations.empty());
            } catch (const std::bad_alloc&) {
                escaped = true;
            }
            if (point < returnCopies) {
                assert(!escaped && !item.status());
            } else {
                // The caller's receive copy is outside the plug-in callback.
                assert(escaped && item.status());
            }
        }
        resetSyntax(); assert(item.validate()); resetSyntax();
        syntax_host::failAt = 1;
        syntax_host::failConstructorAt = 2;
        assert(item.create().operations.empty() && !item.status());
        assert(syntax_host::constructorCalls == 3);
        for (bool copyFailure : {false, true}) {
            resetSyntax(); assert(item.validate()); resetSyntax();
            syntax_host::persistentCopyFailure = copyFailure;
            syntax_host::persistentConstructorFailure = !copyFailure;
            bool escaped = false;
            try { (void)item.create(); }
            catch (const std::bad_alloc&) { escaped = true; }
            // Documented ABI limit: no viable MSyntax value can be returned.
            // Do not terminate; preserve the failure latch if the host recovers.
            assert(escaped && !item.status());
            assert(copyFailure ? syntax_host::copyCalls == 3
                               : syntax_host::constructorCalls == 3);
        }
        resetSyntax(); assert(item.validate());
        resetSyntax(); failNativeSyntax = true;
        dommy::PluginLifetime lifetime;
        { dommy::PluginRegistration registration(plugin, lifetime);
          assert(!item.registerCommand(registration));
          assert(registered == 1 && !lifetime.empty()); }
        assert(removed == 1 && lifetime.empty());
        resetSyntax(); assert(item.validate()); resetSyntax();
        { dommy::PluginRegistration registration(plugin, lifetime);
          assert(item.registerCommand(registration) && registration.commit()); }
        assert(registered == 1 && removed == 0 && !lifetime.empty());
        assert(lifetime.cleanup(plugin) && removed == 1 && lifetime.empty());
    }

    CurveCommand curve;
    assert(dommy::maya::validateCommandSyntax<CurveCommand>("curve"));
    for (bool exception : {false, true}) {
        for (int index = 1; index <= 3; ++index) {
            resetSyntax(); resetArguments();
            curve.curve = MString("oldCurve"); curve.lengthPos = 9;
            if (exception) argument_host::throwAt = index;
            else argument_host::failAt = index;
            assert(!runParse(curve));
            assert(curve.curve == MString("oldCurve") && curve.lengthPos == 9);
            assert(argument_host::calls == index);
        }
    }
    resetSyntax(); resetArguments(); assert(runParse(curve));
    assert(curve.curve == MString("curveA") && curve.lengthPos == 2.5);
    resetSyntax(); resetArguments(); argument_host::absentFlags = true;
    assert(runParse(curve) && curve.curve.length() == 0 && curve.lengthPos == 0);

    ExportCommand exporter;
    assert(dommy::maya::validateCommandSyntax<ExportCommand>("export"));
    for (bool exception : {false, true}) {
        for (int index = 1; index <= 8; ++index) {
            resetSyntax(); resetArguments(true);
            exporter.filePath = MString("oldPath"); exporter.savedNodeNames = {MString("oldNode")};
            if (exception) argument_host::throwAt = index;
            else argument_host::failAt = index;
            assert(!runParse(exporter));
            assert(exporter.filePath == MString("oldPath"));
            assert(exporter.savedNodeNames.size() == 1 && exporter.savedNodeNames[0] == MString("oldNode"));
            assert(argument_host::calls == index);
        }
    }
    for (int point = -1; point <= 2; ++point) {
        resetSyntax(); resetArguments(true);
        exporter.filePath = MString("oldPath"); exporter.savedNodeNames = {MString("oldNode")};
        if (point == -1) argument_host::failReserve = true;
        else argument_host::failCopyAt = point;
        assert(!runParse(exporter) && !denyNextAllocation);
        assert(exporter.filePath == MString("oldPath"));
        assert(exporter.savedNodeNames.size() == 1 && exporter.savedNodeNames[0] == MString("oldNode"));
    }
    resetSyntax(); resetArguments(true); assert(runParse(exporter));
    assert(exporter.filePath == MString("/safe/export.txt") && exporter.savedNodeNames.size() == 3);
    for (unsigned int index = 0; index < 3; ++index) {
        assert(exporter.savedNodeNames[index] == MString((std::string(64, 'n') + std::to_string(index)).c_str()));
    }
    // A failed host syntax callback must block the real parsers before binding.
    resetSyntax(); syntax_host::failAt = 1; (void)CurveCommand::newSyntax();
    resetArguments(); assert(!runParse(curve) && argument_host::calls == 0);
    resetSyntax(); syntax_host::failAt = 1; (void)ExportCommand::newSyntax();
    resetArguments(true); assert(!runParse(exporter) && argument_host::calls == 0);
}
