#pragma once

#include "DMayaEntryPointGuard.h"

#include <maya/MUIDrawManager.h>
#include <maya/MBoundingBox.h>
#include <maya/MPoint.h>

#include <exception>

namespace dommy {
namespace maya {

class ScopedUIDrawable final {
public:
    explicit ScopedUIDrawable(MHWRender::MUIDrawManager& drawManager)
        : drawManager_(drawManager)
    {
        drawManager_.beginDrawable();
    }

    ~ScopedUIDrawable() noexcept
    {
        try {
            drawManager_.endDrawable();
        } catch (...) {
        }
    }

private:
    ScopedUIDrawable(const ScopedUIDrawable&);
    ScopedUIDrawable& operator=(const ScopedUIDrawable&);
    MHWRender::MUIDrawManager& drawManager_;
};

inline void reportViewportCallbackException(
    const char* ownerName,
    const char* callbackName,
    const std::exception_ptr& exception) noexcept
{
    (void)reportEntryPointException(ownerName, callbackName, exception);
}

} // namespace maya
} // namespace dommy

#define DOMMY_VIEWPORT_VOID_CATCH(ownerName, callbackName) \
    catch (...) { \
        ::dommy::maya::reportViewportCallbackException( \
            ownerName, callbackName, std::current_exception()); \
    }

#define DOMMY_VIEWPORT_POINTER_CATCH(ownerName, callbackName) \
    catch (...) { \
        ::dommy::maya::reportViewportCallbackException( \
            ownerName, callbackName, std::current_exception()); \
        return NULL; \
    }

// Bounds callbacks cannot report MStatus. Keep exceptions inside the Maya
// boundary and return a small visible fallback instead of invalid coordinates.
#define DOMMY_VIEWPORT_BOUNDING_BOX_CATCH_WITH_HALF_EXTENT(ownerName, halfExtent) \
    catch (...) { \
        ::dommy::maya::reportViewportCallbackException( \
            ownerName, "boundingBox", std::current_exception()); \
        return MBoundingBox( \
            MPoint(-(halfExtent), -(halfExtent), -(halfExtent)), \
            MPoint((halfExtent), (halfExtent), (halfExtent))); \
    }

// Existing callers retain their original +/-0.5 extent. Locator-specific
// callers can preserve a different historical fallback without copying guards.
#define DOMMY_VIEWPORT_BOUNDING_BOX_CATCH(ownerName) \
    DOMMY_VIEWPORT_BOUNDING_BOX_CATCH_WITH_HALF_EXTENT(ownerName, 0.5)
