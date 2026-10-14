#ifndef DOMMY_DDEFORMER_GUARD_H
#define DOMMY_DDEFORMER_GUARD_H

#include "DMayaEntryPointGuard.h"

namespace dommy {
namespace maya {

inline MStatus reportDeformerException(
    const char* nodeName, const std::exception_ptr& exception) noexcept
{
    return reportEntryPointException(nodeName, "deform", exception);
}

} // namespace maya
} // namespace dommy

#define DOMMY_DEFORMER_CATCH(nodeName) \
    catch (...) { \
        return ::dommy::maya::reportDeformerException( \
            nodeName, std::current_exception()); \
    }

#endif // DOMMY_DDEFORMER_GUARD_H
