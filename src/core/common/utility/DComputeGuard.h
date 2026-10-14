#ifndef DOMMY_DCOMPUTE_GUARD_H
#define DOMMY_DCOMPUTE_GUARD_H

#include "DMayaEntryPointGuard.h"

namespace dommy {
namespace maya {

inline MStatus reportComputeException(
    const char* nodeName, const std::exception_ptr& exception) noexcept
{
    return reportEntryPointException(nodeName, "compute", exception);
}

}  // namespace maya
}  // namespace dommy

#define DOMMY_COMPUTE_CATCH(nodeName) \
    catch (...) { \
        return ::dommy::maya::reportComputeException( \
            nodeName, std::current_exception()); \
    }

#endif  // DOMMY_DCOMPUTE_GUARD_H
