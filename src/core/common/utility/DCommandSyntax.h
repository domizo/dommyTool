#ifndef DOMMY_DCOMMAND_SYNTAX_H
#define DOMMY_DCOMMAND_SYNTAX_H

#include "DMayaEntryPointGuard.h"

#include <atomic>
#include <maya/MSyntax.h>

namespace dommy {
namespace maya {

template <typename Command>
std::atomic<bool>& commandSyntaxFailed() noexcept
{
    static std::atomic<bool> failed(false);
    return failed;
}

// A value-returning Maya syntax callback cannot return MStatus. Latch its
// failure and reject subsequent execution, rather than using partial syntax.
// Only a successful registration preflight resets this latch.
template <typename Command>
MStatus commandSyntaxStatus() noexcept
{
    return commandSyntaxFailed<Command>().load()
        ? MStatus(MS::kFailure) : MStatus(MS::kSuccess);
}

template <typename Command>
MStatus buildCommandSyntax(MSyntax& syntax, const char* ownerName) noexcept
{
    if (!commandSyntaxStatus<Command>()) return MS::kFailure;
    try {
        return Command::buildSyntax(syntax);
    } catch (...) {
        return reportEntryPointException(ownerName, "syntax", std::current_exception());
    }
}

template <typename Command>
MStatus validateCommandSyntax(const char* ownerName) noexcept
{
    try {
        MSyntax syntax;
        const MStatus status = Command::buildSyntax(syntax);
        commandSyntaxFailed<Command>().store(status != MS::kSuccess);
        return status;
    } catch (...) {
        commandSyntaxFailed<Command>().store(true);
        return reportEntryPointException(ownerName, "syntax", std::current_exception());
    }
}

inline void reportCommandSyntaxFailure(const char* ownerName) noexcept
{
    try {
        MGlobal::displayError(MString(ownerName) + ": syntax construction failed.");
    } catch (...) {
    }
}

template <typename Command>
void reportCommandSyntaxException(
    const char* ownerName, const std::exception_ptr& exception) noexcept
{
    commandSyntaxFailed<Command>().store(true);
    reportEntryPointException(ownerName, "syntax", exception);
}

template <typename Command>
MSyntax createCommandSyntax(const char* ownerName)
{
    try {
        MSyntax syntax;
        const MStatus status = buildCommandSyntax<Command>(syntax, ownerName);
        if (status == MS::kSuccess) return syntax;
        commandSyntaxFailed<Command>().store(true);
        reportCommandSyntaxFailure(ownerName);
    } catch (...) {
        reportCommandSyntaxException<Command>(ownerName, std::current_exception());
    }
    // Never return the partially built definition. MSyntax itself is a Maya
    // value type: constructing this empty fallback remains an SDK operation,
    // so it must not be declared noexcept (which would terminate the host).
    // The outer newSyntax function-try block also guards this fallback and
    // its own return-object copy. Persistent SDK value-construction/copy
    // failure cannot be converted into MStatus through this callback ABI.
    return MSyntax();
}

} // namespace maya
} // namespace dommy

#endif // DOMMY_DCOMMAND_SYNTAX_H
