#ifndef DOMMY_DCOMMAND_GUARD_H
#define DOMMY_DCOMMAND_GUARD_H

#include "DMayaEntryPointGuard.h"

namespace dommy {
namespace maya {

inline MStatus reportCommandException(
    const char* commandName, const std::exception_ptr& exception) noexcept
{
    return reportEntryPointException(commandName, "command", exception);
}

template <typename Operation>
MStatus invokeCommandOperation(
    const char* commandName, const char* phase, Operation&& operation) noexcept
{
    try {
        return std::forward<Operation>(operation)();
    } catch (...) {
        return reportEntryPointException(commandName, phase, std::current_exception());
    }
}

// The caller records attempted edits BEFORE invoking a mutating host API.
// Compensate both returned failures and exceptions, retaining a failed rollback
// as a failure rather than claiming that the original scene was restored.
// This cannot repair an API that mutates without returning a usable handle.
template <typename Operation, typename Rollback>
MStatus runCommandTransaction(
    const char* commandName, Operation&& operation, Rollback&& rollback) noexcept
{
    const MStatus status = invokeCommandOperation(
        commandName, "command", std::forward<Operation>(operation));
    if (status == MS::kSuccess) return status;

    const MStatus rollbackStatus = invokeCommandOperation(
        commandName, "rollback", std::forward<Rollback>(rollback));
    if (rollbackStatus != MS::kSuccess) {
        try {
            MGlobal::displayError(MString(commandName) + ": rollback failed; scene state may be incomplete.");
        } catch (...) {
        }
        return rollbackStatus;
    }
    return status;
}

} // namespace maya
} // namespace dommy

#define DOMMY_COMMAND_CATCH(commandName) \
    catch (...) { \
        return ::dommy::maya::reportCommandException( \
            commandName, std::current_exception()); \
    }

#endif // DOMMY_DCOMMAND_GUARD_H
