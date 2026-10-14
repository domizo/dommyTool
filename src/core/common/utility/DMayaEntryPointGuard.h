#ifndef DOMMY_DMAYA_ENTRY_POINT_GUARD_H
#define DOMMY_DMAYA_ENTRY_POINT_GUARD_H

#include <exception>
#include <utility>

#include "DPointerFactoryGuard.h"

#include <maya/MGlobal.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

namespace dommy {
namespace maya {

inline MStatus reportEntryPointException(
    const char* ownerName,
    const char* entryPoint,
    const std::exception_ptr& exception) noexcept
{
    try {
        if (exception) {
            std::rethrow_exception(exception);
        }
        MGlobal::displayError(
            MString(ownerName) + ": unknown " + entryPoint + " error.");
    } catch (const std::exception& error) {
        try {
            MGlobal::displayError(
                MString(ownerName) + ": " + entryPoint + " error: " + error.what());
        } catch (...) {
        }
    } catch (...) {
        try {
            MGlobal::displayError(
                MString(ownerName) + ": unknown " + entryPoint + " error.");
        } catch (...) {
        }
    }
    return MS::kFailure;
}

template <typename Factory>
auto createObject(const char* ownerName, Factory&& factory) noexcept
    -> decltype(factory())
{
    return ::dommy::guardPointerFactory(std::forward<Factory>(factory),
        [ownerName](const std::exception_ptr& exception) noexcept {
            reportEntryPointException(ownerName, "creator", exception);
        });
}

} // namespace maya
} // namespace dommy

#endif // DOMMY_DMAYA_ENTRY_POINT_GUARD_H
